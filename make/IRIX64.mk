
VARIANT     = n64
DSO_SUFFIX  = so
DSO_LDFLAGS = -shared

CC          = cc
CXX         = CC
CXX_DEPS    = g++
AR          = CC -ar -o
DOXYGEN     = ls

CXXFLAGS   += -LANG:std -64
LDFLAGS    += -64
