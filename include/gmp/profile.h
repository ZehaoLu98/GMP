#ifndef GMP_PROFILE_H
#define GMP_PROFILE_H
#include "gmp/log.h"

#if GMP_LOG_LEVEL == 0
#define GMP_PROFILING(name, func, ...) \
  do { func(__VA_ARGS__); } while (0)
#else
#define GMP_PROFILING(name, func, ...) \
do { \
        auto start = std::chrono::high_resolution_clock::now(); \
        func(__VA_ARGS__); \
        auto end = std::chrono::high_resolution_clock::now(); \
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(); \
        GMP_LOG("INFO") << name << " finished in " << duration << " microseconds." << std::endl; \
} while (0)
#endif
#endif  // GMP_PROFILE_H