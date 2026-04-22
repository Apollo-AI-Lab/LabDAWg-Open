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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include "chat_webview.h"

#include <cstdio>
#include <glibmm/main.h>

#ifdef PLATFORM_WINDOWS
#include <ydk/gdkwin32.h>
#include <shlwapi.h>

/*
 * Minimal WebView2 COM interface declarations for MinGW.
 *
 * MinGW does not ship WebView2.h or wrl.h. We declare only the
 * interfaces and methods we actually call, using the standard
 * COM vtable layout. The GUIDs match the official WebView2 SDK.
 *
 * Reference: https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32
 */

/* Forward declarations */
struct ICoreWebView2;
struct ICoreWebView2Environment;
struct ICoreWebView2Controller;

/* {4E8A3389-C9D8-4BD2-B6B5-124FEE6CC14D} */
static const IID IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler =
	{0x4e8a3389, 0xc9d8, 0x4bd2, {0xb6, 0xb5, 0x12, 0x4f, 0xee, 0x6c, 0xc1, 0x4d}};

/* {6C4819F3-C9B7-4260-8127-C9F5BDE7F68C} */
static const IID IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler =
	{0x6c4819f3, 0xc9b7, 0x4260, {0x81, 0x27, 0xc9, 0xf5, 0xbd, 0xe7, 0xf6, 0x8c}};

/*
 * COM callback handlers using C-style vtables.
 *
 * MinGW's C++ virtual inheritance can add destructor slots or RTTI
 * entries that shift the vtable layout. COM requires an exact binary
 * layout: QueryInterface at index 0, AddRef at 1, Release at 2,
 * then interface-specific methods. Using explicit vtable structs
 * guarantees this layout regardless of compiler.
 */

/* ---- Environment creation completed handler ---- */

typedef HRESULT (*EnvCreatedCallback)(HRESULT, ICoreWebView2Environment*, void*);

struct EnvironmentCompletedHandler;

struct EnvironmentCompletedHandlerVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(EnvironmentCompletedHandler*, REFIID, void**);
	ULONG   (STDMETHODCALLTYPE *AddRef)(EnvironmentCompletedHandler*);
	ULONG   (STDMETHODCALLTYPE *Release)(EnvironmentCompletedHandler*);
	HRESULT (STDMETHODCALLTYPE *Invoke)(EnvironmentCompletedHandler*, HRESULT, ICoreWebView2Environment*);
};

struct EnvironmentCompletedHandler {
	EnvironmentCompletedHandlerVtbl* lpVtbl;
	ULONG              refcount;
	EnvCreatedCallback cb;
	void*              ctx;
};

static HRESULT STDMETHODCALLTYPE ECH_QueryInterface (EnvironmentCompletedHandler* self, REFIID riid, void** ppv)
{
	if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler) {
		*ppv = self;
		self->lpVtbl->AddRef (self);
		return S_OK;
	}
	*ppv = nullptr;
	return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE ECH_AddRef (EnvironmentCompletedHandler* self) { return ++self->refcount; }
static ULONG STDMETHODCALLTYPE ECH_Release (EnvironmentCompletedHandler* self)
{
	ULONG r = --self->refcount;
	if (r == 0) free (self);
	return r;
}

static HRESULT STDMETHODCALLTYPE ECH_Invoke (EnvironmentCompletedHandler* self, HRESULT result, ICoreWebView2Environment* env)
{
	return self->cb (result, env, self->ctx);
}

static EnvironmentCompletedHandlerVtbl s_ech_vtbl = {
	ECH_QueryInterface, ECH_AddRef, ECH_Release, ECH_Invoke
};

static EnvironmentCompletedHandler* new_env_handler (EnvCreatedCallback cb, void* ctx)
{
	EnvironmentCompletedHandler* h = (EnvironmentCompletedHandler*)malloc (sizeof (EnvironmentCompletedHandler));
	h->lpVtbl   = &s_ech_vtbl;
	h->refcount = 1;
	h->cb       = cb;
	h->ctx      = ctx;
	return h;
}

/* ---- Controller creation completed handler ---- */

typedef HRESULT (*CtrlCreatedCallback)(HRESULT, ICoreWebView2Controller*, void*);

struct ControllerCompletedHandler;

struct ControllerCompletedHandlerVtbl {
	HRESULT (STDMETHODCALLTYPE *QueryInterface)(ControllerCompletedHandler*, REFIID, void**);
	ULONG   (STDMETHODCALLTYPE *AddRef)(ControllerCompletedHandler*);
	ULONG   (STDMETHODCALLTYPE *Release)(ControllerCompletedHandler*);
	HRESULT (STDMETHODCALLTYPE *Invoke)(ControllerCompletedHandler*, HRESULT, ICoreWebView2Controller*);
};

struct ControllerCompletedHandler {
	ControllerCompletedHandlerVtbl* lpVtbl;
	ULONG               refcount;
	CtrlCreatedCallback  cb;
	void*                ctx;
};

static HRESULT STDMETHODCALLTYPE CCH_QueryInterface (ControllerCompletedHandler* self, REFIID riid, void** ppv)
{
	if (riid == IID_IUnknown || riid == IID_ICoreWebView2CreateCoreWebView2ControllerCompletedHandler) {
		*ppv = self;
		self->lpVtbl->AddRef (self);
		return S_OK;
	}
	*ppv = nullptr;
	return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE CCH_AddRef (ControllerCompletedHandler* self) { return ++self->refcount; }
static ULONG STDMETHODCALLTYPE CCH_Release (ControllerCompletedHandler* self)
{
	ULONG r = --self->refcount;
	if (r == 0) free (self);
	return r;
}

static HRESULT STDMETHODCALLTYPE CCH_Invoke (ControllerCompletedHandler* self, HRESULT result, ICoreWebView2Controller* controller)
{
	return self->cb (result, controller, self->ctx);
}

static ControllerCompletedHandlerVtbl s_cch_vtbl = {
	CCH_QueryInterface, CCH_AddRef, CCH_Release, CCH_Invoke
};

static ControllerCompletedHandler* new_ctrl_handler (CtrlCreatedCallback cb, void* ctx)
{
	ControllerCompletedHandler* h = (ControllerCompletedHandler*)malloc (sizeof (ControllerCompletedHandler));
	h->lpVtbl   = &s_cch_vtbl;
	h->refcount = 1;
	h->cb       = cb;
	h->ctx      = ctx;
	return h;
}

/*
 * Minimal COM interface declarations via vtable.
 * We only declare methods up to and including the ones we call.
 * Earlier vtable slots are declared as padding (void* unused_N).
 */

/* ICoreWebView2 — we only use Navigate() */
struct ICoreWebView2Vtbl {
	/* IUnknown */
	HRESULT (STDMETHODCALLTYPE* QueryInterface)(ICoreWebView2*, REFIID, void**);
	ULONG   (STDMETHODCALLTYPE* AddRef)(ICoreWebView2*);
	ULONG   (STDMETHODCALLTYPE* Release)(ICoreWebView2*);
	/* ICoreWebView2 methods — Navigate is index 5 (after get_Settings=3, get_Source=4) */
	void* unused_3; /* get_Settings */
	void* unused_4; /* get_Source */
	HRESULT (STDMETHODCALLTYPE* Navigate)(ICoreWebView2*, LPCWSTR);
};

struct ICoreWebView2 {
	ICoreWebView2Vtbl* lpVtbl;
};

/* ICoreWebView2Controller — we use get_CoreWebView2, put_Bounds, Close, put_IsVisible */
struct ICoreWebView2ControllerVtbl {
	/* IUnknown */
	HRESULT (STDMETHODCALLTYPE* QueryInterface)(ICoreWebView2Controller*, REFIID, void**);
	ULONG   (STDMETHODCALLTYPE* AddRef)(ICoreWebView2Controller*);
	ULONG   (STDMETHODCALLTYPE* Release)(ICoreWebView2Controller*);
	/* ICoreWebView2Controller methods */
	void* unused_3; /* get_IsVisible */
	void* unused_4; /* put_IsVisible */
	void* unused_5; /* get_Bounds */
	HRESULT (STDMETHODCALLTYPE* put_Bounds)(ICoreWebView2Controller*, RECT);
	void* unused_7; /* get_ZoomFactor */
	void* unused_8; /* put_ZoomFactor */
	void* unused_9; /* add_ZoomFactorChanged */
	void* unused_10; /* remove_ZoomFactorChanged */
	void* unused_11; /* SetBoundsAndZoomFactor */
	void* unused_12; /* MoveFocus */
	void* unused_13; /* add_MoveFocusRequested */
	void* unused_14; /* remove_MoveFocusRequested */
	void* unused_15; /* add_GotFocus */
	void* unused_16; /* remove_GotFocus */
	void* unused_17; /* add_LostFocus */
	void* unused_18; /* remove_LostFocus */
	void* unused_19; /* add_AcceleratorKeyPressed */
	void* unused_20; /* remove_AcceleratorKeyPressed */
	void* unused_21; /* get_ParentWindow */
	void* unused_22; /* put_ParentWindow */
	void* unused_23; /* NotifyParentWindowPositionChanged */
	HRESULT (STDMETHODCALLTYPE* Close)(ICoreWebView2Controller*);
	HRESULT (STDMETHODCALLTYPE* get_CoreWebView2)(ICoreWebView2Controller*, ICoreWebView2**);
};

struct ICoreWebView2Controller {
	ICoreWebView2ControllerVtbl* lpVtbl;
};

/* ICoreWebView2Environment — we use CreateCoreWebView2Controller */
struct ICoreWebView2EnvironmentVtbl {
	/* IUnknown */
	HRESULT (STDMETHODCALLTYPE* QueryInterface)(ICoreWebView2Environment*, REFIID, void**);
	ULONG   (STDMETHODCALLTYPE* AddRef)(ICoreWebView2Environment*);
	ULONG   (STDMETHODCALLTYPE* Release)(ICoreWebView2Environment*);
	/* ICoreWebView2Environment methods */
	HRESULT (STDMETHODCALLTYPE* CreateCoreWebView2Controller)(
		ICoreWebView2Environment*, HWND, IUnknown* /* handler */);
};

struct ICoreWebView2Environment {
	ICoreWebView2EnvironmentVtbl* lpVtbl;
};

/*
 * CreateCoreWebView2EnvironmentWithOptions — loaded dynamically from
 * WebView2Loader.dll to avoid a hard link-time dependency.
 */
typedef HRESULT (STDMETHODCALLTYPE* CreateWebView2EnvironmentFunc)(
	PCWSTR browserExecutableFolder,
	PCWSTR userDataFolder,
	IUnknown* environmentOptions,
	IUnknown* environmentCompletedHandler);

static CreateWebView2EnvironmentFunc get_create_environment_func ()
{
	static CreateWebView2EnvironmentFunc fn = nullptr;
	if (!fn) {
		HMODULE lib = LoadLibraryW (L"WebView2Loader.dll");
		if (lib) {
			fn = (CreateWebView2EnvironmentFunc)GetProcAddress (
				lib, "CreateCoreWebView2EnvironmentWithOptions");
		}
	}
	return fn;
}

#endif /* PLATFORM_WINDOWS */


/* ================================================================
 * ChatWebView implementation
 * ================================================================ */

ChatWebView::ChatWebView ()
	: _initialized (false)
#ifdef PLATFORM_WINDOWS
	, _host_hwnd (nullptr)
	, _webview_environment (nullptr)
	, _webview_controller (nullptr)
	, _webview_view (nullptr)
#endif
{
	_status_label.set_text ("Initializing chat...");
	_status_label.set_alignment (0.5, 0.5);

	pack_start (_host_widget, true, true);
	pack_start (_status_label, false, false);

	_host_widget.signal_realize ().connect (
		sigc::mem_fun (*this, &ChatWebView::on_realize));
	_host_widget.signal_size_allocate ().connect (
		sigc::mem_fun (*this, &ChatWebView::on_size_allocate));

	_host_widget.show ();
	_status_label.show ();
}

ChatWebView::~ChatWebView ()
{
#ifdef PLATFORM_WINDOWS
	if (_webview_controller) {
		auto* ctrl = static_cast<ICoreWebView2Controller*>(_webview_controller);
		ctrl->lpVtbl->Close (ctrl);
		ctrl->lpVtbl->Release (ctrl);
		_webview_controller = nullptr;
	}
	if (_webview_view) {
		auto* view = static_cast<ICoreWebView2*>(_webview_view);
		view->lpVtbl->Release (view);
		_webview_view = nullptr;
	}
	if (_webview_environment) {
		auto* env = static_cast<ICoreWebView2Environment*>(_webview_environment);
		env->lpVtbl->Release (env);
		_webview_environment = nullptr;
	}
#endif
}

void
ChatWebView::navigate (const std::string& url)
{
	_pending_url = url;

#ifdef PLATFORM_WINDOWS
	if (_initialized && _webview_view) {
		std::wstring wurl (url.begin (), url.end ());
		auto* view = static_cast<ICoreWebView2*>(_webview_view);
		view->lpVtbl->Navigate (view, wurl.c_str ());
	}
#endif
}

void
ChatWebView::on_realize ()
{
#ifdef PLATFORM_WINDOWS
	/* Guard against multiple realize signals */
	if (_host_hwnd) {
		return;
	}

	GtkWidget* widget = GTK_WIDGET (_host_widget.gobj ());
	if (!widget->window) {
		return;
	}
	HWND gdk_hwnd = (HWND)gdk_win32_drawable_get_handle (widget->window);
	fprintf (stderr, "ChatWebView: got GDK HWND %p\n", gdk_hwnd);

	if (!gdk_hwnd) {
		return;
	}

	/* Create a dedicated Win32 child window for WebView2.
	 * This isolates WebView2's window management from GTK/GDK,
	 * preventing conflicts with GDK's window tracking. */
	static bool wc_registered = false;
	if (!wc_registered) {
		WNDCLASSEXW wc = {};
		wc.cbSize        = sizeof (WNDCLASSEXW);
		wc.lpfnWndProc   = DefWindowProcW;
		wc.hInstance      = GetModuleHandleW (nullptr);
		wc.lpszClassName  = L"LabDAWgChatWebViewHost";
		RegisterClassExW (&wc);
		wc_registered = true;
	}

	RECT rc;
	GetClientRect (gdk_hwnd, &rc);

	HWND child = CreateWindowExW (
		0, L"LabDAWgChatWebViewHost", L"",
		WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
		0, 0, rc.right - rc.left, rc.bottom - rc.top,
		gdk_hwnd, nullptr, GetModuleHandleW (nullptr), nullptr);

	_host_hwnd = (void*)child;
	fprintf (stderr, "ChatWebView: created child HWND %p (parent GDK %p)\n", child, gdk_hwnd);

	if (_host_hwnd) {
		init_webview2 (_host_hwnd);
	}
#endif
}

void
ChatWebView::on_size_allocate (Gtk::Allocation& allocation)
{
#ifdef PLATFORM_WINDOWS
	if (_host_hwnd) {
		/* Resize the child Win32 window to match the GTK allocation */
		MoveWindow (static_cast<HWND>(_host_hwnd),
		            0, 0, allocation.get_width (), allocation.get_height (), TRUE);
	}
	resize_webview ();
#endif
}

#ifdef PLATFORM_WINDOWS

/* Thin C callbacks that forward to ChatWebView member functions */
static HRESULT wv2_ctrl_cb (HRESULT result, ICoreWebView2Controller* ctrl, void* ctx)
{
	static_cast<ChatWebView*>(ctx)->on_controller_created ((long)result, ctrl);
	return S_OK;
}

static HRESULT wv2_env_cb (HRESULT result, ICoreWebView2Environment* env, void* ctx)
{
	static_cast<ChatWebView*>(ctx)->on_environment_created ((long)result, env);
	return S_OK;
}

void
ChatWebView::on_controller_created (long hr, void* controller_ptr)
{
	fprintf (stderr, "ChatWebView: controller callback, hr=0x%lx\n", (unsigned long)hr);

	if (FAILED ((HRESULT)hr) || !controller_ptr) {
		fprintf (stderr, "ChatWebView: failed to create controller\n");
		return;
	}

	auto* controller = static_cast<ICoreWebView2Controller*>(controller_ptr);
	fprintf (stderr, "ChatWebView: AddRef controller...\n");
	controller->lpVtbl->AddRef (controller);
	_webview_controller = controller;

	fprintf (stderr, "ChatWebView: get_CoreWebView2...\n");
	ICoreWebView2* view = nullptr;
	controller->lpVtbl->get_CoreWebView2 (controller, &view);
	if (view) {
		fprintf (stderr, "ChatWebView: got view, AddRef...\n");
		view->lpVtbl->AddRef (view);
		_webview_view = view;
	}

	fprintf (stderr, "ChatWebView: setting initialized...\n");
	_initialized = true;

	fprintf (stderr, "ChatWebView: calling resize_webview...\n");
	resize_webview ();

	fprintf (stderr, "ChatWebView: navigating...\n");
	if (!_pending_url.empty ()) {
		navigate (_pending_url);
	}

	fprintf (stderr, "ChatWebView: WebView2 fully initialized\n");

	if (!_pending_url.empty ()) {
		navigate (_pending_url);
	}

	fprintf (stderr, "ChatWebView: WebView2 initialized successfully\n");
}

void
ChatWebView::on_environment_created (long hr, void* env_ptr)
{
	fprintf (stderr, "ChatWebView: environment callback, hr=0x%lx\n", (unsigned long)hr);

	if (FAILED ((HRESULT)hr) || !env_ptr) {
		fprintf (stderr, "ChatWebView: failed to create environment\n");
		return;
	}

	auto* env = static_cast<ICoreWebView2Environment*>(env_ptr);
	env->lpVtbl->AddRef (env);
	_webview_environment = env;

	ControllerCompletedHandler* ctrl_handler = new_ctrl_handler (wv2_ctrl_cb, this);
	env->lpVtbl->CreateCoreWebView2Controller (
		env, static_cast<HWND>(_host_hwnd), (IUnknown*)ctrl_handler);
}

void
ChatWebView::init_webview2 (void* parent_hwnd)
{
	HWND parent = static_cast<HWND>(parent_hwnd);
	fprintf (stderr, "ChatWebView: init_webview2 called with HWND %p\n", parent);

	auto create_fn = get_create_environment_func ();
	if (!create_fn) {
		_status_label.set_text ("WebView2Loader.dll not found.\n"
		                        "Please install Microsoft Edge WebView2 Runtime.");
		fprintf (stderr, "ChatWebView: WebView2Loader.dll not found\n");
		return;
	}

	EnvironmentCompletedHandler* env_handler = new_env_handler (wv2_env_cb, this);

	fprintf (stderr, "ChatWebView: calling CreateCoreWebView2EnvironmentWithOptions...\n");
	HRESULT hr = create_fn (nullptr, nullptr, nullptr, (IUnknown*)env_handler);
	fprintf (stderr, "ChatWebView: CreateCoreWebView2EnvironmentWithOptions returned 0x%lx\n", (unsigned long)hr);
	if (FAILED (hr)) {
		_status_label.set_text ("Failed to initialize WebView2.\n"
		                        "Please install Microsoft Edge WebView2 Runtime.");
		fprintf (stderr, "ChatWebView: CreateCoreWebView2EnvironmentWithOptions failed (0x%lx)\n",
		         (unsigned long)hr);
	}
}

void
ChatWebView::resize_webview ()
{
	if (!_webview_controller || !_host_hwnd) {
		return;
	}

	RECT bounds;
	GetClientRect (static_cast<HWND>(_host_hwnd), &bounds);

	auto* ctrl = static_cast<ICoreWebView2Controller*>(_webview_controller);
	ctrl->lpVtbl->put_Bounds (ctrl, bounds);
}

#endif /* PLATFORM_WINDOWS */
