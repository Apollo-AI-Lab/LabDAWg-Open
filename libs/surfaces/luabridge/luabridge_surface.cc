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

#include "ardour/session.h"
#include "ardour/luabindings.h"

#include "pbd/error.h"

#include "LuaBridge/LuaBridge.h"

#include "luabridge_surface.h"

using namespace ARDOUR;
using namespace ArdourSurface;

#include "pbd/abstract_ui.inc.cc" // instantiate template

namespace {

std::string
preview_lua_code (const std::string& code)
{
	std::string preview = code;
	for (char& c : preview) {
		if (c == '\n' || c == '\r' || c == '\t') {
			c = ' ';
		}
	}
	const size_t max_len = 160;
	if (preview.size () > max_len) {
		preview = preview.substr (0, max_len) + "...";
	}
	return preview;
}

}

LuaBridgeSurface::LuaBridgeSurface (Session& s)
    : ControlProtocol (s, X_ (luabridge_surface_name))
    , AbstractUI<LuaBridgeUIRequest> (name ())
    , _server (*this)
    , _lua (0)
{
}

LuaBridgeSurface::~LuaBridgeSurface ()
{
	stop ();
}

int
LuaBridgeSurface::set_active (bool yn)
{
	if (yn != active ()) {
		if (yn) {
			if (start ()) {
				return -1;
			}
		} else {
			if (stop ()) {
				return -1;
			}
		}
	}

	return ControlProtocol::set_active (yn);
}

void
LuaBridgeSurface::thread_init ()
{
	PBD::notify_event_loops_about_thread_creation (pthread_self (), event_loop_name (), 2048);
	SessionEvent::create_per_thread_pool (event_loop_name (), 128);
}

void
LuaBridgeSurface::do_request (LuaBridgeUIRequest* req)
{
	if (req->type == CallSlot) {
		call_slot (MISSING_INVALIDATOR, req->the_slot);
	} else if (req->type == Quit) {
		stop ();
	}
}

void
LuaBridgeSurface::init_lua_state ()
{
	delete _lua;
	_lua = new LuaState (true, false);
	_lua->Print.connect (sigc::mem_fun (*this, &LuaBridgeSurface::on_lua_print));

	lua_State* L = _lua->getState ();

	/* Register Ardour Lua bindings */
	LuaBindings::stddef (L);
	LuaBindings::common (L);
	LuaBindings::non_rt (L);
	LuaBindings::osc (L);

	/* Bind the Session global */
	LuaBindings::set_session (L, session);

	/* Define a no-op ardour() function so scripts with ardour{} headers work */
	_lua->do_command ("function ardour () end");

	PBD::info << "LuaBridge: Lua state initialized" << endmsg;
}

void
LuaBridgeSurface::on_lua_print (std::string s)
{
	_pending_prints.push_back (s);
}

LuaBridgeSurface::LuaResult
LuaBridgeSurface::execute_lua (const std::string& code)
{
	LuaResult result;
	_pending_prints.clear ();

	PBD::info << "LuaBridge: execute_lua begin | chars=" << code.size ()
	          << " | preview=" << preview_lua_code (code) << endmsg;

	if (!_lua) {
		result.success = false;
		result.error = "Lua state not initialized";
		PBD::error << "LuaBridge: execute_lua aborted | Lua state not initialized" << endmsg;
		return result;
	}

	try {
		int rc = _lua->do_command (code);
		if (rc == 0) {
			result.success = true;
			result.output = "OK";
			PBD::info << "LuaBridge: execute_lua success" << endmsg;
		} else {
			result.success = false;
			result.error = "Lua execution failed with code " + std::to_string (rc);
			PBD::warning << "LuaBridge: execute_lua failed | rc=" << rc << endmsg;
		}
	} catch (luabridge::LuaException const& e) {
		result.success = false;
		result.error = std::string ("LuaException: ") + e.what ();
		PBD::warning << "LuaBridge: execute_lua LuaException | " << e.what () << endmsg;
	} catch (std::exception const& e) {
		result.success = false;
		result.error = std::string ("C++ Exception: ") + e.what ();
		PBD::error << "LuaBridge: execute_lua std::exception | " << e.what () << endmsg;
	} catch (...) {
		result.success = false;
		result.error = "Unknown exception during Lua execution";
		PBD::error << "LuaBridge: execute_lua unknown exception" << endmsg;
	}

	result.prints = _pending_prints;
	_pending_prints.clear ();

	/* Do NOT force a full GC cycle here.
	 *
	 * The original `_lua->collect_garbage()` call ran a complete Lua
	 * mark-and-sweep after every single Lua execution.  During the agent's
	 * agentic loop (3–12 consecutive tool calls per user message), that
	 * triggered repeated full GC cycles back-to-back.  With the Ardour
	 * non-rt bindings loaded (hundreds of registered C++ classes as Lua
	 * userdata), each cycle walks a large object graph — causing the CPU
	 * spikes the user observed.  The plugin-discovery call that returns
	 * 200+ PluginInfo userdata objects made this especially bad.
	 *
	 * Lua's built-in incremental GC manages collection automatically using
	 * a heap-growth threshold.  There is no need to force collection after
	 * every call.  Removing this lets the GC amortize its work across many
	 * executions instead of spiking on each one. */

	return result;
}

int
LuaBridgeSurface::start ()
{
	/* startup the event loop thread */
	BaseUI::run ();

	init_lua_state ();

	int rc = _server.start ();
	if (rc != 0) {
		BaseUI::quit ();
		return -1;
	}

	PBD::info << "LuaBridge: started" << endmsg;

	return 0;
}

int
LuaBridgeSurface::stop ()
{
	_server.stop ();

	delete _lua;
	_lua = 0;

	BaseUI::quit ();

	PBD::info << "LuaBridge: stopped" << endmsg;

	return 0;
}
