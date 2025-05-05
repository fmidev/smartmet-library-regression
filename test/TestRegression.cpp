#include <regression/tframe.h>

void test1() {
  // Simulate a test that passes
  TEST_PASSED();
}

class TestRegression1 : public ::tframe::tests {
public:
  TestRegression1() : ::tframe::tests('.', 50) {}
  TestRegression1(std::ostream& o) : ::tframe::tests(o, '.', 50) {}

  void test() override {
    TEST(test1);
  }
};

void test2a() {
  // Simulate a test that fails
  TEST_FAILED("Failed test");
}

void test2b() {
  // Test case 3
  // Simulate a test that is not implemented
  TEST_PASSED();
}

class TestRegression2 : public ::tframe::tests {
public:
  TestRegression2() : ::tframe::tests('.', 50) {}
  TestRegression2(std::ostream& o) : ::tframe::tests(o, '.', 50) {}

  void test() override {
    TEST(test2a);
    TEST(test2b);
  }
};

void test3a() {
  TEST_NOT_IMPLEMENTED();
}

void test3b() {
  TEST_NOT_IMPLEMENTED();
}


class TestRegression3 : public ::tframe::tests {
public:
  TestRegression3() : ::tframe::tests('.', 50) {}
  TestRegression3(std::ostream& o) : ::tframe::tests(o, '.', 50) {}

  void test() override {
    TEST(test3a);
    TEST(test3b);
  }
};

int main(int argc,char **argv) {
  bool ok = true;

  std::cout << "\n### Testing passing test\n";
  TestRegression1 test1;
  if (test1.run() != EXIT_SUCCESS)
  {
    ok = false;
    std::cerr << "*** Test 1 failed even if should have passed\n";
  }

  std::cout << "\n### Testing failing test\n";
  TestRegression2 test2;
  if (test2.run() != EXIT_FAILURE)
  {
    ok = false;
    std::cerr << "*** Test 2 failed (only unimplemented tests)\n";
  }

  std::cout << "\n### Testing unimplemented tests\n";
  TestRegression3 test3;
  if (test3.run() != EXIT_SUCCESS)
  {
    ok = false;
    std::cerr << "*** Test 3 failed even if should have passed, when there is only not implemented tests\n";
  }

  return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
