# LabDAWg

LabDAWg is a fork of [Ardour](https://ardour.org) — a professional, open-source digital audio
workstation — extended with an AI assistant interface.

---

## License / Modifications

This project is based on **Ardour**, licensed under the **GNU General Public License, version 2
(GPL-2.0)**. A full copy of the license is included in the `COPYING` file.

Modifications made by **Apollo AI Lab, Inc.** include:

* A new LuaBridge WebSocket control surface (`libs/surfaces/luabridge/`) that allows an external
  AI agent to execute Lua commands inside a live Ardour session.
* A new MCP HTTP control surface (`libs/surfaces/mcp_http/`) that exposes Ardour session control
  as an MCP server over HTTP, allowing any MCP-compatible AI client to control a live session.
* A bundled chat UI frontend (`share/chat_ui/`) for in-DAW AI assistant interaction.
* Modifications to `gtk2_ardour/ardour_ui_access_web.cc` to launch the Python AI agent from
  within the Ardour process on Windows and macOS.
* Modifications to `gtk2_ardour/bundle_env_cocoa.cc` and `bundle_env_mingw.cc` for custom font
  registration and rendering on macOS and Windows respectively.
* A minor fix to `gtk2_ardour/midi_region_view.cc` for MIDI region label rendering.
* LabDAWg branding: application name, icons, splash screen, and custom color themes.
* Minor modifications to `gtk2_ardour/editor.cc/.h`, `about.cc`, build scripts, and platform
  metadata files to integrate the above.

All modified and newly added files are marked with a notice in their header. Original Ardour
files are unmodified and retain their original copyright headers.

See `CHANGELOG.md` for a detailed record of all changes from upstream.

---

## Building

See `BUILD_AND_RUN.md` for platform-specific build instructions.

For general Ardour build documentation, see the [Ardour build guide](https://ardour.org/development.html).

---

## Upstream

Ardour source: https://github.com/Ardour/ardour
LabDAWg fork: https://github.com/Apollo-AI-Lab/dawgs
