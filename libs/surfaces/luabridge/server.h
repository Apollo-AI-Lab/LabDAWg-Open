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

#ifndef _ardour_surface_luabridge_server_h_
#define _ardour_surface_luabridge_server_h_

#include <atomic>
#include <deque>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <glib.h>
#include <libwebsockets.h>

#if LWS_LIBRARY_VERSION_MAJOR < 3
#undef HZ
#endif

#define LUABRIDGE_LISTEN_PORT 9871

namespace ArdourSurface {

class LuaBridgeSurface;

typedef struct lws* LBClient;

/* Per-connection state: output message queue. */
struct LBClientContext {
	LBClientContext (LBClient w) : _wsi (w) {}
	LBClient wsi () const { return _wsi; }
	std::deque<std::string>& output_buf () { return _output; }
private:
	LBClient _wsi;
	std::deque<std::string> _output;
};

/* A command received from a WebSocket client.
 * Enqueued on the background lws thread, dispatched on the main GLib thread
 * so Lua execution never touches the network layer. */
struct PendingCommand {
	LBClient    wsi;
	std::string type;   // "ping" | "state_query" | "lua_exec" | "lua_eval"
	std::string id;     // echoed back in every response
	std::string code;   // Lua source (lua_exec / lua_eval only)
};

class LuaBridgeServer
{
public:
	LuaBridgeServer (LuaBridgeSurface&);
	virtual ~LuaBridgeServer () {}

	int start ();
	int stop ();

	void send_to_client (LBClient, const std::string& json);
	void send_to_all    (const std::string& json);

private:
	LuaBridgeSurface& _surface;

	struct lws_protocols             _lws_proto[2];
	struct lws_context_creation_info _lws_info;
	struct lws_context*              _lws_context;

	/* Client map and output buffers — protected by _client_mutex.
	 * Accessed from both the lws background thread and the main thread. */
	typedef std::unordered_map<LBClient, LBClientContext> ClientContextMap;
	ClientContextMap _client_ctx;
	std::mutex       _client_mutex;

	int  add_client   (LBClient);
	int  del_client   (LBClient);
	int  recv_client  (LBClient, void*, size_t);
	int  write_client (LBClient);        // called on lws thread
	void request_write (LBClient);

	/* ── Background lws service thread ───────────────────────────────────
	 *
	 * lws_service() runs here exclusively — it blocks on poll() until
	 * network events arrive, consuming zero CPU when the client is idle.
	 * lws_cancel_service() is used to wake this thread when the main
	 * thread has queued a response to send.
	 */
	std::thread       _lws_thread;
	std::atomic<bool> _stopping { false };

	void lws_service_loop ();

	/* ── Command queue: background thread → main GLib thread ─────────────
	 *
	 * When a message arrives on the lws thread, it is parsed and pushed
	 * to _command_queue, then g_idle_add() is called ONCE (guarded by
	 * _dispatch_pending) to schedule dispatch_idle_cb() on the main thread.
	 *
	 * The main thread drains the queue, executes Lua (safe here), and
	 * calls send_to_client() which wakes the lws thread via lws_cancel_service()
	 * to flush the response.
	 */
	std::queue<PendingCommand> _command_queue;
	std::mutex                 _queue_mutex;
	std::atomic<bool>          _dispatch_pending { false };

	static gboolean dispatch_idle_cb          (void* data);  // runs on main thread
	void            dispatch_pending_commands ();
	void            handle_command            (const PendingCommand&);

	/* ── libwebsockets static callback ───────────────────────────────── */
	static int lws_callback (struct lws*, enum lws_callback_reasons,
	                         void*, void*, size_t);

	/* ── JSON helpers ─────────────────────────────────────────────────── */
	std::string json_escape     (const std::string&);
	std::string json_get_string (const std::string& json, const std::string& key);

	std::string build_response (const std::string& id, bool success,
	                            const std::string& output,
	                            const std::vector<std::string>& prints,
	                            const std::string& error);

	std::string build_state_response (const std::string& id);
};

} // namespace ArdourSurface

#endif // _ardour_surface_luabridge_server_h_
