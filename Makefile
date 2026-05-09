# Makefile — ProceduralGeneration engine root
#
# ── vcpkg ─────────────────────────────────────────────────────────────────────
#   make install-deps          install engine vcpkg deps
#   make install-deps-demo     install demo vcpkg deps
#   make install-deps-all      both
#
# ── Engine (Libraries/) ───────────────────────────────────────────────────────
#   make debug                 build engine archives (debug)
#   make release               build engine archives (release)
#   make engine                build debug+release, dist, tools, and demo/engine/
#   make dist                  package engine → dist/engine-<platform>/
#   make engine-tool           build the engine scaffolding binary
#   make sync-demo             fast sync of current BUILD_TYPE into demo/engine/
#   make install-demo          copy full dist package into demo/engine/
#   make clean / fclean        remove engine build artifacts
#
# ── Demo (demo/) ──────────────────────────────────────────────────────────────
#   make demo-debug            build demo (debug)
#   make demo-release          build demo (release)
#   make demo-run              run last built demo binary
#   make demo-install [pkg…]   add vcpkg dep(s) to demo
#   make demo-search kw        search vcpkg catalog
#   make demo-clean / fclean   remove demo build artifacts
#
# ── Combined ──────────────────────────────────────────────────────────────────
#   make all-debug             engine debug → sync → demo debug
#   make all-release           engine release → sync → demo release
#   make setup                 install engine deps + make engine + demo deps + demo debug
#   make setup-run             setup + run demo

SHELL      := sh
VCPKG      ?= vcpkg
BUILD_TYPE ?= debug
ENGINE_MK  := Libraries/Makefile

# ── vcpkg ─────────────────────────────────────────────────────────────────────
.PHONY: install-deps install-deps-demo install-deps-all

install-deps:
	$(MAKE) -f $(ENGINE_MK) install VCPKG=$(VCPKG)

install-deps-demo:
	$(MAKE) -C demo install VCPKG=$(VCPKG)

install-deps-all: install-deps install-deps-demo

# ── Engine ────────────────────────────────────────────────────────────────────
.PHONY: debug dev release engine dist engine-tool sync-demo install-demo clean fclean info

debug dev release:
	$(MAKE) -f $(ENGINE_MK) BUILD_TYPE=$@ $@

engine:
	$(MAKE) -f $(ENGINE_MK) engine

dist:
	$(MAKE) -f $(ENGINE_MK) dist

engine-tool:
	$(MAKE) -f $(ENGINE_MK) engine-tool

sync-demo:
	$(MAKE) -f $(ENGINE_MK) sync-demo BUILD_TYPE=$(BUILD_TYPE)

install-demo:
	$(MAKE) -f $(ENGINE_MK) install-demo

clean:
	$(MAKE) -f $(ENGINE_MK) clean

fclean:
	$(MAKE) -f $(ENGINE_MK) fclean

# ── Demo ──────────────────────────────────────────────────────────────────────
.PHONY: demo-debug demo-dev demo-release demo-run demo-clean demo-fclean
.PHONY: demo-install demo-search

demo-debug:
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

demo-install:
	$(MAKE) -C demo install $(ARGS) LIBS=$(LIBS) VCPKG=$(VCPKG)

demo-search:
	$(MAKE) -C demo search Q=$(Q) VCPKG=$(VCPKG)

# ── Combined ──────────────────────────────────────────────────────────────────
.PHONY: all-debug all-release setup setup-run

all-debug:
	$(MAKE) -f $(ENGINE_MK) sync-demo BUILD_TYPE=debug
	$(MAKE) -C demo debug

all-release:
	$(MAKE) -f $(ENGINE_MK) sync-demo BUILD_TYPE=release
	$(MAKE) -C demo release

# install engine deps → prepare full engine → install demo deps → build demo
setup:
	$(MAKE) -f $(ENGINE_MK) install VCPKG=$(VCPKG)
	$(MAKE) -f $(ENGINE_MK) engine
	$(MAKE) -C demo install VCPKG=$(VCPKG)
	$(MAKE) -C demo debug

setup-run: setup demo-run

# ── Info ──────────────────────────────────────────────────────────────────────
.PHONY: info
info:
	@printf "\n=== Engine ===\n"
	@$(MAKE) -f $(ENGINE_MK) info --no-print-directory
	@printf "=== Demo ===\n"
	@$(MAKE) -C demo info --no-print-directory
	@printf "BUILD_TYPE  : $(BUILD_TYPE)\n"
	@printf "VCPKG       : $(VCPKG)\n\n"
