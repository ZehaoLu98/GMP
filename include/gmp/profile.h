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
#include <cuda.h>
#include "gmp/range_profiling.h"
#include "gmp/data_struct.h"

#define USE_CUPTI

#ifdef USE_CUPTI

#include <cupti.h>
#include "gmp/log.h"
#include "gmp/callback.h"
#include "gmp/util.h"

#define ENABLE_USER_RANGE false
#define MAX_NUM_RANGES 1000
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

// Abstract Node
class GmpProfileSession
{
  using time_t = std::chrono::microseconds;

public:
  GmpProfileSession(const std::string &session_name)
      : sessionName(session_name) {}
  virtual void report() const = 0;
  bool isActive() const { return is_active; }
  void deactivate() { is_active = false; }
  std::string getSessionName()
  {
    return sessionName;
  }

  void setRuntimeData(const ApiRuntimeRecord &data)
  {
    runtimeData = data;
  }

  const ApiRuntimeRecord &getRuntimeData() const
  {
    return runtimeData;
  }

  CUpti_SubscriberHandle getRuntimeSubscriberHandle() const
  {
    return runtimeSubscriber;
  }

  void setRuntimeHandle(CUpti_SubscriberHandle runtimeSubscriber)
  {
    this->runtimeSubscriber = runtimeSubscriber;
  }

  void pushKernelData(const GmpKernelData &data)
  {
    kernelData.push_back(data);
  }
  
  std::vector<GmpKernelData> getKernelData() const
  {
    return kernelData;
  }

protected:
  std::string sessionName;      // Name of the profiling session
  ApiRuntimeRecord runtimeData; // Data structure to hold timing information
  CUpti_SubscriberHandle runtimeSubscriber;
  std::vector<GmpKernelData> kernelData; // Names of kernels launched in this session
  bool is_active = true;
  CUcontext context = 0;
};

// Concrete Node
class GmpConcurrentKernelSession : public GmpProfileSession
{
public:
  GmpConcurrentKernelSession(const std::string &sessionName)
      : GmpProfileSession(sessionName) {}

  void report() const override
  {
    printf("Session %s captured %lld calls\n", sessionName.c_str(), num_calls);
  }
  unsigned long long num_calls;

private:
};

class SessionManager
{
public:
  SessionManager() = default;

  std::string getSessionName(GmpProfileType type)
  {
    if (ActivityMap.find(type) != ActivityMap.end() && !ActivityMap[type].empty())
    {
      return ActivityMap[type].back()->getSessionName();
    }
    else
    {
      GMP_LOG_ERROR("No active session of type " + std::to_string(static_cast<int>(type)) + " found.");
      return "";
    }
  }

  template <typename DerivedSession>
  using AccumulateFunc = std::function<void(DerivedSession *)>;

  // Apply the provided callback function to the active session of that type.
  template <typename DerivedSession>
  GmpResult accumulate(GmpProfileType type, AccumulateFunc<DerivedSession> callback)
  {
    if (!ActivityMap[type].empty())
    {
      auto &sessionPtr = ActivityMap[type].back();
      if (auto derivedSessionPtr = dynamic_cast<DerivedSession *>(sessionPtr.get()))
      {
        if(sessionPtr->isActive()){
          callback(derivedSessionPtr);
        }
        return GmpResult::SUCCESS;
      }
      else{
        return GmpResult::ERROR;
      }
    }
    return GmpResult::SUCCESS;
  }

  GmpResult reportAllSessions()
  {
    for (auto &pair : ActivityMap)
    {
      for (const auto &sessionPtr : pair.second)
      {
        sessionPtr->report();
      }
    }
    return GmpResult::SUCCESS;
  }

  GmpResult startSession(GmpProfileType type, std::unique_ptr<GmpProfileSession> sessionPtr);

  GmpResult endSession(GmpProfileType type);

  std::vector<GmpRangeData> getAllKernelDataOfType(GmpProfileType type)
  {
    std::vector<GmpRangeData> allKernelData;
    for (const auto &sessionPtr : ActivityMap[type])
    {
      auto dataInRange = sessionPtr->getKernelData();
      allKernelData.push_back({sessionPtr->getSessionName(), dataInRange});
    }
    return allKernelData;
  }

private:
  std::map<GmpProfileType, std::vector<std::unique_ptr<GmpProfileSession>>> ActivityMap;
};
#endif

class GmpProfiler
{
  GmpProfiler(const GmpProfiler &) = delete;
  GmpProfiler &operator=(const GmpProfiler &) = delete;

public:
  GmpProfiler();

  ~GmpProfiler();

  static GmpProfiler *getInstance()
  {
    if (!instance)
    {
      instance = new GmpProfiler();
#ifdef USE_CUPTI
      CUPTI_CALL(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL));
      CUPTI_CALL(cuptiActivityRegisterCallbacks(&GmpProfiler::bufferRequestedThunk,
                                                &GmpProfiler::bufferCompletedThunk));
      instance->cuptiProfilerHost = std::make_shared<CuptiProfilerHost>();

      // Get the current ctx for the device
      CUdevice cuDevice;
      DRIVER_API_CALL(cuDeviceGet(&cuDevice, 0));
      int computeCapabilityMajor = 0, computeCapabilityMinor = 0;
      DRIVER_API_CALL(cuDeviceGetAttribute(&computeCapabilityMajor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, cuDevice));
      DRIVER_API_CALL(cuDeviceGetAttribute(&computeCapabilityMinor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, cuDevice));
      printf("Compute Capability of Device: %d.%d\n", computeCapabilityMajor, computeCapabilityMinor);

      if (computeCapabilityMajor < 7 || (computeCapabilityMajor == 7 && computeCapabilityMinor < 5))
      {
        std::cerr << "Range Profiling is supported only on devices with compute capability 7.5 and above" << std::endl;
        exit(EXIT_FAILURE);
      }

      RangeProfilerConfig config;
      // default config values
      config.maxNumOfRanges = MAX_NUM_RANGES;
      config.minNestingLevel = MIN_NESTING_LEVEL;
      config.numOfNestingLevel = MAX_NUM_NESTING_LEVEL;

      // Should not create a context!!!!!!!!!
      CUcontext cuContext;
      // DRIVER_API_CALL(cuCtxCreate(&cuContext, 0, cuDevice));
      DRIVER_API_CALL(cuDevicePrimaryCtxRetain(&cuContext, cuDevice));
      DRIVER_API_CALL(cuCtxSetCurrent(cuContext)); // matches what Eigen/Runtime use
      instance->rangeProfilerTargetPtr = std::make_shared<RangeProfilerTarget>(cuContext, config);

      // Get chip name
      std::string chipName;
      CUPTI_CALL(RangeProfilerTarget::GetChipName(cuDevice, chipName));

      // Get Counter availability image
      std::vector<uint8_t> counterAvailabilityImage;
      CUPTI_CALL(RangeProfilerTarget::GetCounterAvailabilityImage(cuContext, counterAvailabilityImage));

      // Create config image
      std::vector<uint8_t> configImage;
      instance->cuptiProfilerHost->SetUp(chipName, counterAvailabilityImage);
      CUPTI_CALL(instance->cuptiProfilerHost->CreateConfigImage(instance->metrics, configImage));

      // Enable Range profiler
      CUPTI_CALL(instance->rangeProfilerTargetPtr->EnableRangeProfiler());

      // Create CounterData Image
      CUPTI_CALL(instance->rangeProfilerTargetPtr->CreateCounterDataImage(instance->metrics, instance->counterDataImage));

      CUPTI_CALL(instance->rangeProfilerTargetPtr->SetConfig(
          ENABLE_USER_RANGE ? CUPTI_UserRange : CUPTI_AutoRange,
          ENABLE_USER_RANGE ? CUPTI_UserReplay : CUPTI_KernelReplay,
          configImage,
          instance->counterDataImage));
#endif
    }
    return instance;
  }

  void startRangeProfiling()
  {
#ifdef USE_CUPTI
    CUPTI_API_CALL(rangeProfilerTargetPtr->StartRangeProfiler());
#endif
  }

  void stopRangeProfiling()
  {
#ifdef USE_CUPTI
    CUPTI_API_CALL(rangeProfilerTargetPtr->StopRangeProfiler());
#endif
  }

  // GmpResult RangeProfile(char *name, std::function<void()> func);

  // Range Profiling API
  GmpResult pushRange(const char *rangeName);

  // Activity API
  GmpResult pushRange(const std::string &name, GmpProfileType type);

  // Range Profiling API
  GmpResult popRange();

  // Activity API
  GmpResult popRange(const std::string &name, GmpProfileType type);

  // Called after end of range profiling
  void printProfilerRanges();

  bool isAllPassSubmitted()
  {
#ifdef USE_CUPTI
    return rangeProfilerTargetPtr->IsAllPassSubmitted();
#endif
    return true;
  }

  void decodeCounterData()
  {
#ifdef USE_CUPTI
    CUPTI_API_CALL(rangeProfilerTargetPtr->DecodeCounterData());
#endif
  }

private:
  static GmpProfiler *instance;

#ifdef USE_CUPTI
  RangeProfilerTargetPtr rangeProfilerTargetPtr = nullptr;
  CuptiProfilerHostPtr cuptiProfilerHost = nullptr;
  SessionManager sessionManager;
  std::vector<uint8_t> counterDataImage;
  std::vector<const char *> metrics = {
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

      // // Group 4
      // // Sub Group 1
      // "smsp__average_warp_latency_per_inst_issued.ratio",
      // // Sub Group 2
      // "smsp__average_warps_issue_stalled_math_pipe_throttle_per_issue_active.ratio",
      // "smsp__average_warps_issue_stalled_wait_per_issue_active.ratio",
      // // Sub Group 3
      "smsp__average_warps_issue_stalled_long_scoreboard_per_issue_active.ratio",
      "smsp__average_warps_issue_stalled_short_scoreboard_per_issue_active.ratio",
  };
#endif

  static void CUPTIAPI bufferRequestedThunk(uint8_t **buffer, size_t *size, size_t *maxNumRecords)
  {
#ifdef USE_CUPTI
    if (instance)
      instance->bufferRequestedImpl(buffer, size, maxNumRecords);
#endif
  }

  static void CUPTIAPI bufferCompletedThunk(CUcontext ctx, uint32_t streamId,
                                            uint8_t *buffer, size_t size, size_t validSize)
  {
#ifdef USE_CUPTI
    if (instance)
      instance->bufferCompletedImpl(ctx, streamId, buffer, size, validSize);
#endif
  }

  void bufferRequestedImpl(uint8_t **buffer, size_t *size, size_t *maxNumRecords);

  void bufferCompletedImpl(CUcontext ctx, uint32_t streamId,
                           uint8_t *buffer, size_t size, size_t validSize);
};
#endif // GMP_PROFILE_H