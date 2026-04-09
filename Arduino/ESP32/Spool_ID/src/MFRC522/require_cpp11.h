/**
 * Copyright (c) 2016 by Ludwig Grill (www.rotzbua.de)
 * Throws error if c++11 is not supported
 */
#ifndef REQUIRE_CPP11_H
#define REQUIRE_CPP11_H

#if __cplusplus < 201103L
#error "This library needs at least a C++11 compliant compiler"
#endif

#endif // REQUIRE_CPP11_H
