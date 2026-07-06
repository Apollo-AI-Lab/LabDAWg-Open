# Labdawg-open Build & Run Guide

This repository contains an Ardour-based digital audio workstation. These notes cover basic
developer builds and ordinary ways to launch the application.

For general upstream build information, see the Ardour development documentation:
https://ardour.org/development.html

## Getting the App

Most users should download a packaged build from the project website or release page when one is
available. After installing the package for your platform, launch the application from your
operating system's normal application launcher.

Developer builds are useful when you want to inspect, modify, or test the source code directly.

## Prerequisites

Build requirements vary by platform. This project follows Ardour's normal build system and expects
the standard native compiler, development libraries, and Waf-based build workflow used by Ardour.

On Windows, use an MSYS2 MinGW64 terminal for source builds:

```bash
pacman -Syu
pacman -S mingw-w64-x86_64-toolchain
```

Additional packages may be needed depending on the platform and enabled build options. Refer to
the Ardour build documentation for the current dependency list.

## Build From Source

From the repository root:

```bash
./waf configure
./waf build
```

On Windows from an MSYS2 MinGW64 terminal, invoke Waf with the MSYS2 Python interpreter:

```bash
/c/msys64/mingw64/bin/python.exe waf configure --dist-target=mingw
/c/msys64/mingw64/bin/python.exe waf build
```

Use the MSYS2 Python for Waf on Windows. The Windows system Python and MSYS2 Python have different
toolchain expectations.

## Run a Development Build

After a successful build, launch the development executable from the source tree:

```bash
./gtk2_ardour/ardev
```

On Windows/MSYS2:

```bash
./gtk2_ardour/ardev.exe
```

If the application does not start, verify that the build completed successfully and that required
runtime libraries are available in your development environment.

## Install

To install from a local build:

```bash
./waf install
```

Depending on the installation prefix and platform, this command may require elevated permissions.
You can configure a local prefix during the configure step if you want to install without writing
to a system directory.

## Source and License Notes

This project is based on Ardour and is distributed under the GNU General Public License, version 2
or later. See `COPYING` for the full license text and plugin clarification inherited from Ardour.

When distributing binary builds, provide the corresponding source code and the scripts used to
control compilation and installation, as required by the GPL.
