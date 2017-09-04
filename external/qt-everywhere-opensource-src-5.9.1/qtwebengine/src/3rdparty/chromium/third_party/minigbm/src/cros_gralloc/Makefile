# Copyright 2016 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

GRALLOC = gralloc.cros.so

SOURCES =  $(wildcard *.cc)
SOURCES += $(wildcard ../*.c)

VPATH = $(dir $(SOURCES))

CPPFLAGS += -Wall -fPIC -Werror -flto
CXXFLAGS += -std=c++11
LIBS     += -shared -lcutils -lhardware -ldrm

OBJS =  $(foreach source, $(SOURCES), $(addsuffix .o, $(basename $(source))))

OBJECTS = $(addprefix $(TARGET_DIR), $(notdir $(OBJS)))
LIBRARY = $(addprefix $(TARGET_DIR), $(GRALLOC))

.PHONY: all clean

all: $(LIBRARY)

$(LIBRARY): $(OBJECTS)

clean:
	$(RM) $(LIBRARY)
	$(RM) $(OBJECTS)

$(LIBRARY):
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@ $(LIBS)

$(TARGET_DIR)%.o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $^ -o $@ -MMD

$(TARGET_DIR)%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $^ -o $@ -MMD
