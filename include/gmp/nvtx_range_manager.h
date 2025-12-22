#ifndef GMP_NVTX_RANGE_MANAGER_H
#define GMP_NVTX_RANGE_MANAGER_H
#define ENABLE_NVTX

#ifdef ENABLE_NVTX
#include <nvtx3/nvtx3.hpp>
#include <stack>
#include <unordered_map>
#include <string>

// NVTX Range Manager - independent of CUPTI
class NvtxRangeManager {
public:
  NvtxRangeManager() = default;
  ~NvtxRangeManager() = default;

  // Start an NVTX range and return its ID
  nvtxRangeId_t startRange(const std::string& name);

  // End the most recent NVTX range
  bool endRange(const std::string& expectedName = "");

  // Get the number of active ranges
  size_t getActiveRangeCount() const;

  // Clear all ranges (emergency cleanup)
  void clearAllRanges();

private:
  std::stack<nvtxRangeId_t> activeRanges_;
  std::unordered_map<nvtxRangeId_t, std::string> rangeNameMap_;
};
#endif // ENABLE_NVTX

#endif // GMP_NVTX_RANGE_MANAGER_H