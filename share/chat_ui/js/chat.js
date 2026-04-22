/**
 * Copyright (C) 2026 Apollo AI Lab, Inc.
 * Licensed under the GNU General Public License, version 2 (GPL-2.0)
 *
 * LabDAWg Chat UI — WebSocket client for the LuaBridge chat relay
 *
 * Connects to ws://localhost:9871 and exchanges chat messages
 * with the Python AI agent via the LuaBridge relay protocol.
 */

(function () {
  'use strict';

  const WS_URL = 'ws://localhost:9871';
  const RECONNECT_INTERVAL_MS = 3000;

  let ws = null;
  let messageIdCounter = 0;
  let currentStreamingEl = null;
  let reconnectTimer = null;

  /* DOM elements */
  const messagesEl = document.getElementById('chat-messages');
  const inputEl = document.getElementById('chat-input');
  const sendBtn = document.getElementById('send-btn');
  const statusEl = document.getElementById('connection-status');

  /* ── WebSocket connection ─────────────────────────────────── */

  function connect () {
    if (ws && (ws.readyState === WebSocket.CONNECTING || ws.readyState === WebSocket.OPEN)) {
      return;
    }

    setStatus('connecting');

    try {
      ws = new WebSocket(WS_URL, 'lws-luabridge');
    } catch (e) {
      setStatus('disconnected');
      scheduleReconnect();
      return;
    }

    ws.onopen = function () {
      setStatus('connected');
      if (reconnectTimer) {
        clearTimeout(reconnectTimer);
        reconnectTimer = null;
      }
    };

    ws.onclose = function () {
      setStatus('disconnected');
      ws = null;
      scheduleReconnect();
    };

    ws.onerror = function () {
      setStatus('disconnected');
    };

    ws.onmessage = function (event) {
      try {
        var msg = JSON.parse(event.data);
        handleServerMessage(msg);
      } catch (e) {
        console.error('Failed to parse message:', e);
      }
    };
  }

  function scheduleReconnect () {
    if (reconnectTimer) return;
    reconnectTimer = setTimeout(function () {
      reconnectTimer = null;
      connect();
    }, RECONNECT_INTERVAL_MS);
  }

  function setStatus (state) {
    statusEl.textContent = state;
    statusEl.className = 'status-' + state;
  }

  /* ── Message handling ─────────────────────────────────────── */

  function handleServerMessage (msg) {
    if (msg.type === 'pong') {
      return;
    }

    if (msg.type === 'chat_response') {
      handleChatResponse(msg);
      return;
    }

    /* For non-chat messages (lua results, state), show as system info */
    if (msg.type === 'result') {
      if (msg.error) {
        addSystemMessage('Error: ' + msg.error);
      }
      return;
    }
  }

  function handleChatResponse (msg) {
    if (!currentStreamingEl) {
      currentStreamingEl = addMessage('', 'assistant');
    }

    if (msg.content) {
      /* Append streaming content */
      var textNode = currentStreamingEl.querySelector('.message-text');
      if (textNode) {
        textNode.textContent += msg.content;
      }
    }

    if (msg.done) {
      /* Finalize: render markdown and remove cursor */
      var textNode = currentStreamingEl.querySelector('.message-text');
      if (textNode) {
        textNode.innerHTML = renderMarkdown(textNode.textContent);
      }
      var cursor = currentStreamingEl.querySelector('.typing-indicator');
      if (cursor) cursor.remove();
      currentStreamingEl = null;
      sendBtn.disabled = false;
      inputEl.disabled = false;
      inputEl.focus();
    }

    scrollToBottom();
  }

  /* ── Send message ─────────────────────────────────────────── */

  function sendMessage () {
    var text = inputEl.value.trim();
    if (!text || !ws || ws.readyState !== WebSocket.OPEN) return;

    addMessage(text, 'user');
    inputEl.value = '';
    autoResizeInput();

    sendBtn.disabled = true;
    inputEl.disabled = true;

    var id = 'msg_' + (++messageIdCounter);
    ws.send(JSON.stringify({
      type: 'chat',
      id: id,
      message: text
    }));
  }

  /* ── DOM helpers ──────────────────────────────────────────── */

  function addMessage (text, role) {
    var el = document.createElement('div');
    el.className = 'message message-' + role;

    var textSpan = document.createElement('span');
    textSpan.className = 'message-text';

    if (role === 'user') {
      textSpan.textContent = text;
    } else if (role === 'assistant') {
      textSpan.textContent = text;
      /* Add typing cursor for streaming */
      var cursor = document.createElement('span');
      cursor.className = 'typing-indicator';
      el.appendChild(textSpan);
      el.appendChild(cursor);
      messagesEl.appendChild(el);
      scrollToBottom();
      return el;
    }

    el.appendChild(textSpan);
    messagesEl.appendChild(el);
    scrollToBottom();
    return el;
  }

  function addSystemMessage (text) {
    var el = document.createElement('div');
    el.className = 'message message-system';
    el.textContent = text;
    messagesEl.appendChild(el);
    scrollToBottom();
  }

  function scrollToBottom () {
    messagesEl.scrollTop = messagesEl.scrollHeight;
  }

  /* ── Minimal markdown rendering ───────────────────────────── */

  function renderMarkdown (text) {
    /* Escape HTML */
    var html = text
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;');

    /* Code blocks: ```lang\n...\n``` */
    html = html.replace(/```(\w*)\n([\s\S]*?)```/g, function (m, lang, code) {
      return '<pre><code>' + code.trim() + '</code></pre>';
    });

    /* Inline code: `...` */
    html = html.replace(/`([^`]+)`/g, '<code>$1</code>');

    /* Bold: **...** */
    html = html.replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>');

    /* Italic: *...* */
    html = html.replace(/\*([^*]+)\*/g, '<em>$1</em>');

    /* Line breaks */
    html = html.replace(/\n/g, '<br>');

    return html;
  }

  /* ── Input handling ───────────────────────────────────────── */

  function autoResizeInput () {
    inputEl.style.height = 'auto';
    inputEl.style.height = Math.min(inputEl.scrollHeight, 120) + 'px';
  }

  inputEl.addEventListener('input', autoResizeInput);

  inputEl.addEventListener('keydown', function (e) {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      sendMessage();
    }
  });

  sendBtn.addEventListener('click', sendMessage);

  /* ── Initialize ───────────────────────────────────────────── */

  addSystemMessage('Connecting to DAW...');
  connect();
})();
