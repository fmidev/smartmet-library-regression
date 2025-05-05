// Copyright (C) 2002, Mikael Kilpelainen. All rights reserved.
//
// Permission to use, copy, modify, and distribute this software for any
// purpose is hereby granted without fee, provided that this copyright and
// permissions notice appear in all copies and derivatives.
//
// This software is provided "as is" without express or implied warranty.


#ifndef TFRAME_HPP
#define TFRAME_HPP

#include <exception>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>

#include <cstdlib>

namespace tframe {

class failed {
	const std::string msg;
	const char* fname;
	const unsigned int linen;
public:
	failed(const char* m, const char* f, unsigned int l) : msg(m), fname(f), linen(l) {}
	failed(const std::string & m, const char* f, unsigned int l) : msg(m), fname(f), linen(l) {}

	unsigned int line() const { return linen; }
	const char* file() const { return fname; }
	const char* what() const { return msg.c_str(); }

};
struct not_implemented {};
struct passed {};

#define TEST_FAILED(a) {throw ::tframe::failed(a, __FILE__, __LINE__);}
#define TEST_PASSED() {throw ::tframe::passed();}
#define TEST_NOT_IMPLEMENTED() {throw ::tframe::not_implemented();}


#define TEST(name) \
		{ \
			out << #name << std::setw(pad-std::string(#name).size()) << std::setfill(fill) << '.'; \
			try { \
				++total; \
				name(); \
				error_in_test(); \
				std::cout << "\n"; \
			} catch(::tframe::passed& e) { \
				passed(e); ++pass; \
			} catch(::tframe::failed& e) { \
				failed(e);  ++fail; \
			} catch(::tframe::not_implemented& e) { \
				not_implemented(e); ++nImpl; \
			} catch(std::exception& e) { \
				uexstd(e); ++fail; \
			} catch(...) { \
				uex(); ++fail; \
			} \
		}

class tests {
protected:
	tests() : out(std::cout), fill('.'), pad(50) {};
	tests(char f, unsigned short p) : out(std::cout), fail(0), pass(0), fill(f), pad(p) {}
	tests(std::ostream& o, char f, unsigned short p) : out(o), fail(0), pass(0), fill(f), pad(p) {}

	std::ostream& out;

	unsigned int total = 0;
	unsigned int fail = 0;
	unsigned int pass = 0;
	unsigned int nImpl = 0;

	char fill;
    unsigned short pad;

	virtual void test() = 0;

	virtual const char* error_message_prefix() const {
		return ": ";
	}
	virtual const char* fail_message() const {
		return "failed";
	}

	virtual void passed(::tframe::passed& e) {
		out << "passed\n";
	}
	virtual void not_implemented(::tframe::not_implemented& e) {
		out << "not implemented\n";
	}

	virtual void failed(::tframe::failed& e) {
		out << fail_message() << error_message_prefix() <<
				e.what() << " - " << e.file() << ": " << e.line() << "\n";
	}
	virtual void uexstd(std::exception& e) {
		out << fail_message() << error_message_prefix() <<
				 "unexpected exception: " << e.what() << "\n";
	}
	virtual void uex() {
		out << fail_message() << error_message_prefix() <<
				"unexpected exception: unknown exception\n";
	}

	virtual void error_in_test() {
		std::cout << "error in test (ending macro missing?)";
	}

	virtual void show_total() {
		std::stringstream str;
		str << "tests: "
			<< pass << " succeeded, " << fail << " failed";
		out << str.str() << std::setw(int(pad-str.str().size()))
                    << std::setfill(fill) << '.';
		if (total == pass + nImpl && !fail) {
			out << "passed\n";
		} else {
			out << fail_message() << "\n";
		}
	}
public:
	virtual ~tests() {}

	int run() {
		test();
		out << "\n";
		show_total();
		return fail != 0 ? EXIT_FAILURE : EXIT_SUCCESS;
	}
};

} //end of namespace

#endif
