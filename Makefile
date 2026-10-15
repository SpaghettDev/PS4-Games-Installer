SHELL := /bin/bash
.DEFAULT_GOAL := all
IMAGE ?= ps4-games-installer:build
export DOTNET_SYSTEM_GLOBALIZATION_INVARIANT := 1

.PHONY: all image test check preview screenshots clean validate package package-internal assets
image:
	docker build --label org.psgi.dockerfile="$$(sha256sum Dockerfile | cut -d ' ' -f1)" -t $(IMAGE) .

ifneq ($(PSGI_IN_CONTAINER),1)
all package test check preview screenshots validate:
	@test "$$(docker image inspect $(IMAGE) --format '{{index .Config.Labels "org.psgi.dockerfile"}}' 2>/dev/null)" = "$$(sha256sum Dockerfile | cut -d ' ' -f1)" || $(MAKE) image
	docker run --rm --user "$$(id -u):$$(id -g)" -v "$(CURDIR):/work" \
      -e SDL_VIDEODRIVER=dummy $(IMAGE) make $@ CONFIG="$(CONFIG)"
clean:
	rm -rf build dist
else
SDK := $(OO_PS4_TOOLCHAIN)
TOOLS := $(SDK)/bin/linux
TITLE_ID := PGIN00001
CONTENT_ID := IV0000-$(TITLE_ID)_00-PS4GAMEINSTALLER
CROSSFLAGS := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -D_POSIX_C_SOURCE=200809L \
    -isysroot $(SDK) -isystem $(SDK)/include -Isrc -Ithird_party/cjson -Ithird_party/stb -Ibuild/generated -O2 -g -Wall -Wextra -MMD -MP
COMMON := catalog settings library sfo installed ui
UI_SOURCES := canvas artwork render app demo video main
CONSOLE := $(UI_SOURCES) http install console
PS4_OBJECTS := $(addprefix build/ps4/,$(addsuffix .o,$(COMMON) $(CONSOLE))) build/ps4/cJSON.o
LIBS := -ljbc -lc++ -lSceVideoOut -lSceSysmodule -lSceFreeType -lScePad -lSceUserService -lSceNet -lSceNetCtl -lSceSsl -lSceHttp -lSceAppInstUtil -lSceSystemService -lSceCommonDialog -lSceImeDialog -lc -lkernel
HOSTFLAGS := -std=c++17 -DPSGI_HOST -Isrc -Ithird_party/cjson -Ithird_party/stb -Ibuild/generated -Wall -Wextra -Werror -O1 -g -pthread
HEADERS := $(wildcard src/*.hpp) third_party/cjson/cJSON.h third_party/stb/stb_image.h

all package: assets
	python3 scripts/package.py

assets: assets/icon.png
assets/icon.png: assets/icon.svg
	rsvg-convert $< -o build-icon.tmp
	python3 -c 'from PIL import Image; Image.open("build-icon.tmp").convert("RGB").save("assets/icon.png")'
	rm build-icon.tmp

build/ps4/%.o: src/%.cpp
	@mkdir -p $(@D)
	clang++ $(CROSSFLAGS) -std=c++17 -isystem $(SDK)/include/c++/v1 -c $< -o $@
build/ps4/cJSON.o: third_party/cjson/cJSON.c
	@mkdir -p $(@D)
	clang $(CROSSFLAGS) -c $< -o $@

build/app.elf: $(PS4_OBJECTS)
	ld.lld -m elf_x86_64 -pie --script $(SDK)/link.x --eh-frame-hdr -L$(SDK)/lib -o $@ $(SDK)/lib/crt1.o $(PS4_OBJECTS) $(LIBS)

build/pkg/eboot.bin: build/app.elf
	@mkdir -p $(@D)
	$(TOOLS)/create-fself -in=$< -out=build/app.oelf --eboot $@ --paid 0x3800000000000011

package-internal: build/pkg/eboot.bin assets
	python3 scripts/metadata.py "$(SDK)" "$(REVISION)" "$(CONTENT_ID)" "$(TITLE_ID)"
	cd build/pkg && $(TOOLS)/PkgTool.Core pkg_build pkg.gp4 ../package
	$(TOOLS)/PkgTool.Core pkg_validate build/package/$(CONTENT_ID).pkg

build/host/cJSON.o: third_party/cjson/cJSON.c
	@mkdir -p $(@D)
	gcc -O1 -g -c $< -o $@
build/host/catalog-test: tests/catalog_test.cpp src/catalog.cpp build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) tests/catalog_test.cpp src/catalog.cpp build/host/cJSON.o -o $@
build/host/catalog-check: src/check.cpp src/catalog.cpp src/settings.cpp src/http.cpp build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) src/check.cpp src/catalog.cpp src/settings.cpp src/http.cpp build/host/cJSON.o -lcurl -o $@

build/generated/revision.hpp:
	@mkdir -p $(@D)
	python3 -c 'import json; from pathlib import Path; r=json.load(open("revision.json"))["revision"]; Path("$@").write_text("#pragma once\n#define PSGI_REVISION %d\n#define PSGI_VERSION \"r%d\"\n" % (r,r))'

build/host/preview: $(addprefix src/,$(addsuffix .cpp,$(COMMON) $(UI_SOURCES) http desktop)) build/host/cJSON.o build/generated/revision.hpp $(HEADERS)
	g++ $(HOSTFLAGS) -Wno-unused-function $$(pkg-config --cflags freetype2 sdl2) $(filter %.cpp %.o,$^) -lcurl $$(pkg-config --libs freetype2 sdl2) -o $@
build/demo/0.png: scripts/demo_assets.py assets/font.ttf
	python3 scripts/demo_assets.py
screenshots preview: assets build/host/preview build/demo/0.png
	./build/host/preview --capture
	python3 scripts/screenshots.py
	@echo 'Screenshots: build/screenshots/ — see README for interactive preview controls.'

build/host/model-test: tests/model_test.cpp $(addprefix src/,$(addsuffix .cpp,$(COMMON))) build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) $(filter %.cpp %.o,$^) -o $@
build/host/ui-test: tests/ui_test.cpp $(addprefix src/,$(addsuffix .cpp,$(COMMON))) build/host/cJSON.o $(HEADERS)
	g++ $(HOSTFLAGS) $(filter %.cpp %.o,$^) -o $@
test: build/host/catalog-test build/host/catalog-check build/host/model-test build/host/ui-test
	./build/host/catalog-test
	./build/host/model-test
	./build/host/ui-test
	python3 tests/http_test.py ./build/host/catalog-check
	python3 -m unittest discover -s tests -p '*_test.py'
check: build/host/catalog-check
	@test -n "$(CONFIG)" || { echo 'Use make check CONFIG=/work/config.json'; exit 1; }
	./build/host/catalog-check "$(CONFIG)"
validate:
	python3 scripts/package.py --validate
clean:
	rm -rf build dist
-include $(wildcard build/ps4/*.d)
endif
