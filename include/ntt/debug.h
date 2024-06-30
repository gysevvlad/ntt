#pragma once

#ifndef NDEBUG
#include <assert.h>
#endif

#ifndef NDEBUG
#define DEBUG_FIELD(type, name) type name
#else
#define DEBUG_FIELD(type, name)
#endif

#ifndef NDEBUG
#define DEBUG_ACTION(...) __VA_ARGS__
#else
#define DEBUG_ACTION(...)
#endif

#ifndef NDEBUG
#define DEBUG_ASSERT(...) assert(__VA_ARGS__)
#else
#define DEBUG_ASSERT(...)
#endif
