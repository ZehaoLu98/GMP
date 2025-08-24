#include <functional>
#include <vector>
#include <cassert>
#include <memory>
#ifndef GMP_PROFILE_H
#define GMP_PROFILE_H
#include <stdio.h>
#include <stdlib.h>
#include <cupti.h>
#include <map>
#include <memory>
#include <cuda.h>
#include "gmp/log.h"
#include "gmp/callback.h"
#include "gmp/util.h"
#include "gmp/data_struct.h"
#include "gmp/range_profiling.h"

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

class GmpProfiler
{
  GmpProfiler(const GmpProfiler &) = delete;
  GmpProfiler &operator=(const GmpProfiler &) = delete;

public:
  GmpProfiler(int maxNumOfRanges = 1000, int minNestingLevel = 1, int numOfNestingLevel = 5);

  ~GmpProfiler();

  static GmpProfiler *getInstance()
  {
    return instance;
  }

  void startRangeProfiling()
  {
    CUPTI_API_CALL(rangeProfilerTargetPtr->StartRangeProfiler());
  }

  void stopRangeProfiling()
  {
    CUPTI_API_CALL(rangeProfilerTargetPtr->StopRangeProfiler());
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
    return rangeProfilerTargetPtr->IsAllPassSubmitted();
  }

  void decodeCounterData()
  {
    CUPTI_API_CALL(rangeProfilerTargetPtr->DecodeCounterData());
  }

private:
  static GmpProfiler *instance;
  RangeProfilerTargetPtr rangeProfilerTargetPtr = nullptr;
  CuptiProfilerHostPtr cuptiProfilerHost = nullptr;
  SessionManager sessionManager;
  std::vector<uint8_t> counterDataImage;
  std::vector<const char *> metrics = {
      "sm__warps_active.avg.pct_of_peak_sustained_active",
      "sm__throughput.avg.pct_of_peak_sustained_elapsed",
      "smsp__warps_launched.sum",
      "smsp__inst_executed.avg",
      "smsp__inst_issued.sum",
      "gpu__time_duration.sum",
      "dram__throughput.avg.pct_of_peak_sustained_elapsed",
      "l1tex__t_sectors_pipe_lsu_mem_global_op_ld.sum",
      "l1tex__t_sectors_pipe_lsu_mem_global_op_st.sum",
      "l1tex__t_sector_hit_rate.pct"};

  static void CUPTIAPI bufferRequestedThunk(uint8_t **buffer, size_t *size, size_t *maxNumRecords)
  {
    if (instance)
      instance->bufferRequestedImpl(buffer, size, maxNumRecords);
  }

  static void CUPTIAPI bufferCompletedThunk(CUcontext ctx, uint32_t streamId,
                                            uint8_t *buffer, size_t size, size_t validSize)
  {
    if (instance)
      instance->bufferCompletedImpl(ctx, streamId, buffer, size, validSize);
  }

  void bufferRequestedImpl(uint8_t **buffer, size_t *size, size_t *maxNumRecords);

  void bufferCompletedImpl(CUcontext ctx, uint32_t streamId,
                           uint8_t *buffer, size_t size, size_t validSize);
};
#endif // GMP_PROFILE_H