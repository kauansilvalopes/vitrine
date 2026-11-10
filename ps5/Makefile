# Boxshelf PS5 - Linux/WSL build entry points.
# Build system from ps5-native-app-boilerplate (Copyright (C) 2026 BlackBearReloaded), via Estante.
# SPDX-License-Identifier: GPL-3.0-or-later

SHELL := /bin/bash
.DEFAULT_GOAL := app

-include .env

APP_DEFINITIONS ?=
APP_INCLUDE_PATHS ?=
APP_STATIC_ARCHIVES ?=
APP_IMPORT_STUBS ?=
APP_RUNTIME_MODULES ?=
# Empty selects the pinned ps5-opengl release (tools/fetch-opengl-sdk.sh).
PS5_OPENGL_PREFIX ?=

# Always built against ps5-opengl (the GPU renderer); user APP_* values append.
OPENGL_SDK := .deps/ps5-opengl/current
override APP_DEFINITIONS := $(strip GL_GLEXT_PROTOTYPES=1 BOXSHELF_GL=1 $(APP_DEFINITIONS))
override APP_INCLUDE_PATHS := $(strip src $(OPENGL_SDK)/include $(APP_INCLUDE_PATHS))
override APP_STATIC_ARCHIVES := $(strip .deps/ps5-opengl/libps5opengl-group.a $(APP_STATIC_ARCHIVES))
override APP_IMPORT_STUBS := $(strip $(OPENGL_SDK)/lib/libSceAgc.so \
	$(OPENGL_SDK)/lib/libSceAgcDriver.so $(APP_IMPORT_STUBS))
APP_WRAP_SYMBOLS ?=
# HTTPS (box art downloads): PacBrew's libcurl + OpenSSL, run through src/net/console_curl.c (from Estante's
# update-check example, docs/CURL.md there): fcntl must be wrapped in a sandboxed title.
override PACBREW_PACKAGES := $(strip libcurl $(PACBREW_PACKAGES))
override APP_WRAP_SYMBOLS := $(strip fcntl $(APP_WRAP_SYMBOLS))
APP_SOURCE_DIR ?=
APP_PARAM ?= sce_sys/param.json
APP_SCE_SYS ?= sce_sys
APP_ASSETS ?= assets
APP_ROOT_FILES ?=
APP_LAPY_HELPER ?= 0
PACBREW_INCLUDE_PATHS ?=
PACBREW_STATIC_ARCHIVES ?=
PS5_HOST ?=
FTP_PORT ?= 2121
PS5_FTP_USER ?= anonymous
PS5_FTP_PASSWORD ?= anonymous
DEPLOY_DRY_RUN ?= 0
BUILD_JOBS ?= $(shell nproc 2>/dev/null || echo 2)
USE_CCACHE ?= 1
export BUILD_JOBS USE_CCACHE
export APP_DEFINITIONS APP_INCLUDE_PATHS APP_STATIC_ARCHIVES APP_IMPORT_STUBS APP_RUNTIME_MODULES APP_WRAP_SYMBOLS
export PS5_OPENGL_PREFIX
export APP_SOURCE_DIR APP_PARAM APP_SCE_SYS APP_ASSETS APP_ROOT_FILES
export APP_LAPY_HELPER
export PACBREW_PACKAGES PACBREW_INCLUDE_PATHS PACBREW_STATIC_ARCHIVES
export PS5_HOST FTP_PORT PS5_FTP_USER PS5_FTP_PASSWORD DEPLOY_DRY_RUN

RUNTIME := runtime/libc.prx
RUNTIME_INPUTS := tools/rebuild-libc.sh tools/build-host-tools.sh tools/ninja-build.sh \
	$(wildcard tooling/native/*.cpp tooling/native/*.hpp) \
	$(wildcard tooling/native/runtime/*.txt)

.PHONY: all app build opengl doctor test preview library library-tool libc assets-check deploy undeploy clean distclean help

all: app
build: app

doctor:
	@printf '%s\n' '==> [doctor] Checking the Linux/WSL host without changing it'
	@bash tools/doctor.sh

test:
	@bash tools/test-boxshelf.sh

library-tool:
	@mkdir -p build/tools
	@clang-18 -O1 -c src/cJSON.c -o build/tools/cjson.o
	@clang++-18 -std=c++20 -O1 -Wall -Wextra -Isrc tools/list-library.cpp src/data/*.cpp build/tools/cjson.o \
		-o build/tools/list-library

# Read-only: saves /appdb, emulator configs and folder listings from the console, then prints the shelves.
library: library-tool
	@python3 tools/fetch-listings.py --host $(PS5_HOST)
	@build/tools/list-library .local/listings

preview: opengl
	@bash tools/preview-boxshelf-gl.sh $(MODES)

assets-check:
	@bash tools/validate-assets.sh

libc:
	@bash tools/rebuild-libc.sh

$(RUNTIME): $(RUNTIME_INPUTS)
	@printf '%s\n' '==> [libc] Generating the missing or outdated runtime'
	@bash tools/rebuild-libc.sh

opengl:
	@printf '%s\n' '==> [opengl] Preparing the ps5-opengl SDK link group'
	@bash tools/prepare-opengl.sh

app: $(RUNTIME) opengl
	@printf '%s\n' '==> [app] Compiling, linking, signing, and assembling the app folder'
	@bash tools/build.sh Folder

deploy:
	@printf '%s\n' '==> [deploy] Building and publishing the app folder over FTP'
	@bash tools/deploy.sh

undeploy:
	@printf '%s\n' '==> [undeploy] Removing this title from /data/homebrew over FTP'
	@bash tools/deploy.sh undeploy

clean:
	@rm -rf -- build dist
	@rm -f -- $(RUNTIME)

distclean: clean
	@rm -rf -- .deps

help:
	@printf '%s\n' \
	  'make                 Build the app folder dist/<TITLE_ID>/ (and its ZIP)' \
	  'make preview         Render screens on the PC (Mesa EGL) to .local/gl-*.png [MODES="..."]' \
	  'make test            Run the host unit tests' \
	  'make doctor          Check required Linux/WSL tools' \
	  'make deploy PS5_HOST=<address>    Build and FTP-deploy (only with the owner'"'"'s go-ahead)' \
	  'make undeploy PS5_HOST=<address>  Remove this title from /data/homebrew' \
	  'make clean / distclean'
