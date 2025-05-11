# Makefile for Florb
#
# Author : Eldridge M. Mount IV
# Email  : eldridge.mount@gmail.com

CXX = g++

CXXFLAGS = -std=c++17 -Wall -O2

LIBS = GL GLEW GLU X11

INC_DIRS = include

SOURCES  = main.cpp
SOURCES += Camera.cpp
SOURCES += Florb.cpp
SOURCES += FlorbConfigs.cpp
SOURCES += Flower.cpp
SOURCES += FlorbUtils.cpp
SOURCES += MotionAlgorithm.cpp
SOURCES += SinusoidalMotion.cpp
SOURCES += Spotlight.cpp

OBJS = $(SOURCES:%.cpp=%.o)

HEADERS  = Camera.h
HEADERS += Florb.h
HEADERS += FlorbConfigs.h
HEADERS += FlorbUtils.h
HEADERS += Flower.h
HEADERS += MotionAlgorithm.h
HEADERS += SinusoidalMotion.h
HEADERS += Spotlight.h

MAKEFILE = Makefile

MAKEFLAGS += -r

TARGET = florb

VPATH = $(INC_DIRS)

all: $(TARGET)

%o: %cpp
	$(CXX) $(CXXFLAGS) $(INC_DIRS:%=-I%) -c -o $@ $<

$(TARGET): $(OBJS) $(MAKEFILE)
	$(CXX) $(CXXFLAGS) $(INC_DIRS:%=-I%) -o $(TARGET) $(SOURCES) $(LIBS:%=-l%)

clean:
	rm -f $(TARGET) $(OBJS)
