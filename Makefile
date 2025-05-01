# Makefile for Florb

CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
LIBS = GL GLEW GLU X11
INC_DIRS = include

SRC = main.cpp Florb.cpp florbUtils.cpp
HEADERS = Florb.h
TARGET = florb

VPATH = $(INC_DIRS)

all: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INC_DIRS:%=-I%) -o $(TARGET) $(SRC) $(LIBS:%=-l%)

clean:
	rm -f $(TARGET)
