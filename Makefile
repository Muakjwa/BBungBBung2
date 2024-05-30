CC = gcc
CXX = g++
CFLAGS = -Wall
CXXFLAGS = -Wall

TARGET = linetracer
C_SOURCES = linetracer.c
CPP_SOURCES = qr.cpp
C_OBJECTS = $(C_SOURCES:.c=.o)
CPP_OBJECTS = $(CPP_SOURCES:.cpp=.o)
OBJECTS = $(C_OBJECTS) $(CPP_OBJECTS)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $^ -l wiringPi -O3 `pkg-config --cflags --libs opencv4` -lpthread

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ -O3 -lpthread

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ -O3 `pkg-config --cflags --libs opencv4`

clean:
	rm -f $(TARGET) $(OBJECTS)

depend: $(C_SOURCES) $(CPP_SOURCES)
	makedepend $(C_SOURCES) $(CPP_SOURCES) 2>/dev/null

-include .depend

.PHONY: all clean depend
