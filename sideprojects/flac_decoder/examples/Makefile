CC ?= cc
CFLAGS ?= 

TARGET := flac_player
SOURCES := main.c ../flac.c miniaudio.c

.PHONY: build clean

build: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $(TARGET)

clean:
	rm -f $(TARGET)
