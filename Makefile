CFLAGS=-Wall -Wextra
DEBUG_FLAGS=-g -DDEBUG

BIN=hinfosvc

ARCHIVER=zip
ARCHIVE=xmatus36.zip

all: $(BIN)

run: $(BIN)
	./$< 8080

debug: hinfosvc.c
	$(CC) $^ $(DEBUG_FLAGS) -o $(BIN)

pack: hinfosvc.c Readme.md Makefile
	$(ARCHIVER) $(ARCHIVE) $^

clean:
	-rm -f $(BIN)

.PHONY: all run debug pack clean
