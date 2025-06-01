# Makefile for Florb
#
# Author : Eldridge M. Mount IV
# Email  : eldridge.mount@gmail.com

CXX = g++

CXXFLAGS = -std=c++17 -Wall -O2

LIBS = GL GLEW GLU glfw dl X11 pthread

IMGUI_DIR = imgui

INC_DIRS  = include
INC_DIRS += $(IMGUI_DIR)

SOURCES  = main.cpp
SOURCES += Camera.cpp
SOURCES += Dashboard.cpp
SOURCES += Florb.cpp
SOURCES += FlorbConfigs.cpp
SOURCES += Flower.cpp
SOURCES += FlorbUtils.cpp
SOURCES += LinearMotion.cpp
SOURCES += MotionAlgorithm.cpp
SOURCES += MultiMotion.cpp
SOURCES += SinusoidalMotion.cpp
SOURCES += Spotlight.cpp

IMGUI_SOURCES  = imgui.cpp
IMGUI_SOURCES += imgui_draw.cpp
IMGUI_SOURCES += imgui_widgets.cpp
IMGUI_SOURCES += imgui_tables.cpp
IMGUI_SOURCES += imgui_impl_glfw.cpp
IMGUI_SOURCES += imgui_impl_opengl3.cpp

OBJS = $(SOURCES:%.cpp=%.o) $(IMGUI_SOURCES:%.cpp=%.o)
DEPFILES = ${SOURCES:%.cpp=%.d}

HEADERS  = Camera.h
HEADERS += Florb.h
HEADERS += FlorbConfigs.h
HEADERS += FlorbUtils.h
HEADERS += Flower.h
HEADERS += LinearMotion.h
HEADERS += MotionAlgorithm.h
HEADERS += MultiMotion.h
HEADERS += SinusoidalMotion.h
HEADERS += Spotlight.h

CONFIG = $(TARGET).json

MAKEFILE = Makefile

MAKEFLAGS += -r

TARGET = florb

TARBALL = $(TARGET).tgz

BATCH_METADATA_SCRIPT = scripts/write_image_metadata.sh
METADATA_SCRIPT = scripts/pngmeta.py
FLOWERS_PATH = flowers
DOMESTIC_PATH = $(FLOWERS_PATH)/domestic
WILD_PATH = $(FLOWERS_PATH)/wild

VPATH = $(IMGUI_DIR) $(IMGUI_DIR)/backends $(INC_DIRS)

all: $(TARGET)

%o: %cpp
	$(CXX) $(CXXFLAGS) $(INC_DIRS:%=-I%) -c -o $@ $<

%.d: %.cpp
	$(CXX) -MM $(CXXFLAGS) $(INC_DIRS:%=-I%) $< | sed -e 's,\($*\)\.o[ :]*,\1.o $@: ,g' > $@

$(TARGET): $(OBJS) $(MAKEFILE)
	$(CXX) $(CXXFLAGS) $(INC_DIRS:%=-I%) -o $(TARGET) $(OBJS) $(LIBS:%=-l%)

.PHONY: $(TARBALL)
$(TARBALL): $(SOURCES) $(HEADERS) $(MAKEFILE) $(CONFIG)
	tar czf $@ $^

.PHONY: images_metadata
images_metadata:
	$(BATCH_METADATA_SCRIPT) $(WILD_PATH)/metadata $(WILD_PATH) $(METADATA_SCRIPT)

clean:
	rm -f $(TARGET) $(OBJS) $(DEPFILES) $(TARBALL)

-include ${DEPFILES}
