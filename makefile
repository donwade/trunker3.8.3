TARGET = mtrunk
#LIBS = -lm -lncurses -lasan
CC = g++


CFLAGS=$(shell pkg-config --cflags librtlsdr libairspy libhackrf ncurses libusb-1.0)

CFLAGS += -g -O0 -fno-omit-frame-pointer -w  -I./include -DBOOL=bool -fpermissive -DTRUE=1 -DFALSE=0

#CFLAGS += -fsanitize=address 

CXXFLAGS=$(CFLAGS)

LIBS=$(shell pkg-config --libs librtlsdr libairspy libhackrf soxr ncurses libusb-1.0 soxr ncurses ) 
LIBS += -lm -lpthread
.PHONY: default all clean

LDFLAGS= -g -L/home/dwade/build.${ARCH}/lib
#LDFLAGS+= -fsanitize=address -static-libasan

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.cpp, %.o, $(wildcard *.cpp))
HEADERS = $(wildcard *.h)

%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC)  $(OBJECTS)  -Wall $(LIBS) $(LDFLAGS)  -o $@

clean:
	-rm -f *.o
	-rm -f $(TARGET)
	rm -f *.bak
	rm -f ASAN*
	rm -f *.bak

