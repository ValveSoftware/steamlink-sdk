SOURCES = d3d12.cpp
CONFIG -= qt dylib
CONFIG += console
LIBS += -ldxgi -ld3d12 -ld3dcompiler -ldcomp
