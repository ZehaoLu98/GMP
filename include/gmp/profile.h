#ifndef GMP_PROFILE_H
#define GMP_PROFILE_H
#include <functional>
#include <vector>
#include <cassert>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <memory>
#include <string>
#include <cuda.h>

#include "gmp/range_profiling.h"
#include "gmp/data_struct.h"
#include "gmp/log.h"
#include "gmp/session.h"
#include "gmp/session_manager.h"
#include "gmp/nvtx_range_manager.h"

#define USE_CUPTI
#define ENABLE_NVTX

#ifdef USE_CUPTI

#include <cupti.h>
#include "gmp/log.h"
#include "gmp/callback.h"
#include "gmp/util.h"

#define ENABLE_USER_RANGE false
#define MAX_NUM_RANGES 2000
#define MAX_NUM_NESTING_LEVEL 1
#define MIN_NESTING_LEVEL 1

#if GMP_LOG_LEVEL <= GMP_LOG_LEVEL_INFO
#define GMP_PROFILING(name, func, ...) \
  do                                   \
  {                                    \
    func(__VA_ARGS__);                 \
  } while (0)
#else
#define GMP_PROFILING(name, func, ...)                                                          \
  do                                                                                            \
  {                                                                                             \
    auto start = std::chrono::high_resolution_clock::now();                                     \
    func(__VA_ARGS__);                                                                          \
    auto end = std::chrono::high_resolution_clock::now();                                       \
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count(); \
    GMP_LOG("INFO") << name << " finished in " << duration << " microseconds." << std::endl;    \
  } while (0)
#endif
#endif // GMP_PROFILE_H

// Singleton Profiler Class, exposes high-level profiling APIs
class GmpProfiler
{
  GmpProfiler(const GmpProfiler &) = delete;
  GmpProfiler &operator=(const GmpProfiler &) = delete;

public:
  GmpProfiler();

  ~GmpProfiler();

  void init();

  static GmpProfiler *getInstance();

  void startRangeProfiling();

  void stopRangeProfiling();

  // GmpResult RangeProfile(char *name, std::function<void()> func);

  // Activity + Range Profiling API
  GmpResult pushRange(const std::string &name, GmpProfileType type);

  // Activity + Range Profiling API
  GmpResult popRange(const std::string &name, GmpProfileType type);

  // Called after end of range profiling
  void printProfilerRanges(std::string& configName, GmpOutputKernelReduction option);

  // Print memory activity for all ranges
  void printMemoryActivity();

  // Get all memory activity data
  std::vector<GmpMemRangeData> getMemoryActivity();

  bool isAllPassSubmitted();

  void decodeCounterData();

  void produceOutput(std::string& configName, GmpOutputKernelReduction option);

  void addMetrics(const std::string &metric);

  void enable();

  void disable();

  bool hasSubmittedAllPasses();

private:
  static GmpProfiler *instance;
  bool isInitialized = false;
  bool isEnabled = false;

#ifdef ENABLE_NVTX
  NvtxRangeManager nvtxManager_;
#endif

  std::vector<std::string> metrics = {
      // Group 1
      "gpu__time_duration.sum",
      "gpc__cycles_elapsed.avg.per_second",
      "gpc__cycles_elapsed.max",
      "smsp__inst_executed.sum",
      "smsp__cycles_active.sum",
      "smsp__cycles_active.avg",
      "smsp__sass_inst_executed_op_shared_ld.sum",
      "smsp__sass_inst_executed_op_shared_st.sum",
      "smsp__sass_inst_executed_op_global_ld.sum",
      "smsp__sass_inst_executed_op_global_st.sum",
      "sm__warps_active.sum",
      "smsp__warps_active.sum",
      "smsp__warps_eligible.sum",
      "sm__cycles_active.sum",
      "sm__cycles_active.avg",
      "dram__sectors_read.sum",
      "dram__sectors_write.sum",
      "smsp__warps_issue_stalled_math_pipe_throttle.sum",
      "smsp__warps_issue_stalled_mio_throttle.sum",
      "smsp__warps_issue_stalled_long_scoreboard.sum",
      "smsp__pipe_alu_cycles_active.sum",
      "smsp__pipe_fma_cycles_active.sum",
      "smsp__pipe_fp64_cycles_active.sum",
      "smsp__pipe_shared_cycles_active.sum",
      "smsp__pipe_tensor_cycles_active.sum",
      "l1tex__t_sector_hit_rate.pct",
      "lts__t_sector_hit_rate.pct",
      "l1tex__throughput.avg.pct_of_peak_sustained_active",
      "lts__throughput.avg.pct_of_peak_sustained_active",
      "dram__throughput.avg.pct_of_peak_sustained_active",
      
  };
#ifdef USE_CUPTI
  RangeProfilerTargetPtr rangeProfilerTargetPtr = nullptr;
  CuptiProfilerHostPtr cuptiProfilerHost = nullptr;
  SessionManager sessionManager;
  std::vector<uint8_t> counterDataImage;

#endif

  static void CUPTIAPI bufferRequestedThunk(uint8_t **buffer, size_t *size, size_t *maxNumRecords);

  static void CUPTIAPI bufferCompletedThunk(CUcontext ctx, uint32_t streamId,
                                            uint8_t *buffer, size_t size, size_t validSize);

  void bufferRequestedImpl(uint8_t **buffer, size_t *size, size_t *maxNumRecords);

  void bufferCompletedImpl(CUcontext ctx, uint32_t streamId,
                           uint8_t *buffer, size_t size, size_t validSize);

  // Check if the number of kernels recorded by activity API matches that by range profiler
  GmpResult checkActivityAndRangeResultMatch();
  
  GmpResult pushRangeProfilerRange(const char *rangeName);

  GmpResult popRangeProfilerRange();
};
#endif // GMP_PROFILE_H