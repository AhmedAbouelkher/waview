CFLAGS = -Wall -Wextra -I.. -I/opt/homebrew/include
LDLIBS = -L/opt/homebrew/lib -lsndfile

read_all:
	rm -f ply_ra_wav_parser
	gcc $(CFLAGS) -o ply_ra_wav_parser read_all.c ../wave.c $(LDLIBS)

read_chunks:
	rm -f ply_rc_wav_parser
	gcc $(CFLAGS) -o ply_rc_wav_parser -DWAV_DEBUG read_chunks.c ../wave.c $(LDLIBS)
