# Makefile for Florb

CXX = g++
CXXFLAGS = -std=c++17 -O2
LIBS = -lGL -lGLEW -lX11
TARGET = florb
SRC = florb.cpp

INC_DIRS=include

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(INC_DIRS:%=-I%) -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)
