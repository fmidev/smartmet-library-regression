
test: TestRegression
	./TestRegression
	$(MAKE) -C imgdiff test

TestRegression: TestRegression.cpp
	$(CXX) -I.. -Wall -o TestRegression TestRegression.cpp

clean:
	rm -f TestRegression
	$(MAKE) -C imgdiff clean
