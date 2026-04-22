## [Unreleased] - 2026-04-21

### Added

* **LuaBridge control surface** (`libs/surfaces/luabridge/`) — new Ardour control surface that
  exposes a WebSocket server on port 9871, allowing external processes to execute Lua commands
  inside a live Ardour session. Comprises `luabridge_surface.cc/.h`, `server.cc/.h`, and
  `interface.cc`.

* **WebView2 chat panel** (`gtk2_ardour/chat_webview.cc/.h`) — embeds an in-DAW chat UI using
  the WebView2 runtime on Windows (MinGW build), serving `share/chat_ui/` over a local HTTP
  relay. Enables direct AI-assistant interaction from inside the LabDAWg window.

* **Chat UI frontend** (`share/chat_ui/`) — self-contained HTML/CSS/JS web app that connects to
  the LuaBridge WebSocket relay and renders the AI chat interface inside the WebView2 panel.

* **Agent install target** (`share/agent/`) — Waf build target that installs a pre-built
  Agent_1.3 binary bundle when present (built separately via `tools/package-agent.sh`).

* **LabDAWg themes** (`gtk2_ardour/themes/*-labdawg.colors`) — twelve custom color themes
  (dark, nightfall, cubasish, blueberry milk, and others) tailored to the LabDAWg visual identity.

* **LabDAWg icons and branding assets** (`gtk2_ardour/icons/`, `gtk2_ardour/resources/`) —
  application icons at 16 px through 512 px, splash screen, and small-splash variants.

* `BUILD_AND_RUN.md` — build and run instructions specific to this fork.

### Modified

* `gtk2_ardour/editor.cc` / `editor.h` — wired `ChatWebView` widget into the main editor
  window; added show/hide toggle and layout integration.

* `gtk2_ardour/about.cc` — updated application name, credits, and GPL compliance notice to
  reflect LabDAWg identity.

* `gtk2_ardour/wscript` — added `chat_webview.cc` to the GTK2 Ardour build sources.

* `wscript` (root) — added `share/chat_ui` and `share/agent` subdirectories to the top-level
  Waf build.

* `gtk2_ardour/ardour-mime-info.xml`, `ardour.appdata.xml.in.in`, `ardour.desktop.in`,
  `gtk2_ardour/win32/msvc_resources.rc.in` — rebranded application name, description, and
  metadata from Ardour to LabDAWg.

### Notes

* This repository is a fork of [Ardour](https://ardour.org), an open-source DAW licensed under
  GPL-2.0. All original Ardour source files retain their original copyright and license headers.
  Only files listed above were added or modified by Apollo AI Lab, Inc.

* The companion AI agent (`Agent_1.3/`) is a separate Python application that communicates with
  this fork exclusively via the LuaBridge WebSocket interface. It is not compiled into or linked
  with the Ardour binary and is maintained in a separate repository.
