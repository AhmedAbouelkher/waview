build: clean
	gcc -o waview main.c $$(pkg-config --cflags --libs raylib)


build-brew: clean	
	gcc -o waview main.c $$(/opt/homebrew/bin/pkg-config --cflags --libs raylib)

clean:
	rm -f waview