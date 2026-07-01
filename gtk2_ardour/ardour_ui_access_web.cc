/*
 * Copyright (C) 2005-2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2005-2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2005-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2008-2010 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2015 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2013-2016 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
 * Copyright (C) 2016-2018 Len Ovens <len@ovenwerks.net>
 * Copyright (C) 2017 Johannes Mueller <github@johannes-mueller.org>
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
 * Modified by Apollo AI Lab, Inc. on 2026-06-26
 * Summary of changes: Added macOS app bundle launcher path for the Python agent,
 * resolving the bundled Python interpreter under Contents/Resources/ when running
 * from a packaged .app.
 */

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#include "gtk2ardour-version.h"
#endif

#ifdef _MSC_VER
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

#include <glibmm/spawn.h>

#include "pbd/openuri.h"
#ifdef PLATFORM_WINDOWS
#include "pbd/windows_special_dirs.h"
#endif

#include "ardour/filesystem_paths.h"

#include "ardour_message.h"
#include "ardour_ui.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace std;

void
ARDOUR_UI::launch_chat ()
{
	/* Launch the Dexter chat window (Agent_2.0).
	 *
	 * Two layouts, probed in order:
	 *
	 *  1. Installed: the MSI ships a frozen agent at $INSTDIR\agent\
	 *     LabDAWgAgent.exe. $INSTDIR is derived from ARDOUR_DLL_PATH
	 *     ($INSTDIR\lib\ardourN → two levels up), with the registry key
	 *     written by the installer as fallback.
	 *
	 *  2. Dev tree: ardev sets ARDOUR_DLL_PATH=$TOP/build/libs — two levels
	 *     up gives $TOP; spawn Agent_2.0/venv python on chat_ui.py.
	 *
	 * Either way the agent auto-starts its FastAPI server and reconnects to
	 * LabDAWg; --watch-pid ties its lifetime (and the install-on-exit update
	 * choreography) to this process.
	 */
	/* Resolve relative to the actually-running module rather than the raw
	 * ARDOUR_DLL_PATH env var: installed builds have no launcher script to set
	 * it, so on Windows ardour_dll_directory() falls back to the package dir
	 * ($INSTDIR\lib\ardourN) computed from this exe's location. Two levels up
	 * is $INSTDIR (installed) or $TOP (dev tree where ardev sets the env). This
	 * avoids depending on a possibly-unset env var or a stale registry value
	 * left by a previous install. */
	std::string dll_path = ARDOUR::ardour_dll_directory ();
	std::string base;
	if (!dll_path.empty ()) {
		/* installed: $INSTDIR\lib\ardourN — dev: $TOP/build/libs */
		base = Glib::path_get_dirname (Glib::path_get_dirname (dll_path));
	}

	std::vector<std::string> argv;
	std::string workdir;

#ifdef PLATFORM_WINDOWS
	std::string agent_exe;
	if (!base.empty ()) {
		std::string candidate = Glib::build_filename (base, "agent", "LabDAWgAgent.exe");
		if (Glib::file_test (candidate, Glib::FILE_TEST_EXISTS)) {
			agent_exe = candidate;
		}
	}
	if (agent_exe.empty ()) {
		/* Fallback: Install_Dir registry value written by the installer. */
		std::string install_dir;
		if (PBD::windows_query_registry ("Software\\" PROGRAM_NAME "\\v" PROGRAM_VERSION "\\w64", "Install_Dir", install_dir)) {
			std::string candidate = Glib::build_filename (install_dir, "agent", "LabDAWgAgent.exe");
			if (Glib::file_test (candidate, Glib::FILE_TEST_EXISTS)) {
				agent_exe = candidate;
			}
		}
	}
	if (!agent_exe.empty ()) {
		argv.push_back (agent_exe);
		workdir = Glib::path_get_dirname (agent_exe);
	}
#endif

#ifdef __APPLE__
	/* macOS .app bundle: the agent source and a bundled portable Python live
	 * under Contents/Resources/ (see tools/mac/package.sh) — there is no venv/
	 * inside the bundle. ardour_dll_directory() is Contents/lib, so Resources is
	 * its sibling. If this layout is not present (e.g. a macOS dev tree) we fall
	 * through to the dev-tree branch below, which uses Agent_2.0/venv. */
	if (argv.empty () && !dll_path.empty ()) {
		std::string resources   = Glib::build_filename (Glib::path_get_dirname (dll_path), "Resources");
		std::string py          = Glib::build_filename (resources, "python", "bin", "python3");
		std::string chat_script = Glib::build_filename (resources, "Agent_2.0", "chat_ui.py");
		if (Glib::file_test (py, Glib::FILE_TEST_EXISTS) &&
		    Glib::file_test (chat_script, Glib::FILE_TEST_EXISTS)) {
			argv.push_back (py);
			argv.push_back (chat_script);
			workdir = Glib::build_filename (resources, "Agent_2.0");
		}
	}
#endif

	if (argv.empty ()) {
		/* Dev tree fallback. */
		if (base.empty ()) {
			ArdourMessageDialog dlg (_("ARDOUR_DLL_PATH is not set — cannot locate Dexter."),
			                         false, Gtk::MESSAGE_ERROR);
			dlg.run ();
			return;
		}
#ifdef _WIN32
		std::string venv_python = Glib::build_filename (base, "Agent_2.0", "venv", "Scripts", "python.exe");
#else
		std::string venv_python = Glib::build_filename (base, "Agent_2.0", "venv", "bin", "python");
#endif
		std::string chat_script = Glib::build_filename (base, "Agent_2.0", "chat_ui.py");

		if (!Glib::file_test (venv_python, Glib::FILE_TEST_EXISTS) ||
		    !Glib::file_test (chat_script, Glib::FILE_TEST_EXISTS)) {
			ArdourMessageDialog dlg (_("Could not find the Dexter chat files.\n\n"
			                           "Installed builds need agent\\LabDAWgAgent.exe next to bin\\; "
			                           "dev trees need Agent_2.0/venv and Agent_2.0/chat_ui.py."),
			                         false, Gtk::MESSAGE_ERROR);
			dlg.run ();
			return;
		}
		argv.push_back (venv_python);
		argv.push_back (chat_script);
		workdir = Glib::path_get_dirname (chat_script);
	}

	try {
		argv.push_back ("--watch-pid");
		argv.push_back (std::to_string (getpid ()));
		Glib::spawn_async (workdir, argv, Glib::SPAWN_DEFAULT);
	} catch (Glib::SpawnError const& e) {
		std::string msg = "Failed to launch Dexter chat: ";
		msg += e.what ();
		ArdourMessageDialog dlg (msg, false, Gtk::MESSAGE_ERROR);
		dlg.run ();
	}
}

void
ARDOUR_UI::launch_tutorial ()
{
}

void
ARDOUR_UI::launch_reference ()
{
}

void
ARDOUR_UI::launch_tracker ()
{
}

void
ARDOUR_UI::launch_subscribe ()
{
}

void
ARDOUR_UI::launch_website ()
{
}

void
ARDOUR_UI::launch_website_dev ()
{
}

void
ARDOUR_UI::launch_forums ()
{
}

void
ARDOUR_UI::launch_howto_report ()
{
}

