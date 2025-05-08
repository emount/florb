# Makefile for Florb
#
# Author : Eldridge M. Mount IV
# Email  : eldridge.mount@gmail.com

CXX = g++

CXXFLAGS = -std=c++17 -Wall -O2

LIBS = GL GLEW GLU X11 png

INC_DIRS = include

SOURCES  = main.cpp
SOURCES += Florb.cpp
SOURCES += Flower.cpp
SOURCES += FlorbUtils.cpp
SOURCES += SinusoidalMotion.cpp

HEADERS  = Florb.h
HEADERS += FlorbUtils.h
HEADERS += Flower.h
HEADERS += MotionAlgorithm.h
HEADERS += SinusoidalMotion.h

TARGET = florb

VPATH = $(INC_DIRS)

all: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(INC_DIRS:%=-I%) -o $(TARGET) $(SOURCES) $(LIBS:%=-l%)

clean:
	rm -f $(TARGET)
