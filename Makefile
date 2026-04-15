APP_NAME = Waview
BUILD_DIR = build
APP_BUNDLE = $(BUILD_DIR)/$(APP_NAME).app
CONTENTS = $(APP_BUNDLE)/Contents
MACOS = $(CONTENTS)/MacOS
RESOURCES = $(CONTENTS)/Resources
RAYLIB_STATIC = /opt/homebrew/lib/libraylib.a
FRAMEWORKS = -framework CoreVideo -framework IOKit -framework Cocoa -framework OpenGL

.PHONY: clean build macos

build:
	rm -f $(BUILD_DIR)/waview
	mkdir -p $(BUILD_DIR)
	gcc -o $(BUILD_DIR)/waview main.c $$(pkg-config --cflags --libs raylib)

macos:
	rm -rf $(APP_BUNDLE)
	mkdir -p $(MACOS) $(RESOURCES)
	clang -o $(MACOS)/waview main.c $$(pkg-config --cflags raylib) $(RAYLIB_STATIC) $(FRAMEWORKS)
	cp Info.plist.template $(CONTENTS)/Info.plist

clean:
	rm -f $(BUILD_DIR)/waview
	rm -rf $(APP_BUNDLE)