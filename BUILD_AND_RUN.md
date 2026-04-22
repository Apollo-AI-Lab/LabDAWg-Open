# LabDAWg — Build & Run Guide

## Prerequisites

### MSYS2 / MinGW64
All build commands run in an **MSYS2 MinGW64** terminal.
Install from https://www.msys2.org/ if not already present.

### Python (Windows native)
The AI agent requires **Windows Python 3.10+** (not the MSYS2 Python).
Install from https://www.python.org/ — the default `C:/Python314/python.exe` (or similar) works.

### WebView2 Runtime
Required for the embedded chat panel. Usually pre-installed on Windows 10/11 via Microsoft Edge.
If missing, download from https://developer.microsoft.com/en-us/microsoft-edge/webview2/

---

## 1. Build the DAW

```bash
# From MSYS2 MinGW64 terminal
cd /c/Users/never/dawgs

# Configure (first time only, or after changing build options)
/c/msys64/mingw64/bin/python.exe waf configure --dist-target=mingw

# Build
/c/msys64/mingw64/bin/python.exe waf build
```

> **Note:** Use the MSYS2 Python (`/c/msys64/mingw64/bin/python.exe`) for WAF,
> not the Windows system Python. They have different toolchain expectations.

---

## 2. Set Up the Python Agent

The agent runs in its own virtual environment using Windows Python:

```bash
cd /c/Users/never/dawgs/agent

# Create venv (first time only)
/c/Python314/python.exe -m venv venv

# Activate
source venv/Scripts/activate

# Install dependencies
pip install -r requirements.txt
```

### Required API Keys

Create a `.env` file in the `agent/` directory:

```env
GROQ_API_KEY=your-key-here
# Optional, depending on which provider you use:
ANTHROPIC_API_KEY=your-key-here
GOOGLE_API_KEY=your-key-here
```

---

## 3. Run the DAW

```bash
cd /c/Users/never/dawgs
./gtk2_ardour/ardev
```

On startup, the DAW will:
1. Start the LuaBridge WebSocket server on **port 9871**
2. Serve the chat UI via HTTP at `http://localhost:9871/`
3. Open your default browser to the chat page
4. Attempt to auto-launch the Python agent

### If the agent doesn't auto-launch

Run it manually in a separate terminal:

```bash
cd /c/Users/never/dawgs/agent
source venv/Scripts/activate
python main.py --websocket-chat
```

You should see:
```
Connecting to Ardour LuaBridge at ws://localhost:9871...
Connected to Ardour LuaBridge
Registered as agent with LuaBridge
```

---

## 4. Using the Chat

Once the DAW and agent are both running:

1. Open **http://localhost:9871/** in your browser (auto-opened on startup)
2. The connection indicator should show **Connected**
3. Type a message and press **Enter** (Shift+Enter for newline)
4. The agent will respond using the configured LLM provider

### Agent CLI Options

```
python main.py --websocket-chat [OPTIONS]

--provider   LLM provider: groq (default), anthropic, gemini
--model      Override default model for the chosen provider
--host       LuaBridge host (default: localhost)
--port       LuaBridge port (default: 9871)
--api-key    Override API key from environment
```

Default models:
- **Groq:** `llama-3.3-70b-versatile`
- **Anthropic:** `claude-haiku-4-5`
- **Gemini:** `gemini-2.5-flash`

---

## Architecture Overview

```
Browser (Chat UI)          DAW (Ardour + LuaBridge)          Python Agent
     |                            |                              |
     |--- ws://localhost:9871 --->|                              |
     |    { type: "chat" }       |--- ws relay ----------------->|
     |                            |   { type: "chat_request" }   |
     |                            |                              |--- LLM API
     |                            |<-- ws relay -----------------|
     |<-- ws broadcast -----------|   { type: "chat_response" }  |
     |    { type: "chat_response"}|                              |
```

- **LuaBridge** acts as the central WebSocket hub on port 9871
- It serves the chat UI files via HTTP and relays messages between the browser and agent
- The agent connects as a special client and registers with `{ type: "register_agent" }`
- Chat messages flow: Browser -> LuaBridge -> Agent -> LLM -> Agent -> LuaBridge -> Browser

---

## Troubleshooting

### "Agent is not connected"
The Python agent isn't running or failed to register. Start it manually (see step 3).

### Browser shows 404
The chat UI files weren't found. Make sure `share/chat_ui/` exists with `index.html`, `css/style.css`, and `js/chat.js`.

### WebSocket connection fails
- Ensure the DAW is running with a session open
- Check that LuaBridge is enabled: **Preferences > Control Surfaces > LuaBridge**
- Verify port 9871 is not blocked by firewall

### WAF build uses wrong Python
Always use MSYS2 Python for WAF: `/c/msys64/mingw64/bin/python.exe waf build`

### pip install fails with "externally-managed-environment"
Use the venv, not system Python: `source agent/venv/Scripts/activate` first.
