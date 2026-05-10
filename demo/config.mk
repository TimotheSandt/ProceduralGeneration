# Project configuration — commit this file.

# ── Identity ──────────────────────────────────────────────────────────────────
PROJECT_NAME        ?= {{PROJECT_NAME}}
VERSION             ?= 0.1.0
PUBLISHER           ?=
MAINTAINER          ?=
PROJECT_DESCRIPTION ?=
ICON_NAME           ?=

# ── Windows packaging ─────────────────────────────────────────────────────────
ARCHITECTURE_WINDOWS ?= x64

# ── Linux packaging (.deb) ────────────────────────────────────────────────────
PACKAGE            ?= {{PROJECT_NAME_LOWER}}
ARCHITECTURE_LINUX ?= amd64
DEPENDS            ?=
SECTION            ?= utils
PRIORITY           ?= optional

# ── Extra vcpkg dependencies ──────────────────────────────────────────────────
# Added automatically by 'make install <pkg>'. Do not edit by hand.
EXTRA_LDLIBS   ?=
EXTRA_CXXFLAGS ?=
