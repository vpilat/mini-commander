CC = gcc
CFLAGS = -lncurses

mc: mc.c
	$(CC) mc.c cmd.c dialog.c filelist.c init.c panel.c ui.c $(CFLAGS) -o mc

.PHONY: clean

test: mc
	./mc

clean:
	rm -f mc
