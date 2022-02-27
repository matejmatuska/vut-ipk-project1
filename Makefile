CFLAGS= -Wall -Wextra

TARGET=hinfosvc

hinfosvc: hinfosvc.c

run: $(TARGET)
	./$(TARGET) 8080

pack: $(TARGET)
	zip xmatus36.zip $^

clean:
	-rm -f $(TARGET)

.PHONY: run pack clean
