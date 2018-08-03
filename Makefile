SUBNAME = regression
LIB = smartmet-$(SUBNAME)
SPEC = smartmet-library-$(SUBNAME)
INCDIR = smartmet/$(SUBNAME)

MAINFLAGS = -Wall -W -Wno-unused-parameter

EXTRAFLAGS = -Werror -pedantic -Wpointer-arith -Wcast-qual \
	-Wcast-align -Wwrite-strings -Wconversion -Winline \
	-Wctor-dtor-privacy -Wnon-virtual-dtor -Wno-pmf-conversions \
	-Wsign-promo -Wchar-subscripts -Wold-style-cast

DIFFICULTFLAGS = -Weffc++ -Wredundant-decls -Wshadow -Woverloaded-virtual -Wunreachable-code

CC = g++
CFLAGS = -DUNIX -O0 -g $(MAINFLAGS) $(EXTRAFLAGS) -Werror
ARFLAGS = -r
INCLUDES = 
LIBS =

# Common library compiling template

# Installation directories

processor := $(shell uname -p)

ifeq ($(origin PREFIX), undefined)
  PREFIX = /usr
else
  PREFIX = $(PREFIX)
endif

ifeq ($(processor), x86_64)
  libdir = $(PREFIX)/lib64
else
  libdir = $(PREFIX)/lib
endif

bindir = $(PREFIX)/bin
includedir = $(PREFIX)/include
objdir = obj

# What to install

LIBFILE = libsmartmet-$(SUBNAME).a

# How to install

INSTALL_PROG = install -m 775
INSTALL_DATA = install -m 664

# Compilation directories

vpath %.cpp source
vpath %.h include
vpath %.o $(objdir)

# The files to be compiled

SRCS = $(patsubst source/%,%,$(wildcard source/*.cpp))
HDRS = $(patsubst include/%,%,$(wildcard include/*.h))
OBJS = $(SRCS:%.cpp=%.o)

OBJFILES = $(OBJS:%.o=obj/%.o)

INCLUDES := -Iinclude $(INCLUDES)

.PHONY: test rpm

# The rules

all: objdir $(LIBFILE)
debug: all
release: all
profile: all

$(LIBFILE): $(OBJS)
	$(AR) $(ARFLAGS) $(LIBFILE) $(OBJFILES)

clean:
	rm -f $(LIBFILE) $(OBJFILES) *~ source/*~ include/*~

install:
	@mkdir -p $(includedir)/$(INCDIR)
	@list='$(HDRS)'; \
	for hdr in $$list; do \
	  $(INSTALL_DATA) include/$$hdr $(includedir)/$(INCDIR)/$$hdr; \
	done
	@mkdir -p $(libdir)
	$(INSTALL_DATA) $(LIBFILE) $(libdir)/$(LIBFILE)

test:
	cd test && make test

objdir:
	@mkdir -p $(objdir)

rpm: clean $(SPEC).spec
	rm -f $(SPEC).tar.gz # Clean a possible leftover from previous attempt
	tar -czvf $(SPEC).tar.gz --transform "s,^,$(SPEC)/," *
	rpmbuild -ta $(SPEC).tar.gz
	rm -f $(SPEC).tar.gz

headertest:
	@echo "Checking self-sufficiency of each header:"
	@echo
	@for hdr in $(HDRS); do \
	echo $$hdr; \
	echo "#include \"$$hdr\"" > /tmp/$(SUBNAME).cpp; \
	echo "int main() { return 0; }" >> /tmp/$(SUBNAME).cpp; \
	$(CC) $(CFLAGS) $(INCLUDES) -o /dev/null /tmp/$(SUBNAME).cpp $(LIBS); \
	done

.SUFFIXES: $(SUFFIXES) .cpp

.cpp.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c -o $(objdir)/$@ $<

