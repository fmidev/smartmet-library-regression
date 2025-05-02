SUBNAME = regression
LIB = smartmet-$(SUBNAME)
SPEC = smartmet-library-$(SUBNAME)
INCDIR = smartmet/$(SUBNAME)

include $(shell smartbuildcfg --prefix)/share/smartmet/devel/makefile.inc

# The files to be compiled

HDRS = $(patsubst regression/%,%,$(wildcard regression/*.h))

INCLUDES := -Iregression $(INCLUDES)

.PHONY: test rpm

# The rules

all:
debug: all
release: all
profile: all

clean:
	rm -f *~ source/*~ include/*~
	$(MAKE) -C test clean
	rm -f $(SPEC).tar.gz

install:
	mkdir -p $(includedir)/$(INCDIR)
	list='$(HDRS)'; \
	for hdr in $$list; do \
	  $(INSTALL_DATA) regression/$$hdr $(includedir)/$(INCDIR)/$$hdr; \
	done
	mkdir -p $(libdir)

test:
	cd test && make test

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
	$(CXX) $(CFLAGS) -Iregression -o /dev/null /tmp/$(SUBNAME).cpp $(LIBS); \
	done
