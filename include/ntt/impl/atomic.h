#pragma once

// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=60932

#ifdef __cplusplus
#include <atomic> // IWYU pragma: export
using namespace std;
#else
#include <stdatomic.h> // IWYU pragma: export
#endif
