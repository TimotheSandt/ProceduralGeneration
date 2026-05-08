# Makefile — ProceduralGeneration engine root
#
# ── Engine ────────────────────────────────────────────────────────────────────
#   make debug           build engine libraries (debug)
#   make release         build engine libraries (release)
#   make dist            package engine for distribution (dist/engine-<platform>/)
#   make engine-tool     build the scaffolding binary (dist/engine-<platform>/engine)
#   make install-demo    install dist package into demo/engine/
#
# ── Demo game ─────────────────────────────────────────────────────────────────
#   make demo            build and run demo (debug)
#   make demo-debug      build demo (debug)
#   make demo-release    build demo (release)
#   make demo-run        run last built demo binary
#   make demo-clean      remove demo build artifacts
#   make demo-fclean     remove demo build artifacts + binaries

SHELL := /bin/sh

ENGINE_MK := Libraries/Makefile

# ── Engine targets ─────────────────────────────────────────────────────────────
.PHONY: all debug dev release dist engine-tool install-demo clean fclean

all debug dev release:
	$(MAKE) -f $(ENGINE_MK) $@

dist:
	$(MAKE) -f $(ENGINE_MK) dist

engine-tool:
	$(MAKE) -f $(ENGINE_MK) engine-tool

install-demo:
	$(MAKE) -f $(ENGINE_MK) install-demo

clean:
	$(MAKE) -f $(ENGINE_MK) clean

fclean:
	$(MAKE) -f $(ENGINE_MK) fclean

# ── Demo game targets ──────────────────────────────────────────────────────────
.PHONY: demo demo-debug demo-dev demo-release demo-run demo-clean demo-fclean

demo demo-debug:
	$(MAKE) -C demo debug

demo-dev:
	$(MAKE) -C demo dev

demo-release:
	$(MAKE) -C demo release

demo-run:
	$(MAKE) -C demo run

demo-clean:
	$(MAKE) -C demo clean

demo-fclean:
	$(MAKE) -C demo fclean
