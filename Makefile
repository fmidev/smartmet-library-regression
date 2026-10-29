SUBNAME = regression
LIB = smartmet-$(SUBNAME)
SPEC = smartmet-library-$(SUBNAME)
INCDIR = smartmet/$(SUBNAME)

REQUIRES = filesystem

include $(or $(PREFIX),$(shell smartbuildcfg --prefix))/share/smartmet/devel/makefile.inc

mandir ?= $(PREFIX)/share/man

MAGICK_CFLAGS := $(shell pkg-config --cflags Magick++)
MAGICK_LIBS := $(shell pkg-config --libs Magick++)

CFLAGS += $(MAGICK_CFLAGS)

LIBFILE = lib$(LIB).so
PROGS = smartimagediff_psnr smartimagediff

HDRS = $(patsubst regression/%,%,$(wildcard regression/*.h))

SRCS = $(wildcard regression/*.cpp)
OBJS = $(patsubst regression/%.cpp, obj/%.o, $(SRCS))

INCLUDES := -Iregression $(INCLUDES) $(MAGICK_CFLAGS)

.PHONY: test rpm

# The rules

all: objdir $(LIBFILE) $(PROGS)
debug: all
release: all
profile: all

$(LIBFILE): $(OBJS)
	$(CXX) $(LDFLAGS) -shared -rdynamic $(REQUIRED_LIBS) -o $(LIBFILE) $(OBJS) $(MAGICK_LIBS)
	@echo Checking $(LIBFILE) for unresolved references
	@if ldd -r $(LIBFILE) 2>&1 | c++filt | grep ^undefined\ symbol; \
	then \
		rm -v $(LIBFILE); \
		exit 1; \
	fi

$(PROGS): %: main/%.cpp $(LIBFILE)
	$(CXX) $(CFLAGS) $(INCLUDES) -o $@ $< -L. -l$(LIB) $(REQUIRED_LIBS) '-Wl,-rpath,$(libdir)' $(MAGICK_LIBS)

clean:
	rm -f *~ source/*~ include/*~ $(LIBFILE) $(PROGS)
	$(MAKE) -C test clean
	rm -f $(SPEC).tar.gz
	rm -rf $(objdir)

install:
	mkdir -p $(includedir)/$(INCDIR)
	list='$(HDRS)'; \
	for hdr in $$list; do \
	  $(INSTALL_DATA) regression/$$hdr $(includedir)/$(INCDIR)/$$hdr; \
	done
	mkdir -p $(libdir)
	$(INSTALL_PROG) $(LIBFILE) $(libdir)/$(LIBFILE)
	mkdir -p $(bindir)
	for prog in $(PROGS); do \
	  $(INSTALL_PROG) $$prog $(bindir)/$$prog; \
	done
	$(INSTALL_PROG) smartpngdiff $(bindir)/smartpngdiff
	mkdir -p $(mandir)/man1
	@for page in docs/man/*.1; do \
	  base=`basename $$page`; \
	  echo $(INSTALL_DATA) $$page $(mandir)/man1/$$base; \
	  $(INSTALL_DATA) $$page $(mandir)/man1/$$base; \
	  gzip -nf $(mandir)/man1/$$base; \
	done

test: $(PROGS) $(LIBFILE)
	$(MAKE) -C test test

rpm: clean $(SPEC).spec
	rm -f $(SPEC).tar.gz # Clean a possible leftover from previous attempt
	tar -czvf $(SPEC).tar.gz --transform "s,^,$(SPEC)/," *
	rpmbuild -tb $(SPEC).tar.gz
	rm -f $(SPEC).tar.gz

headertest:
	@echo "Checking self-sufficiency of each header:"
	@echo
	@for hdr in $(HDRS); do \
	echo $$hdr; \
	echo "#include \"$$hdr\"" > /tmp/$(SUBNAME).cpp; \
	echo "int main() { return 0; }" >> /tmp/$(SUBNAME).cpp; \
	$(CXX) $(CFLAGS) $(INCLUDES) -o /dev/null /tmp/$(SUBNAME).cpp $(MAGICK_LIBS) $(LIBS); \
	done

objdir:
	@mkdir -p $(objdir)

obj/%.o: regression/%.cpp
	@mkdir -p $(objdir)
	$(CXX) $(CFLAGS) $(INCLUDES) -c -MD -MF $(patsubst obj/%.o, obj/%.d, $@) -MT $@ -o $@ $<

ifneq ($(wildcard obj/*.d),)
-include $(wildcard obj/*.d)
endif
