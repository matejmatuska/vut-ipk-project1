CFLAGS= -Wall -g 

TARGET=hinfosvc

run: $(TARGET)
	./$(TARGET) 8080

clean:
	-rm -f $(TARGET)
