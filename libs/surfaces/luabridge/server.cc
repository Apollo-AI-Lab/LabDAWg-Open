/*
 * Copyright (C) 2026 Apollo AI Lab, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <iomanip>
#include <sstream>
#include <exception>

#include "pbd/compose.h"

#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/track.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/plugin_insert.h"
#include "ardour/plugin.h"

#include "luabridge_surface.h"
#include "server.h"

#define LWS_LIBRARY_VERSION_NUM \
	((LWS_LIBRARY_VERSION_MAJOR * 1000000) + (LWS_LIBRARY_VERSION_MINOR * 1000))

using namespace ArdourSurface;

/* ── Constructor ─────────────────────────────────────────────────────────── */

LuaBridgeServer::LuaBridgeServer (LuaBridgeSurface& surface)
    : _surface (surface)
    , _lws_context (nullptr)
{
	lws_protocols proto;
	memset (&proto, 0, sizeof (lws_protocols));
	proto.name           = "lws-luabridge";
	proto.callback       = LuaBridgeServer::lws_callback;
	proto.rx_buffer_size = 65536;
	proto.id             = 0;
	proto.user           = 0;
#if LWS_LIBRARY_VERSION_MAJOR >= 3
	proto.tx_packet_size = 0;
#endif
	_lws_proto[0] = proto;
	memset (&_lws_proto[1], 0, sizeof (lws_protocols));

	memset (&_lws_info, 0, sizeof (lws_context_creation_info));
	_lws_info.port      = LUABRIDGE_LISTEN_PORT;
	_lws_info.protocols = _lws_proto;
	_lws_info.mounts    = nullptr;
	_lws_info.uid       = -1;
	_lws_info.gid       = -1;
	_lws_info.user      = this;
	_lws_info.options   = 0;   /* no GLib integration — we own the service thread */
}

/* ── Lifecycle ───────────────────────────────────────────────────────────── */

int
LuaBridgeServer::start ()
{
	/* Suppress libwebsockets' default verbose output.
	 *
	 * The guard was previously `#ifndef NDEBUG`, which meant release builds
	 * (where NDEBUG IS defined) never called lws_set_log_level and left lws
	 * in its default all-levels mode:
	 *   ERR | WARN | NOTICE | INFO | DEBUG | PARSER | HEADER | EXT | ...
	 *
	 * With a client connected that fires dozens of writes per second —
	 * every poll() wake-up, every WebSocket frame header, every HTTP parser
	 * event — each one allocating a formatted timestamp string and writing
	 * to stderr.  The result is a continuous CPU and memory drain.
	 *
	 * Fix: always restrict lws logging.  In release builds allow errors
	 * only; in debug builds also allow warnings to catch handshake issues. */
#ifdef NDEBUG
	lws_set_log_level (LLL_ERR, nullptr);
#else
	lws_set_log_level (LLL_ERR | LLL_WARN, nullptr);
#endif

	if (_lws_context) {
		stop ();
	}

	_lws_context = lws_create_context (&_lws_info);
	if (!_lws_context) {
		PBD::error << "LuaBridge: could not create libwebsockets context" << endmsg;
		return -1;
	}

	_stopping.store (false);
	_lws_thread = std::thread (&LuaBridgeServer::lws_service_loop, this);

	PBD::info << "LuaBridge: WebSocket server listening on port "
	          << std::dec << LUABRIDGE_LISTEN_PORT << endmsg;
	return 0;
}

int
LuaBridgeServer::stop ()
{
	/* Signal the service thread to exit, then wake it from poll(). */
	_stopping.store (true);
	if (_lws_context) {
		lws_cancel_service (_lws_context);
	}

	if (_lws_thread.joinable ()) {
		_lws_thread.join ();
	}

	if (_lws_context) {
		lws_context_destroy (_lws_context);
		_lws_context = nullptr;
	}

	return 0;
}

/* ── Background lws service thread ──────────────────────────────────────────
 *
 * lws_service() blocks inside poll() until a network event arrives — zero
 * CPU when idle.  lws_cancel_service() wakes this thread whenever the main
 * thread queues an outgoing message, so responses are flushed immediately.
 */

void
LuaBridgeServer::lws_service_loop ()
{
	while (!_stopping.load ()) {
		/* 250 ms timeout: lws_service() blocks in poll() until a network
		 * event arrives, so this only affects clean-shutdown latency, not
		 * responsiveness (lws_cancel_service() wakes us immediately when
		 * there is something to send).  250 ms halves the loop iterations
		 * vs. the original 50 ms with no functional difference. */
		lws_service (_lws_context, 250);
	}
}

/* ── Client management (called on lws thread) ────────────────────────────── */

int
LuaBridgeServer::add_client (LBClient wsi)
{
	std::lock_guard<std::mutex> lock (_client_mutex);
	_client_ctx.emplace (wsi, LBClientContext (wsi));
	PBD::info << "LuaBridge: client connected" << endmsg;
	return 0;
}

int
LuaBridgeServer::del_client (LBClient wsi)
{
	std::lock_guard<std::mutex> lock (_client_mutex);
	_client_ctx.erase (wsi);
	PBD::info << "LuaBridge: client disconnected" << endmsg;
	return 0;
}

/* ── Receive path: lws thread → command queue → main thread ──────────────── */

int
LuaBridgeServer::recv_client (LBClient wsi, void* buf, size_t len)
{
	std::string msg (static_cast<char*> (buf), len);

	PendingCommand cmd;
	cmd.wsi  = wsi;
	cmd.type = json_get_string (msg, "type");
	cmd.id   = json_get_string (msg, "id");
	cmd.code = json_get_string (msg, "code");

	/* Push to the shared queue under a brief lock. */
	{
		std::lock_guard<std::mutex> lock (_queue_mutex);
		_command_queue.push (cmd);
	}

	/* Schedule a one-shot idle callback on the main GLib thread.
	 * The atomic CAS ensures g_idle_add() is called at most once per drain
	 * cycle — even if several messages arrive before dispatch runs.
	 *
	 * G_PRIORITY_HIGH_IDLE (100) instead of the g_idle_add() default
	 * G_PRIORITY_DEFAULT_IDLE (200): Ardour schedules low-priority UI work
	 * (redraws, deferred signal emissions) at 200.  At that level, a busy
	 * GUI can push our Lua dispatch callback dozens of frames into the
	 * future, causing multi-second g_idle_add latency before Lua even
	 * starts — the primary reason plugin_add appears to "time out" even
	 * when the Lua itself runs quickly.  HIGH_IDLE fires before those
	 * low-priority tasks while still yielding to real GTK events. */
	bool expected = false;
	if (_dispatch_pending.compare_exchange_strong (expected, true)) {
		g_idle_add_full (G_PRIORITY_HIGH_IDLE, dispatch_idle_cb, this, nullptr);
	}

	return 0;
}

/* ── Main-thread dispatch ────────────────────────────────────────────────── */

/* static — called by GLib main loop when idle */
gboolean
LuaBridgeServer::dispatch_idle_cb (void* data)
{
	LuaBridgeServer* self = static_cast<LuaBridgeServer*> (data);

	/* Reset the flag BEFORE draining so that messages arriving during
	 * dispatch schedule a fresh g_idle_add rather than being silently lost. */
	self->_dispatch_pending.store (false);

	self->dispatch_pending_commands ();

	return G_SOURCE_REMOVE;  /* one-shot: do not re-register automatically */
}

void
LuaBridgeServer::dispatch_pending_commands ()
{
	/* Swap the entire shared queue into a local queue under a brief lock,
	 * then process without holding it — Lua execution can take tens of ms
	 * and must not stall the lws recv thread. */
	std::queue<PendingCommand> local;
	{
		std::lock_guard<std::mutex> lock (_queue_mutex);
		std::swap (local, _command_queue);
	}

	while (!local.empty ()) {
		handle_command (local.front ());
		local.pop ();
	}
}

void
LuaBridgeServer::handle_command (const PendingCommand& cmd)
{
	try {
		if (cmd.type == "ping") {
			send_to_client (cmd.wsi,
			    "{\"id\":\"" + json_escape (cmd.id) + "\",\"type\":\"pong\"}");
			return;
		}

		if (cmd.type == "state_query") {
			send_to_client (cmd.wsi, build_state_response (cmd.id));
			return;
		}

		if (cmd.type == "lua_exec" || cmd.type == "lua_eval") {
			if (cmd.code.empty ()) {
				std::vector<std::string> empty;
				send_to_client (cmd.wsi,
				    build_response (cmd.id, false, "", empty,
				                    "No 'code' field in request"));
				return;
			}

			std::string code = cmd.code;

			/* lua_eval: auto-wrap bare expressions so the caller gets a value back. */
			if (cmd.type == "lua_eval" && code.find ("return") == std::string::npos) {
				code = "return tostring(" + code + ")";
			}

			LuaBridgeSurface::LuaResult result = _surface.execute_lua (code);
			send_to_client (cmd.wsi,
			    build_response (cmd.id, result.success, result.output,
			                    result.prints, result.error));
			return;
		}

		/* Unknown message type */
		std::vector<std::string> empty;
		send_to_client (cmd.wsi,
		    build_response (cmd.id, false, "", empty,
		                    "Unknown message type: " + cmd.type));
	} catch (std::exception const& e) {
		PBD::error << "LuaBridge: handle_command exception | type=" << cmd.type
		           << " | id=" << cmd.id << " | error=" << e.what () << endmsg;
		std::vector<std::string> empty;
		send_to_client (cmd.wsi,
		    build_response (cmd.id, false, "", empty,
		                    std::string ("Bridge exception: ") + e.what ()));
	} catch (...) {
		PBD::error << "LuaBridge: handle_command unknown exception | type=" << cmd.type
		           << " | id=" << cmd.id << endmsg;
		std::vector<std::string> empty;
		send_to_client (cmd.wsi,
		    build_response (cmd.id, false, "", empty,
		                    "Bridge exception: unknown error"));
	}
}

/* ── Send path: main thread → lws thread ────────────────────────────────── */

void
LuaBridgeServer::send_to_client (LBClient wsi, const std::string& json)
{
	/* Append to the client's output buffer under the lock, then release
	 * before calling into lws so we never hold the lock during a callback. */
	{
		std::lock_guard<std::mutex> lock (_client_mutex);
		ClientContextMap::iterator  it = _client_ctx.find (wsi);
		if (it == _client_ctx.end ()) {
			return;
		}
		it->second.output_buf ().push_back (json);
	}

	/* lws_callback_on_writable() and lws_cancel_service() are both
	 * thread-safe since libwebsockets 3.x. */
	lws_callback_on_writable (wsi);
	if (_lws_context) {
		lws_cancel_service (_lws_context);
	}
}

void
LuaBridgeServer::send_to_all (const std::string& json)
{
	std::vector<LBClient> clients;

	{
		std::lock_guard<std::mutex> lock (_client_mutex);
		for (ClientContextMap::iterator it = _client_ctx.begin ();
		     it != _client_ctx.end (); ++it) {
			it->second.output_buf ().push_back (json);
			clients.push_back (it->second.wsi ());
		}
	}

	for (LBClient wsi : clients) {
		lws_callback_on_writable (wsi);
	}
	if (_lws_context && !clients.empty ()) {
		lws_cancel_service (_lws_context);
	}
}

void
LuaBridgeServer::request_write (LBClient wsi)
{
	lws_callback_on_writable (wsi);
	if (_lws_context) {
		lws_cancel_service (_lws_context);
	}
}

/* ── Write path (called on lws thread by SERVER_WRITEABLE callback) ──────── */

int
LuaBridgeServer::write_client (LBClient wsi)
{
	std::string msg;
	bool        has_more = false;

	/* Dequeue one message under the lock; release before calling lws_write()
	 * since lws may invoke callbacks re-entrantly on some platforms. */
	{
		std::lock_guard<std::mutex> lock (_client_mutex);
		ClientContextMap::iterator  it = _client_ctx.find (wsi);
		if (it == _client_ctx.end ()) {
			return 1;
		}
		std::deque<std::string>& pending = it->second.output_buf ();
		if (pending.empty ()) {
			return 0;
		}
		msg      = pending.front ();
		has_more = pending.size () > 1;
		pending.pop_front ();
	}

	size_t                     msg_len = msg.size ();
	std::vector<unsigned char> out_buf (LWS_PRE + msg_len);
	memcpy (&out_buf[LWS_PRE], msg.c_str (), msg_len);

	int written = lws_write (wsi, &out_buf[LWS_PRE], msg_len, LWS_WRITE_TEXT);
	if (written < 0 || static_cast<size_t> (written) != msg_len) {
		return 1;
	}

	/* Schedule the next write callback if more messages are queued. */
	if (has_more) {
		lws_callback_on_writable (wsi);
	}

	return 0;
}

/* ── libwebsockets callback ──────────────────────────────────────────────── */

/* static */
int
LuaBridgeServer::lws_callback (struct lws*                wsi,
                                enum lws_callback_reasons  reason,
                                void*                      user,
                                void*                      in,
                                size_t                     len)
{
	void*            ctx_userdata = lws_context_user (lws_get_context (wsi));
	LuaBridgeServer* server       = static_cast<LuaBridgeServer*> (ctx_userdata);
	int              rc           = 0;

	switch (reason) {
		case LWS_CALLBACK_ESTABLISHED:
			rc = server->add_client (wsi);
			break;

		case LWS_CALLBACK_CLOSED:
			rc = server->del_client (wsi);
			break;

		case LWS_CALLBACK_RECEIVE:
			rc = server->recv_client (wsi, in, len);
			break;

		case LWS_CALLBACK_SERVER_WRITEABLE:
			rc = server->write_client (wsi);
			break;

#if LWS_LIBRARY_VERSION_NUM >= 2001000
		default:
			rc = lws_callback_http_dummy (wsi, reason, user, in, len);
			break;
#else
		default:
			rc = 0;
			break;
#endif
	}

	return rc;
}

/* ── JSON helpers ────────────────────────────────────────────────────────── */

std::string
LuaBridgeServer::json_escape (const std::string& s)
{
	std::ostringstream o;
	for (char c : s) {
		if (c == '"' || c == '\\') {
			o << '\\' << c;
		} else if (c == '\n') {
			o << "\\n";
		} else if (c == '\r') {
			o << "\\r";
		} else if (c == '\t') {
			o << "\\t";
		} else if ('\x00' <= c && c <= '\x1f') {
			o << "\\u" << std::hex << std::setw (4)
			  << std::setfill ('0') << static_cast<int> (c);
		} else {
			o << c;
		}
	}
	return o.str ();
}

std::string
LuaBridgeServer::json_get_string (const std::string& json, const std::string& key)
{
	std::string search = "\"" + key + "\"";
	size_t      pos    = json.find (search);
	if (pos == std::string::npos) {
		return "";
	}

	pos += search.size ();

	while (pos < json.size () &&
	       (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t' ||
	        json[pos] == '\n' || json[pos] == '\r')) {
		pos++;
	}

	if (pos >= json.size () || json[pos] != '"') {
		return "";
	}
	pos++; /* skip opening quote */

	std::string result;
	while (pos < json.size () && json[pos] != '"') {
		if (json[pos] == '\\' && pos + 1 < json.size ()) {
			pos++;
			switch (json[pos]) {
				case '"':  result += '"';  break;
				case '\\': result += '\\'; break;
				case 'n':  result += '\n'; break;
				case 'r':  result += '\r'; break;
				case 't':  result += '\t'; break;
				default:   result += json[pos]; break;
			}
		} else {
			result += json[pos];
		}
		pos++;
	}
	return result;
}

std::string
LuaBridgeServer::build_response (const std::string&              id,
                                  bool                            success,
                                  const std::string&              output,
                                  const std::vector<std::string>& prints,
                                  const std::string&              error)
{
	std::ostringstream json;
	json << "{";
	json << "\"id\":\""     << json_escape (id)     << "\",";
	json << "\"type\":\"result\",";
	json << "\"success\":"  << (success ? "true" : "false") << ",";
	json << "\"output\":\"" << json_escape (output)  << "\",";
	json << "\"prints\":[";
	for (size_t i = 0; i < prints.size (); i++) {
		if (i > 0) json << ",";
		json << "\"" << json_escape (prints[i]) << "\"";
	}
	json << "]";
	if (!error.empty ()) {
		json << ",\"error\":\"" << json_escape (error) << "\"";
	}
	json << "}";
	return json.str ();
}

std::string
LuaBridgeServer::build_state_response (const std::string& id)
{
	ARDOUR::Session& sess = _surface.ardour_session ();

	std::ostringstream json;
	json << "{";
	json << "\"id\":\""    << json_escape (id)        << "\",";
	json << "\"type\":\"state\",";
	json << "\"data\":{";
	json << "\"session_name\":\"" << json_escape (sess.name ())          << "\",";
	json << "\"sample_rate\":"    << sess.nominal_sample_rate ()          << ",";

	json << "\"transport\":{";
	json << "\"rolling\":"         << (sess.transport_rolling ()  ? "true" : "false") << ",";
	json << "\"speed\":"           << sess.actual_speed ()                            << ",";
	json << "\"record_enabled\":"  << (sess.get_record_enabled () ? "true" : "false") << ",";
	json << "\"sample_position\":" << sess.transport_sample ();
	json << "},";

	json << "\"tracks\":[";
	std::shared_ptr<ARDOUR::RouteList const> routes = sess.get_routes ();
	bool first = true;
	for (auto const& r : *routes) {
		if (!first) json << ",";
		first = false;

		json << "{";
		json << "\"name\":\""  << json_escape (r->name ())             << "\",";
		json << "\"id\":"      << r->presentation_info ().order ()      << ",";
		json << "\"muted\":"   << (r->muted ()  ? "true" : "false")    << ",";
		json << "\"soloed\":"  << (r->soloed () ? "true" : "false")    << ",";

		std::shared_ptr<ARDOUR::Track>     track = std::dynamic_pointer_cast<ARDOUR::Track> (r);
		std::shared_ptr<ARDOUR::MidiTrack> mt    = std::dynamic_pointer_cast<ARDOUR::MidiTrack> (r);

		if (track) {
			json << "\"type\":\""      << (mt ? "midi" : "audio") << "\",";
			json << "\"rec_enabled\":" << (track->rec_enable_control ()->get_value () > 0 ? "true" : "false") << ",";
		} else {
			json << "\"type\":\"bus\",";
			json << "\"rec_enabled\":false,";
		}

		std::shared_ptr<ARDOUR::GainControl> gc = r->gain_control ();
		json << "\"gain_db\":" << (gc ? accurate_coefficient_to_dB (gc->get_value ()) : 0.0);

		json << ",\"plugins\":[";
		bool     first_p = true;
		uint32_t pi_idx  = 0;
		for (;;) {
			std::shared_ptr<ARDOUR::Processor>    proc = r->nth_plugin (pi_idx);
			if (!proc) break;
			std::shared_ptr<ARDOUR::PluginInsert> pi   = std::dynamic_pointer_cast<ARDOUR::PluginInsert> (proc);
			if (pi) {
				if (!first_p) json << ",";
				first_p = false;
				std::shared_ptr<ARDOUR::Plugin> plugin = pi->plugin ();
				json << "{";
				json << "\"name\":\""      << json_escape (plugin->name ())      << "\",";
				json << "\"unique_id\":\"" << json_escape (plugin->unique_id ()) << "\",";
				json << "\"enabled\":"     << (pi->enabled () ? "true" : "false") << ",";
				json << "\"index\":"       << pi_idx;
				json << "}";
			}
			pi_idx++;
		}
		json << "]";
		json << "}";
	}
	json << "]}}";
	return json.str ();
}
