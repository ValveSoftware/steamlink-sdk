CC=gcc
CFLAGS= -g -Wall -std=c99
CPP=g++
CPPFLAGS= -g3 -Wall -lpthread -D___BUILD_MODEL___

PINYINIME_DICTBUILDER=pinyinime_dictbuilder

LIBRARY_SRC= \
	    ../share/dictbuilder.cpp \
	    ../share/dictlist.cpp \
	    ../share/dicttrie.cpp \
	    ../share/lpicache.cpp \
	    ../share/mystdlib.cpp \
	    ../share/ngram.cpp \
	    ../share/searchutility.cpp \
	    ../share/spellingtable.cpp \
	    ../share/spellingtrie.cpp \
	    ../share/splparser.cpp \
	    ../share/utf16char.cpp \
	    ../share/utf16reader.cpp \

all: engine

engine: $(PINYINIME_DICTBUILDER)

$(PINYINIME_DICTBUILDER): $(LIBRARY_SRC) pinyinime_dictbuilder.cpp
	@$(CPP) $(CPPFLAGS) -o $@ $?


clean:
	-rm -rf $(PINYINIME_DICTBUILDER)

.PHONY: clean
