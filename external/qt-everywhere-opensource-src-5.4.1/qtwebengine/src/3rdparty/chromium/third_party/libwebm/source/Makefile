CXX       := g++
CXXFLAGS  := -W -Wall -g -MMD -MP
LIBWEBMA  := libwebm.a
LIBWEBMSO := libwebm.so
WEBMOBJS  := mkvparser.o mkvreader.o mkvmuxer.o mkvmuxerutil.o mkvwriter.o
OBJSA     := $(WEBMOBJS:.o=_a.o)
OBJSSO    := $(WEBMOBJS:.o=_so.o)
OBJECTS1  := sample.o
OBJECTS2  := sample_muxer.o vttreader.o webvttparser.o sample_muxer_metadata.o
OBJECTS3  := dumpvtt.o vttreader.o webvttparser.o
OBJECTS4  := vttdemux.o webvttparser.o
INCLUDES  := -I.
DEPS      := $(WEBMOBJS:.o=.d) $(OBJECTS1:.o=.d) $(OBJECTS2:.o=.d)
DEPS      += $(OBJECTS3:.o=.d) $(OBJECTS4:.o=.d)
EXES      := samplemuxer sample dumpvtt vttdemux

all: $(EXES)

sample: sample.o $(LIBWEBMA)
	$(CXX) $^ -o $@

samplemuxer: $(OBJECTS2) $(LIBWEBMA)
	$(CXX) $^ -o $@

dumpvtt: $(OBJECTS3)
	$(CXX) $^ -o $@

shared: $(LIBWEBMSO)

vttdemux: $(OBJECTS4) $(LIBWEBMA)
	$(CXX) $^ -o $@

libwebm.a: $(OBJSA)
	$(AR) rcs $@ $^

libwebm.so: $(OBJSSO)
	$(CXX) $(CXXFLAGS) -shared $(OBJSSO) -o $(LIBWEBMSO)

%.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $< -o $@

%_a.o: %.cpp
	$(CXX) -c $(CXXFLAGS) $(INCLUDES) $< -o $@

%_so.o: %.cpp
	$(CXX) -c $(CXXFLAGS) -fPIC $(INCLUDES) $< -o $@

clean:
	$(RM) -f $(OBJECTS1) $(OBJECTS2) $(OBJECTS3) $(OBJECTS4) $(OBJSA) $(OBJSSO) $(LIBWEBMA) $(LIBWEBMSO) $(EXES) $(DEPS) Makefile.bak

ifneq ($(MAKECMDGOALS), clean)
  -include $(DEPS)
endif
