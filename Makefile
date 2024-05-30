# Makefile

# 컴파일러 설정
CC = gcc
CXX = g++
CFLAGS = -Wall
CXXFLAGS = -Wall
LDFLAGS =

# 대상 파일 설정
TARGET = linetracer
C_SOURCES = linetracer.c
CPP_SOURCES = qr.cpp
C_OBJECTS = $(C_SOURCES:.c=.o)
CPP_OBJECTS = $(CPP_SOURCES:.cpp=.o)
OBJECTS = $(C_OBJECTS) $(CPP_OBJECTS)

# 기본 빌드 규칙
all: $(TARGET)

# 링크 규칙
$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $^ -l wiringPi -O3 `pkg-config --cflags --libs opencv4` -lpthread

# C 파일 컴파일 규칙
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ -O3 -lpthread

# C++ 파일 컴파일 규칙
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ -O3 `pkg-config --cflags --libs opencv4`

# 클린 규칙
clean:
	rm -f $(TARGET) $(OBJECTS)

# 의존성 파일 생성
depend: $(C_SOURCES) $(CPP_SOURCES)
	makedepend $(C_SOURCES) $(CPP_SOURCES) 2>/dev/null

# 의존성 포함
-include .depend

.PHONY: all clean depend
