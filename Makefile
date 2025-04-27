# Makefile for Florb

CXX = g++
CXXFLAGS = -std=c++17 -O2
LIBS = -lGL -lGLEW -lX11
TARGET = florb
SRC = florb.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)
