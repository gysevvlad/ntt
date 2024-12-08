#pragma once

// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=60932

#ifdef __cplusplus
#include <atomic>
using namespace std;
#else
#include <stdatomic.h>
#endif
