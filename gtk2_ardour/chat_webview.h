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

#pragma once

#include <string>

#include <ytkmm/box.h>
#include <ytkmm/eventbox.h>
#include <ytkmm/label.h>

class ChatWebView : public Gtk::VBox
{
public:
	ChatWebView ();
	~ChatWebView ();

	/** Navigate the webview to the given URL */
	void navigate (const std::string& url);

	/** Check if the webview has been successfully initialized */
	bool is_initialized () const { return _initialized; }

#ifdef PLATFORM_WINDOWS
	/* Called from C-style COM callbacks in chat_webview.cc */
	void on_environment_created (long hr, void* env);
	void on_controller_created (long hr, void* controller);
#endif

private:
	void on_realize ();
	void on_size_allocate (Gtk::Allocation& allocation);

	Gtk::EventBox _host_widget;
	Gtk::Label    _status_label;

	bool        _initialized;
	std::string _pending_url;

#ifdef PLATFORM_WINDOWS
	void* _host_hwnd;
	void* _webview_environment;
	void* _webview_controller;
	void* _webview_view;

	void init_webview2 (void* parent_hwnd);
	void resize_webview ();
#endif
};
