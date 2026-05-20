APP_NAME = Waview
BUILD_DIR = build
APP_BUNDLE = $(BUILD_DIR)/$(APP_NAME).app
CONTENTS = $(APP_BUNDLE)/Contents
MACOS = $(CONTENTS)/MacOS
RESOURCES = $(CONTENTS)/Resources
RAYLIB_STATIC = /opt/homebrew/lib/libraylib.a
FRAMEWORKS = -framework CoreVideo -framework IOKit -framework Cocoa -framework OpenGL

WEB_BUILD_DIR = $(BUILD_DIR)/web
RAYLIB_WEB_PATH = ./raylib-web
RAYLIB_WEB_FLAGS = $(RAYLIB_WEB_PATH)/lib/libraylib.a -I$(RAYLIB_WEB_PATH)/include
BUILD_WEB_RESOURCES_PATH  ?= $(dir $<)resources@resources
BUILD_WEB_SHELL       ?= minshell.html

.PHONY: clean build macos

build:
	rm -f $(BUILD_DIR)/waview
	mkdir -p $(BUILD_DIR)
	gcc -o $(BUILD_DIR)/waview main.c $$(pkg-config --cflags --libs raylib)

build-brew: clean	
	gcc -o waview main.c $$(/opt/homebrew/bin/pkg-config --cflags --libs raylib)

clean:
	rm -f $(BUILD_DIR)/waview
	rm -rf $(APP_BUNDLE)

build-web:
	mkdir -p $(WEB_BUILD_DIR)
	emcc -o $(WEB_BUILD_DIR)/waview.html main.c -Os -Wall -DPLATFORM_WEB \
		$(RAYLIB_WEB_FLAGS) -sUSE_GLFW=3 -sASYNCIFY -sFORCE_FILESYSTEM=1 -sMINIFY_HTML=1 \
		--preload-file $(BUILD_WEB_RESOURCES_PATH) \
		--shell-file $(BUILD_WEB_SHELL)
		
build-web-deploy: build-web
	rm -rf ./docs
	cp -r $(WEB_BUILD_DIR) ./docs
	cp $(WEB_BUILD_DIR)/waview.html ./docs/index.html
	rm -rf ./docs/waview.html