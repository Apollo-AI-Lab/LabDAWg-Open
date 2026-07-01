## [1.1] - 2026-06-30

### Added

* **LabDAWg theme** (`gtk2_ardour/themes/labdawg-labdawg.colors`) — thirteenth custom color
  theme named after the product itself, added to the LabDAWg theme collection.  
  *Apollo AI Lab, Inc. — 2026-06-03*

### Removed

* **WebView2 chat panel** (`gtk2_ardour/chat_webview.cc/.h`) — removed the Windows-only
  in-process WebView2 chat panel. Chat interaction is now handled externally by the Python
  agent launcher via `ardour_ui_access_web.cc`.  
  *Apollo AI Lab, Inc. — 2026-06-06*

### Modified

* `gtk2_ardour/editor.cc` / `editor.h` — removed `ChatWebView` widget references following
  the removal of the in-process chat panel.  
  *Apollo AI Lab, Inc. — 2026-06-06*

* `gtk2_ardour/wscript` — removed `chat_webview.cc` from the GTK2 Ardour build sources.  
  *Apollo AI Lab, Inc. — 2026-06-06*

* `gtk2_ardour/bundle_env_mingw.cc` — set `PANGOCAIRO_BACKEND=fc` to enable the fontconfig
  backend for correct custom font rendering on Windows (MinGW build).  
  *Apollo AI Lab, Inc. — 2026-06-06*

* `gtk2_ardour/midi_region_view.cc` — re-raise region name highlight and label above the MIDI
  note group on height change so the region label is always drawn in front of MIDI notes.  
  *Apollo AI Lab, Inc. — 2026-06-06*

* `gtk2_ardour/ardour_ui_access_web.cc` — added macOS app bundle launcher path for the Python
  agent, resolving the bundled Python interpreter under `Contents/Resources/` when running from
  a packaged `.app`.  
  *Apollo AI Lab, Inc. — 2026-06-26*

* `gtk2_ardour/bundle_env_cocoa.cc` — registered custom UI fonts (`Manrope`, `SpaceGrotesk`)
  with both fontconfig and CoreText on macOS, ensuring they are visible to the bundled Pango
  font backend and the dev-tree CoreText backend respectively.  
  *Apollo AI Lab, Inc. — 2026-06-26*

### Notes

* Merged upstream Ardour 9 source changes into this fork. No Apollo AI Lab, Inc. modifications
  were altered by this merge; all changes listed above and in prior entries remain intact.  
  *Apollo AI Lab, Inc. — 2026-06-02*

---

## [1.0] - 2026-04-21

### Added

* **LuaBridge control surface** (`libs/surfaces/luabridge/`) — new Ardour control surface that
  exposes a WebSocket server on port 9871, allowing external processes to execute Lua commands
  inside a live Ardour session. Comprises `luabridge_surface.cc/.h`, `server.cc/.h`, and
  `interface.cc`.  
  *Apollo AI Lab, Inc. — 2026-03-02*

* **MCP HTTP control surface** (`libs/surfaces/mcp_http/`) — new Ardour control surface that
  exposes Ardour session control as an MCP server over HTTP on port 4820. Supports session and
  transport control, track and bus management, plugin manipulation, send routing, marker and
  range management, region editing, and bulk MIDI note import/export. Comprises
  `mcp_http.cc/.h`, `mcp_http_server.cc/.h`, `mcp_http_gui.cc/.h`, `interface.cc`, and
  `tools_json.inc`.  
  *Apollo AI Lab, Inc. — 2026-03-22*

* **WebView2 chat panel** (`gtk2_ardour/chat_webview.cc/.h`) — embedded an in-DAW chat UI
  using the WebView2 runtime on Windows (MinGW build), serving `share/chat_ui/` over a local
  HTTP relay. (Removed in 1.1.)  
  *Apollo AI Lab, Inc. — 2026-03-16*

* **Chat UI frontend** (`share/chat_ui/`) — self-contained HTML/CSS/JS web app that connects
  to the LuaBridge WebSocket relay and renders the AI chat interface inside the WebView2 panel.  
  *Apollo AI Lab, Inc. — 2026-03-16*

* **Agent install target** (`share/agent/`) — Waf build target that installs a pre-built
  agent binary bundle when present.  
  *Apollo AI Lab, Inc. — 2026-03-16*

* **LabDAWg themes** (`gtk2_ardour/themes/*-labdawg.colors`) — twelve custom color themes
  (dark, nightfall, cubasish, blueberry milk, and others) tailored to the LabDAWg visual
  identity.  
  *Apollo AI Lab, Inc. — 2026-03-17*

* **LabDAWg icons and branding assets** (`gtk2_ardour/icons/`, `gtk2_ardour/resources/`) —
  application icons at 16 px through 512 px, splash screen, and small-splash variants.  
  *Apollo AI Lab, Inc. — 2026-03-17*

* `BUILD_AND_RUN.md` — build and run instructions specific to this fork.  
  *Apollo AI Lab, Inc. — 2026-03-17*

### Modified

* `gtk2_ardour/editor.cc` / `editor.h` — wired `ChatWebView` widget into the main editor
  window; added show/hide toggle and layout integration.  
  *Apollo AI Lab, Inc. — 2026-03-16*

* `gtk2_ardour/about.cc` — updated application name, credits, and GPL compliance notice to
  reflect LabDAWg identity.  
  *Apollo AI Lab, Inc. — 2026-03-17*

* `gtk2_ardour/wscript` — added `chat_webview.cc` to the GTK2 Ardour build sources.  
  *Apollo AI Lab, Inc. — 2026-03-16*

* `wscript` (root) — added `share/chat_ui` and `share/agent` subdirectories to the top-level
  Waf build.  
  *Apollo AI Lab, Inc. — 2026-03-17*

* `gtk2_ardour/ardour-mime-info.xml`, `ardour.appdata.xml.in.in`, `ardour.desktop.in`,
  `gtk2_ardour/win32/msvc_resources.rc.in` — rebranded application name, description, and
  metadata from Ardour to LabDAWg.  
  *Apollo AI Lab, Inc. — 2026-03-17*

### Notes

* This repository is a fork of [Ardour](https://ardour.org), an open-source DAW licensed under
  GPL-2.0. All original Ardour source files retain their original copyright and license headers.
  Only files listed above were added or modified by Apollo AI Lab, Inc.

* The companion AI agent is a separate Python application that communicates with this fork
  exclusively via the LuaBridge WebSocket interface and the MCP HTTP surface. It is not
  compiled into or linked with the Ardour binary and is maintained in a separate repository.
