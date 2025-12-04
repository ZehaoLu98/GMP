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
// #define ENABLE_NVTX

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
  void printProfilerRanges(GmpOutputKernelReduction option);

  // Print memory activity for all ranges
  void printMemoryActivity();

  // Get all memory activity data
  std::vector<GmpMemRangeData> getMemoryActivity();

  bool isAllPassSubmitted();

  void decodeCounterData();

  void produceOutput(GmpOutputKernelReduction option);

  void addMetrics(const std::string &metric);

  void enable();

  void disable();

private:
  static GmpProfiler *instance;
  bool isInitialized = false;
  bool isEnabled = false;

#ifdef ENABLE_NVTX
  NvtxRangeManager nvtxManager_;
#endif

  std::vector<std::string> metrics = {
      // Group 1
      // "gpu__time_duration.sum",
      // "gpu__time_duration.max",
      // "gpc__cycles_elapsed.avg.per_second",
      // "gpc__cycles_elapsed.max",
      // "sm__cycles_active.max",

      // // Group 2
      // // Sub Group 1
      // "smsp__inst_executed.sum",
      // "smsp__sass_inst_executed_op_shared_ld.sum",
      // "smsp__sass_inst_executed_op_shared_st.sum",
      // "smsp__sass_inst_executed_op_global_ld.sum",
      // "smsp__sass_inst_executed_op_global_st.sum",
      // // Sub Group 2
      // "sm__pipe_alu_cycles_active.max",
      // "sm__pipe_fma_cycles_active.max",
      // "sm__pipe_tensor_cycles_active.max",
      // "sm__pipe_shared_cycles_active.max",
      // // Sub Group 3
      // "sm__sass_inst_executed_op_ldgsts_cache_access.sum",
      // "sm__sass_inst_executed_op_ldgsts_cache_bypass.sum",

      // // Group 3
      // // Sub Group 1
      // "l1tex__t_requests_pipe_lsu_mem_global_op_ld.sum",
      // // Sub Group 2
      // "l1tex__t_sectors_pipe_lsu_mem_global_op_ld.sum",
      // // Sub Group 3
      // "l1tex__t_requests_pipe_lsu_mem_global_op_st.sum",
      // // Sub Group 4
      // "l1tex__t_sectors_pipe_lsu_mem_global_op_st.sum",
      // // Sub Group 5
      // "sm__sass_l1tex_t_requests_pipe_lsu_mem_global_op_ldgsts_cache_access.sum",
      // "sm__sass_l1tex_t_sectors_pipe_lsu_mem_global_op_ldgsts_cache_access.sum",
      // "sm__sass_l1tex_t_requests_pipe_lsu_mem_global_op_ldgsts_cache_bypass.sum",
      // "sm__sass_l1tex_t_sectors_pipe_lsu_mem_global_op_ldgsts_cache_bypass.sum",
      // // Sub Group 6
      // "lts__t_requests_srcunit_tex_op_read.sum",
      // "lts__t_requests_srcunit_tex_op_write.sum",
      // "dram__sectors_read.sum",
      // "dram__sectors_write.sum",
      // // Sub Group 7
      // "lts__t_requests_srcunit_l1_op_read.sum",
      // "lts__t_requests_srcunit_l1_op_write.sum",

      // // Group 4
      // // Sub Group 1
      // "smsp__average_warp_latency_per_inst_issued.ratio",
      // // Sub Group 2
      // "smsp__average_warps_issue_stalled_math_pipe_throttle_per_issue_active.ratio",
      // "smsp__average_warps_issue_stalled_wait_per_issue_active.ratio",
      // // Sub Group 3
      // "smsp__average_warps_issue_stalled_long_scoreboard_per_issue_active.ratio",
      // "smsp__average_warps_issue_stalled_short_scoreboard_per_issue_active.ratio",
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
#endif // GMP_PROFILE_H