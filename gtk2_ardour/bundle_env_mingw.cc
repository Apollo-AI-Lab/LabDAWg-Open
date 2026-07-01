/*
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2016 Paul Davis <paul@linuxaudiosystems.com>
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

/*
 * Modified by Apollo AI Lab, Inc. on 2026-06-06
 * Summary of changes: Set PANGOCAIRO_BACKEND=fc to enable the fontconfig backend
 * for correct custom font rendering on Windows (MinGW build).
 */

#include <stdlib.h>
#include <string>
#include "bundle_env.h"
#include "pbd/i18n.h"

#include <glibmm.h>
#include <fontconfig/fontconfig.h>
#include <pango/pangoft2.h>
#include <pango/pangocairo.h>

#include <windows.h>
#include <wingdi.h>

#include "ardour/ardour.h"
#include "ardour/search_paths.h"
#include "ardour/filesystem_paths.h"

#include "pbd/file_utils.h"
#include "pbd/epa.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;


enum MY_PROCESS_DPI_AWARENESS {
  PROCESS_DPI_UNAWARE,
  PROCESS_SYSTEM_DPI_AWARE,
  PROCESS_PER_MONITOR_DPI_AWARE
};

typedef HRESULT (WINAPI* SetProcessDpiAwareness_t)(MY_PROCESS_DPI_AWARENESS);

static void
set_suil_module_dir ()
{
	const char* cstr = getenv ("SUIL_MODULE_DIR");
	if (cstr && *cstr && Glib::file_test (cstr, Glib::FILE_TEST_IS_DIR)) {
		return;
	}

	const std::string bundle_suil_dir = Glib::build_filename (ardour_dll_directory(), "suil");
	if (Glib::file_test (bundle_suil_dir, Glib::FILE_TEST_IS_DIR)) {
		Glib::setenv ("SUIL_MODULE_DIR", bundle_suil_dir, true);
		return;
	}

	const std::string dev_suil_dir = Glib::build_filename (Glib::build_filename (ardour_dll_directory(), "tk"), "suil");
	if (Glib::file_test (dev_suil_dir, Glib::FILE_TEST_IS_DIR)) {
		Glib::setenv ("SUIL_MODULE_DIR", dev_suil_dir, true);
		return;
	}

	Glib::setenv ("SUIL_MODULE_DIR", bundle_suil_dir, true);
}

void
fixup_bundle_environment (int, char* [], string & localedir)
{
	EnvironmentalProtectionAgency::set_global_epa (new EnvironmentalProtectionAgency (true));
	/* what to do ? */
	// we should at least set ARDOUR_DATA_PATH to prevent the warning message.
	// setting a FONTCONFIG_FILE won't hurt either see bundle_env_msvc.cc
	// (pangocairo prefers the windows gdi backend unless PANGOCAIRO_BACKEND=fc is set)
	Glib::setenv ("PANGOCAIRO_BACKEND", "fc", true);

	// Unset GTK2_RC_FILES so that only ardour specific files are loaded
	Glib::unsetenv ("GTK2_RC_FILES");

	std::string path;

	if (ARDOUR::translations_are_enabled ()) {
		path = windows_search_path().to_string();
		path += "\\locale";
		Glib::setenv ("GTK_LOCALEDIR", path, true);

		// and return the same path to our caller
		localedir = path;
	}

	const char *cstr;
	cstr = getenv ("VAMP_PATH");
	if (cstr) {
		path = cstr;
		path += G_SEARCHPATH_SEPARATOR;
	} else {
		path = "";
	}
	path += Glib::build_filename(ardour_dll_directory(), "vamp");
	path += G_SEARCHPATH_SEPARATOR;
	path += "%ProgramFiles%\\Vamp Plugins"; // default vamp path
	path += G_SEARCHPATH_SEPARATOR;
	path += "%COMMONPROGRAMFILES%\\Vamp Plugins";
	Glib::setenv ("VAMP_PATH", path, true);

	set_suil_module_dir ();

	/* XXX this should really be PRODUCT_EXE see tools/x-win/package.sh
	 * ardour on windows does not have a startup wrapper script.
	 *
	 * then again, there's probably nobody using NSM on windows.
	 * because neither nsmd nor the GUI is currently available for windows.
	 * furthermore it'll be even less common for derived products.
	 */
	Glib::setenv ("ARDOUR_SELF", Glib::build_filename(ardour_dll_directory(), "ardour.exe"), true);

	/* https://docs.microsoft.com/en-us/windows/win32/api/shellscalingapi/nf-shellscalingapi-setprocessdpiawareness */
	HMODULE module = LoadLibraryA ("Shcore.dll");
	if (module) {
		SetProcessDpiAwareness_t setProcessDpiAwareness = reinterpret_cast<SetProcessDpiAwareness_t> (GetProcAddress (module, "SetProcessDpiAwareness"));
		if (setProcessDpiAwareness) {
			setProcessDpiAwareness (PROCESS_SYSTEM_DPI_AWARE);
		}
		FreeLibrary (module);
	}
}

static std::string ardour_mono_file;
static std::string ardour_sans_file;
static std::string manrope_file;
static std::string space_grotesk_file;

void
load_custom_fonts()
{
	if (!find_file (ardour_data_search_path(), "ArdourMono.ttf", ardour_mono_file)) {
		cerr << _("Cannot find ArdourMono TrueType font") << endl;
	}
	if (!find_file (ardour_data_search_path(), "ArdourSans.ttf", ardour_sans_file)) {
		cerr << _("Cannot find ArdourSans TrueType font") << endl;
	}
	if (!find_file (ardour_data_search_path(), "Manrope.ttf", manrope_file)) {
		cerr << _("Cannot find Manrope TrueType font") << endl;
	}
	if (!find_file (ardour_data_search_path(), "SpaceGrotesk.ttf", space_grotesk_file)) {
		cerr << _("Cannot find SpaceGrotesk TrueType font") << endl;
	}

	if (ardour_mono_file.empty () && manrope_file.empty ()) {
		return;
	}

	/* The bundle forces the fontconfig pangocairo backend
	 * (PANGOCAIRO_BACKEND=fc, set in fixup_bundle_environment above), so
	 * register the bundled fonts via fontconfig — exactly as the Linux build
	 * does (bundle_env_linux.cc) — and do NOT touch the pango font map here.
	 *
	 * The previous code queried pango_cairo_font_map_get_default() to branch
	 * fc-vs-GDI. That both mis-detected the backend (a PangoCairoFcFontMap is
	 * not a PangoFT2FontMap, so the check was always false) AND force-created
	 * the font map before FcConfigSetCurrent — so the custom UI font (Manrope)
	 * was only ever registered with Windows GDI, which the fontconfig backend
	 * never reads. It happened to render in a dev shell only because that
	 * environment's fontconfig already knew the font; the shortcut-launched
	 * install did not, and fell back to a system font. The font map is created
	 * later by gtk_init(), after the config below is made current. */
	FcConfig* config = FcInitLoadConfigAndFonts ();

	if (!ardour_mono_file.empty () && FcFalse == FcConfigAppFontAddFile (config, reinterpret_cast<const FcChar8*>(ardour_mono_file.c_str ()))) {
		cerr << _("Cannot load ArdourMono TrueType font.") << endl;
	}
	if (!ardour_sans_file.empty () && FcFalse == FcConfigAppFontAddFile (config, reinterpret_cast<const FcChar8*>(ardour_sans_file.c_str ()))) {
		cerr << _("Cannot load ArdourSans TrueType font.") << endl;
	}
	if (!manrope_file.empty () && FcFalse == FcConfigAppFontAddFile (config, reinterpret_cast<const FcChar8*>(manrope_file.c_str ()))) {
		cerr << _("Cannot load Manrope TrueType font.") << endl;
	}
	if (!space_grotesk_file.empty () && FcFalse == FcConfigAppFontAddFile (config, reinterpret_cast<const FcChar8*>(space_grotesk_file.c_str ()))) {
		cerr << _("Cannot load SpaceGrotesk TrueType font.") << endl;
	}

	if (FcFalse == FcConfigSetCurrent (config)) {
		cerr << _("Failed to set fontconfig configuration.") << endl;
	}
}
