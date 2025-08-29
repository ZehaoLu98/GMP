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
#define MAX_NUM_RANGES 200
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

protected:
  std::string sessionName;      // Name of the profiling session
  ApiRuntimeRecord runtimeData; // Data structure to hold timing information
  CUpti_SubscriberHandle runtimeSubscriber;
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
    printf("Session %s captured %llu calls\n", sessionName.c_str(), num_calls);
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

  // Attempt to add a session of a specific type.
  // If the last session of that type is active, this function does nothing.
  GmpResult startSession(GmpProfileType type, std::unique_ptr<GmpProfileSession> sessionPtr);

  template <typename DerivedSession>
  using AccumulateFunc = std::function<void(DerivedSession *)>;

  // Apply the provided callback function to the active session of that type.
  template <typename DerivedSession>
  GmpResult accumulate(GmpProfileType type, AccumulateFunc<DerivedSession> callback)
  {
    if (ActivityMap[type].empty())
    {
      GMP_LOG_ERROR("No active session of type " + std::to_string(static_cast<int>(type)) + " found.");
      return GmpResult::ERROR;
    }
    auto &sessionPtr = ActivityMap[type].back();
    if (auto derivedSessionPtr = dynamic_cast<DerivedSession *>(sessionPtr.get()))
    {
      assert(derivedSessionPtr->isActive());
      callback(derivedSessionPtr);
      return GmpResult::SUCCESS;
    }
  }

  GmpResult endSession(GmpProfileType type);

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
      // CUPTI_CALL(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL));
      // CUPTI_CALL(cuptiActivityRegisterCallbacks(&GmpProfiler::bufferRequestedThunk,
      //                                           &GmpProfiler::bufferCompletedThunk));
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
        ENABLE_USER_RANGE? CUPTI_UserRange : CUPTI_AutoRange,
          ENABLE_USER_RANGE? CUPTI_UserReplay : CUPTI_KernelReplay,
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
      // "dram__bytes_read.sum",
      // "dram__bytes_write.sum",
      // "l1tex__t_sectors_pipe_lsu_mem_global_op_st.sum",

      // "sm__warps_active.avg.pct_of_peak_sustained_active",
      // "sm__throughput.avg.pct_of_peak_sustained_elapsed",
      // "smsp__warps_launched.sum",
      // "smsp__inst_executed.avg",
      // "smsp__inst_issued.sum",
      // "gpu__time_duration.sum",
      "gpc__cycles_active.max",
      "gpc__cycles_elapsed.max",
      // "gpu__time_active.sum",
      // "gpu__time_duration_measured_wallclock.sum",
      // "gpu__cycles_active.sum",
      // "gpu__cycles_elapsed.sum",
      // "sm__cycles_active.sum",
      // "smsp__cycles_active.sum",
      // "smsp__inst_executed.sum",
      // "dram__throughput.avg.pct_of_peak_sustained_elapsed",
      // "dram__bytes.sum",
      // "l1tex__t_sectors_pipe_lsu_mem_global_op_ld.sum",
      // "dram__bytes_write.sum"
      // "gpu__dram_throughput.avg.pct_of_peak_sustained_elapsed",

      // "sm__ctas_launched.sum",  
      // "dram__sectors_read.sum",
      // "gpu__cycles_in_region",
      // "dram__throughput.avg.pct_of_peak_sustained_elapsed", // Seems like it cannot be read with the dram__sectors_read.sum.
      // "l1tex__t_sectors_pipe_lsu_mem_global_op_ld.sum",
      // "l1tex__t_sectors_pipe_lsu_mem_global_op_st.sum",
      // "l1tex__t_sector_hit_rate.pct"
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