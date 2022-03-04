CFLAGS=-Wall -Wextra

TARGET=hinfosvc

ARCHIVER=zip
ARCHIVE=xmatus36.zip

all: $(TARGET)

run: $(TARGET)
	./$< 8080

pack: hinfosvc.c readme.md Makefile
	$(ARCHIVER) $(ARCHIVE) $^

clean:
	-rm -f $(TARGET)

.PHONY: all run pack clean
