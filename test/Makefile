MAGICK_CFLAGS := $(shell pkg-config --cflags Magick++)
MAGICK_LIBS := $(shell pkg-config --libs Magick++)

test: TestRegression StructuralImageDiffTest
	./TestRegression
	./StructuralImageDiffTest
	$(MAKE) -C imgdiff test

TestRegression: TestRegression.cpp
	$(CXX) -I.. -Wall -o TestRegression TestRegression.cpp

StructuralImageDiffTest: StructuralImageDiffTest.cpp ../libsmartmet-regression.so
	$(CXX) -std=c++17 -I.. $(MAGICK_CFLAGS) -Wall -o StructuralImageDiffTest StructuralImageDiffTest.cpp \
	  -L.. -lsmartmet-regression -lstdc++fs -Wl,-rpath,'$$ORIGIN/..' $(MAGICK_LIBS)

clean:
	rm -f TestRegression StructuralImageDiffTest
	$(MAKE) -C imgdiff clean
