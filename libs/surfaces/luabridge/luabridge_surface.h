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

#ifndef _ardour_surface_luabridge_h_
#define _ardour_surface_luabridge_h_

#define ABSTRACT_UI_EXPORTS
#include "pbd/abstract_ui.h"

#include "control_protocol/control_protocol.h"

#include "lua/luastate.h"

#include "server.h"

namespace ArdourSurface {

const char * const luabridge_surface_name = "LuaBridge (AI Agent)";
const char * const luabridge_surface_id = "uri://ardour.org/surfaces/ardour_luabridge:0";

struct LuaBridgeUIRequest : public BaseUI::BaseRequestObject {
public:
	LuaBridgeUIRequest () {}
	~LuaBridgeUIRequest () {}
};

class LuaBridgeSurface : public ARDOUR::ControlProtocol,
                         public AbstractUI<LuaBridgeUIRequest>
{
public:
	LuaBridgeSurface (ARDOUR::Session&);
	virtual ~LuaBridgeSurface ();

	int set_active (bool);

	ARDOUR::Session& ardour_session () { return *session; }

	LuaBridgeServer& server_component () { return _server; }

	/* Execute Lua code and return result */
	struct LuaResult {
		bool success;
		std::string output;
		std::vector<std::string> prints;
		std::string error;
	};

	LuaResult execute_lua (const std::string& code);

	/* ControlProtocol */
	void stripable_selection_changed () {}

	CONTROL_PROTOCOL_THREADS_NEED_TEMPO_MAP_DECL();

protected:
	/* BaseUI */
	void thread_init ();

	/* AbstractUI */
	void do_request (LuaBridgeUIRequest*);

private:
	LuaBridgeServer _server;
	LuaState*       _lua;

	void init_lua_state ();

	int start ();
	int stop ();

	/* print capture */
	std::vector<std::string> _pending_prints;
	void on_lua_print (std::string);
};

} // namespace ArdourSurface

#endif // _ardour_surface_luabridge_h_
