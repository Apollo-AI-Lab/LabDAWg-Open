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
* An in-DAW WebView2 chat panel (`gtk2_ardour/chat_webview.cc/.h`) for AI assistant interaction
  on Windows.
* A bundled chat UI frontend (`share/chat_ui/`) served inside the WebView2 panel.
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
