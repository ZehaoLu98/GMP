#ifndef GMP_LLMC_UTILS_LOG_H
#define GMP_LLMC_UTILS_LOG_H

#include <iostream>
#include <chrono>
#include <ctime>
#include <iomanip>


// 0: No logging
// 1: ERROR
// 2: WARNING
// 3: SYS INFO
// 4: DEBUG INFO
#define GMP_LOG_LEVEL 4

#define GMP_LOG(fmt) \
  ([&]() -> std::ostream& { \
    auto now = std::chrono::system_clock::now(); \
    std::time_t t = std::chrono::system_clock::to_time_t(now); \
    std::tm* tm = std::localtime(&t); \
    return std::cout << "[" << fmt << ", " << std::put_time(tm, "%Y-%m-%d %H:%M:%S") << "] "; \
  })()

#define GMP_LOG_ERROR(msg) \
  do { if (GMP_LOG_LEVEL >= 1) GMP_LOG("ERROR") << msg << std::endl; } while (0)
#define GMP_LOG_WARNING(msg) \
  do { if (GMP_LOG_LEVEL >= 2) GMP_LOG("WARNING") << msg << std::endl; } while (0)
#define GMP_LOG_INFO(msg) \
  do { if (GMP_LOG_LEVEL >= 3) GMP_LOG("INFO") << msg << std::endl; } while (0)
#define GMP_LOG_DEBUG(msg) \
  do { if (GMP_LOG_LEVEL >= 4) GMP_LOG("DEBUG") << msg << std::endl; } while (0)

#endif  // GMP_LLMC_UTILS_LOG_H