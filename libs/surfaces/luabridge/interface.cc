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

#include "ardour/rc_configuration.h"
#include "control_protocol/control_protocol.h"

#include "luabridge_surface.h"

using namespace ARDOUR;
using namespace ArdourSurface;

static ControlProtocol*
new_luabridge_protocol (Session* s)
{
	LuaBridgeSurface* surface = new LuaBridgeSurface (*s);

	surface->set_active (true);

	return surface;
}

static void
delete_luabridge_protocol (ControlProtocol* cp)
{
	delete cp;
}

static ControlProtocolDescriptor luabridge_descriptor = {
	/* name       */ luabridge_surface_name,
	/* id         */ luabridge_surface_id,
	/* module     */ 0,
	/* available  */ 0,
	/* probe_port */ 0,
	/* match usb  */ 0,
	/* initialize */ new_luabridge_protocol,
	/* destroy    */ delete_luabridge_protocol,
};

extern "C" ARDOURSURFACE_API ControlProtocolDescriptor*
protocol_descriptor ()
{
	return &luabridge_descriptor;
}
