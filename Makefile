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
#   make run                   run last built binary (debug or release, whichever is newer); builds debug if none exists
#   make run-debug             build debug if needed, then run it
#   make run-release           build release if needed, then run it
#   make demo-run              alias for make run
#   make demo-install [pkg…]   add vcpkg dep(s) to demo
#   make demo-search kw        search vcpkg catalog
#   make demo-clean / fclean   remove demo build artifacts
#
# ── Combined ──────────────────────────────────────────────────────────────────
#   make all-debug             engine debug → sync → demo debug
#   make all-release           engine release → sync → demo release
#   make setup                 install engine deps + make engine + demo deps + demo debug
#   make setup-run             setup + run demo
#
# ── Release ───────────────────────────────────────────────────────────────────
#   make publish               bump patch (0.1.0 → 0.1.1), commit, tag, push
#   make publish VERSION=x.y.z bump to exact version, commit, tag, push
#                              Must be on main/master. VERSION must be > current.

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
.PHONY: demo-debug demo-dev demo-release run run-debug run-release demo-run demo-clean demo-fclean
.PHONY: demo-install demo-search

demo-debug:
	$(MAKE) -C demo debug

demo-dev:
	$(MAKE) -C demo dev

demo-release:
	$(MAKE) -C demo release

run:
	$(MAKE) -C demo run

run-debug:
	$(MAKE) -C demo run-debug

run-release:
	$(MAKE) -C demo run-release

demo-run: run

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

# ── Publish ───────────────────────────────────────────────────────────────────
# make publish              — bump patch version (0.1.0 → 0.1.1)
# make publish VERSION=x.y.z — bump to exact version (must be > current)
# Requires: clean working tree, main or master branch.
.PHONY: publish
publish:
	@set -e; \
	CONFIG=Libraries/config.mk; \
	\
	BRANCH=$$(git rev-parse --abbrev-ref HEAD 2>/dev/null); \
	if [ "$$BRANCH" != "main" ] && [ "$$BRANCH" != "master" ]; then \
	    printf "error: publish requires main or master branch (current: %s)\n" "$$BRANCH"; \
	    exit 1; \
	fi; \
	\
	if ! git diff --quiet || ! git diff --cached --quiet; then \
	    printf "error: working tree has uncommitted changes -- commit or stash first\n"; \
	    exit 1; \
	fi; \
	\
	CURRENT=$$(awk '/^ENGINE_VERSION[[:space:]]*\?=/ { \
	    match($$0, /\?=[[:space:]]*/); \
	    print substr($$0, RSTART+RLENGTH) }' "$$CONFIG" | tr -d ' \t\r'); \
	if [ -z "$$CURRENT" ]; then \
	    printf "error: ENGINE_VERSION not found in %s\n" "$$CONFIG"; \
	    exit 1; \
	fi; \
	\
	if [ -n "$(VERSION)" ]; then \
	    NEW="$(VERSION)"; \
	    if ! printf '%s' "$$NEW" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$$'; then \
	        printf "error: VERSION=%s is not valid semver (expected X.Y.Z)\n" "$$NEW"; \
	        exit 1; \
	    fi; \
	    VALID=$$(awk -v cur="$$CURRENT" -v new="$$NEW" 'BEGIN { \
	        split(cur,c,"."); split(new,n,"."); \
	        if (n[1]+0>c[1]+0 || \
	            (n[1]+0==c[1]+0 && n[2]+0>c[2]+0) || \
	            (n[1]+0==c[1]+0 && n[2]+0==c[2]+0 && n[3]+0>c[3]+0)) \
	            print "ok"; else print "fail" }'); \
	    if [ "$$VALID" != "ok" ]; then \
	        printf "error: VERSION=%s is not greater than current %s\n" "$$NEW" "$$CURRENT"; \
	        exit 1; \
	    fi; \
	else \
	    MAJ=$$(printf '%s' "$$CURRENT" | cut -d. -f1); \
	    MIN=$$(printf '%s' "$$CURRENT" | cut -d. -f2); \
	    PAT=$$(printf '%s' "$$CURRENT" | cut -d. -f3); \
	    NEW="$$MAJ.$$MIN.$$((PAT + 1))"; \
	fi; \
	\
	printf "Bumping %s -> %s\n" "$$CURRENT" "$$NEW"; \
	awk -v ver="$$NEW" '/^ENGINE_VERSION[[:space:]]*\?=/ \
	    { sub(/\?=.*/, "?= " ver) } 1' "$$CONFIG" > "$$CONFIG.tmp" \
	    && mv "$$CONFIG.tmp" "$$CONFIG"; \
	\
	git add "$$CONFIG"; \
	git commit -m "update version to $$NEW"; \
	git tag "v$$NEW"; \
	git push; \
	git push origin "v$$NEW"; \
	printf "Published v%s\n" "$$NEW"

# ── Info ──────────────────────────────────────────────────────────────────────
.PHONY: info
info:
	@printf "\n=== Engine ===\n"
	@$(MAKE) -f $(ENGINE_MK) info --no-print-directory
	@printf "=== Demo ===\n"
	@$(MAKE) -C demo info --no-print-directory
	@printf "BUILD_TYPE  : $(BUILD_TYPE)\n"
	@printf "VCPKG       : $(VCPKG)\n\n"
