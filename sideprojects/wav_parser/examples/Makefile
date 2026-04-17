CFLAGS = -Wall -Wextra -I.. -I/opt/homebrew/include
LDLIBS = -L/opt/homebrew/lib -lsndfile
DEPENDENCIES_DIR = ../../dependencies

read_all:
	rm -f ply_ra_wav_parser
	gcc $(CFLAGS) -o ply_ra_wav_parser read_all.c ../wave.c $(LDLIBS)

read_chunks:
	rm -f ply_rc_wav_parser
	gcc $(CFLAGS) -o ply_rc_wav_parser -DWAV_DEBUG read_chunks.c ../wave.c $(LDLIBS)

decode_encode:
	rm -f ply_de_wav_parser
	gcc $(CFLAGS) -o ply_de_wav_parser decode_encode.c ../wave.c $(LDLIBS)

decode_encode_sanity:
	rm -f ply_de_wav_parser
	gcc $(CFLAGS) -o ply_de_wav_parser decode_encode.c ../wave.c $(LDLIBS)

audio_player:
	rm -f ply_audio_player
	gcc $(CFLAGS) -o ply_audio_player audio_player.c ../wave.c $(DEPENDENCIES_DIR)/miniaudio.c $(LDLIBS)