#ifndef GMP_PROFILE_H
#define GMP_PROFILE_H
#include <stdio.h>
#include <stdlib.h>
#include <cupti.h>
#include <map>
#include <memory>
#include <cupti.h>
#include "gmp/log.h"

#if GMP_LOG_LEVEL <= GMP_LOG_LEVEL_INFO
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

#define CUPTI_CALL(call)                                                \
do {                                                                  \
  CUptiResult _status = call;                                         \
  if (_status != CUPTI_SUCCESS) {                                     \
    const char *errstr;                                               \
    cuptiGetResultString(_status, &errstr);                           \
    fprintf(stderr, "%s:%d: error: function %s failed with error %s.\n", \
            __FILE__, __LINE__, #call, errstr);                       \
    exit(-1);                                                         \
  }                                                                   \
} while (0)

enum class GmpResult {
  SUCCESS = 0,
  WARNING = 1,
  ERROR = 2,
};

enum class GmpProfileType {
  CONCURRENT_KERNEL = 0,
};

// Abstract Node
class GmpProfileSession{
  using time_t = std::chrono::microseconds;
  public:
    GmpProfileSession(const std::string& session_name)
        : session_name(session_name) {}
    virtual void report() const = 0;
    bool isActive() const { return is_active; }
    void deactivate() { is_active = false; }
    std::string getSessionName(){
      return session_name;
    }
  protected:
    std::string session_name; // Name of the profiling session
    time_t duration;
    bool is_active = true;
};

// Concrete Node
class GmpConcurrentKernelSession : public GmpProfileSession {
public:
  GmpConcurrentKernelSession(const std::string& session_name)
      : GmpProfileSession(session_name) {}

  void report() const override {
    printf("Session %s captured %llu calls\n", session_name.c_str(), num_calls);
  }

  unsigned long long num_calls;

private:
};

class SessionManager{
  public:
    SessionManager() = default;

    std::string getSessionName(GmpProfileType type) {
      if (ActivityMap.find(type) != ActivityMap.end() && !ActivityMap[type].empty()) {
        return ActivityMap[type].back()->getSessionName();
      }
      else {
        GMP_LOG_ERROR("No active session of type " + std::to_string(static_cast<int>(type)) + " found.");
        return "";
      }
    }

    // Attempt to add a session of a specific type.
    // If the last session of that type is active, this function does nothing.
    GmpResult addSession(GmpProfileType type, std::unique_ptr<GmpProfileSession> session) {
      assert(session != nullptr);
      if(ActivityMap[type].empty()||!ActivityMap[type].back()->isActive()) {
        ActivityMap[type].push_back(std::move(session));
        GMP_LOG_DEBUG("Session " + ActivityMap[type].back()->getSessionName() + " of type " + std::to_string(static_cast<int>(type)) + " added.");
        return GmpResult::SUCCESS;
      }
      else{
        GMP_LOG_WARNING("Session " + ActivityMap[type].back()->getSessionName() + " of type " + std::to_string(static_cast<int>(type)) + " is already active. Cannot add a new session.");
        return GmpResult::ERROR;
      }
    }

    template<typename DerivedSession>
    using AccumulateFunc = std::function<void(DerivedSession*)>;

    // Apply the provided callback function to the active session of that type.
    template<typename DerivedSession>
    GmpResult accumulate(GmpProfileType type, AccumulateFunc<DerivedSession> callback) {
      if(ActivityMap[type].empty()){
        GMP_LOG_ERROR("No active session of type " + std::to_string(static_cast<int>(type)) + " found.");
        return GmpResult::ERROR;
      }
      auto& sessionPtr = ActivityMap[type].back();
      if (auto derivedSessionPtr = dynamic_cast<DerivedSession*>(sessionPtr.get())) {
        assert(derivedSessionPtr->isActive());
        callback(derivedSessionPtr);
        return GmpResult::SUCCESS;
      }
    }

    GmpResult endSession(GmpProfileType type){
      if(ActivityMap[type].empty()) {
        GMP_LOG_ERROR("No active session of type " + std::to_string(static_cast<int>(type)) + " found.");
        return GmpResult::ERROR;
      }
      auto& sessionPtr = ActivityMap[type].back();
      if (sessionPtr->isActive()) {
        sessionPtr->report();
        sessionPtr->deactivate();
        GMP_LOG_DEBUG("Session of type " + std::to_string(static_cast<int>(type)) + " ended.");
        return GmpResult::SUCCESS;
      }
      GMP_LOG_WARNING("Session of type " + std::to_string(static_cast<int>(type)) + " is already inactive.");
      return GmpResult::WARNING;
    }

  private:
    std::map<GmpProfileType, std::vector<std::unique_ptr<GmpProfileSession>>> ActivityMap;
};

class GmpProfiler{
  GmpProfiler(const GmpProfiler&) = delete;
  GmpProfiler& operator=(const GmpProfiler&) = delete;
public:
  GmpProfiler() {
    s_instance = this;
    CUPTI_CALL(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL));
    CUPTI_CALL(cuptiActivityRegisterCallbacks(&GmpProfiler::bufferRequestedThunk,
                                              &GmpProfiler::bufferCompletedThunk));
  }

  ~GmpProfiler() {
    CUPTI_CALL(cuptiActivityFlushAll(0));
    CUPTI_CALL(cuptiActivityDisable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL));
    if (s_instance == this) s_instance = nullptr;
  }

  static GmpProfiler* getInstance() {
    return s_instance;
  }

  GmpResult startProfiling(std::string name, GmpProfileType type) {
    GMP_LOG_DEBUG("Starting profiling for type: " + std::to_string(static_cast<int>(type)) + " with name: " + name);

    switch (type) {
      case GmpProfileType::CONCURRENT_KERNEL:
        CUPTI_CALL(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL));
        return sessionManager.addSession(
          GmpProfileType::CONCURRENT_KERNEL,
          std::make_unique<GmpConcurrentKernelSession>(name)
        );
      default:
        GMP_LOG_ERROR("Unsupported profile type: " + std::to_string(static_cast<int>(type)));
        return GmpResult::ERROR;
    }
  }

  GmpResult endProfiling(GmpProfileType type) {
    switch (type)
    {
      case GmpProfileType::CONCURRENT_KERNEL:{

        CUPTI_CALL(cuptiActivityDisable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL));
        GMP_LOG_DEBUG("Flusing all called");
        CUPTI_CALL(cuptiActivityFlushAll(0));
        GMP_LOG_DEBUG("Ending profiling for type: " + std::to_string(static_cast<int>(type)) + " with session name: " + sessionManager.getSessionName(type));
        auto result = sessionManager.endSession(type);
        if(result != GmpResult::SUCCESS) {
          GMP_LOG_ERROR("Failed to end profiling for type: " + std::to_string(static_cast<int>(type)));
        }
        return result;
      }
      default:{
        GMP_LOG_ERROR("Unsupported profile type: " + std::to_string(static_cast<int>(type)));
        return GmpResult::ERROR;
      }
    }
  }

private:
  static GmpProfiler* s_instance;
  SessionManager sessionManager;

  static void CUPTIAPI bufferRequestedThunk(uint8_t **buffer, size_t *size, size_t *maxNumRecords) {
    if (s_instance) s_instance->bufferRequestedImpl(buffer, size, maxNumRecords);
  }

  static void CUPTIAPI bufferCompletedThunk(CUcontext ctx, uint32_t streamId,
                                            uint8_t *buffer, size_t size, size_t validSize) {
    if (s_instance) s_instance->bufferCompletedImpl(ctx, streamId, buffer, size, validSize);
  }

  void bufferRequestedImpl(uint8_t **buffer, size_t *size, size_t *maxNumRecords) {
    *size = 16 * 1024;
    *buffer = (uint8_t *)malloc(*size);
    *maxNumRecords = 0;
  }

  void bufferCompletedImpl(CUcontext ctx, uint32_t streamId,
                           uint8_t *buffer, size_t size, size_t validSize) {
    CUptiResult status;
    CUpti_Activity *record = nullptr;
    GMP_LOG_DEBUG("Buffer completion callback called");
    for (;;) {
      status = cuptiActivityGetNextRecord(buffer, validSize, &record);
      if (status == CUPTI_SUCCESS) {
        if (record->kind == CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL) {
          auto *kernel = (CUpti_ActivityKernel8 *)record;
          printf("CUPTI: Kernel \"%s\" launched on stream %u, grid (%u,%u,%u), block (%u,%u,%u)\n",
                 kernel->name, kernel->streamId,
                 kernel->gridX, kernel->gridY, kernel->gridZ,
                 kernel->blockX, kernel->blockY, kernel->blockZ);
          auto result = sessionManager.accumulate<GmpConcurrentKernelSession>(
            GmpProfileType::CONCURRENT_KERNEL,
            [](GmpConcurrentKernelSession* sessionPtr){ sessionPtr->num_calls++; }
          );
          if (result != GmpResult::SUCCESS) {
            GMP_LOG_ERROR("Failed to accumulate concurrent kernel session.");
          }
        }
      } else if (status == CUPTI_ERROR_MAX_LIMIT_REACHED) {
        break;
      } else {
        CUPTI_CALL(status);
      }
    }
    size_t dropped = 0;
    cuptiActivityGetNumDroppedRecords(ctx, streamId, &dropped);
    if (dropped != 0) {
      printf("CUPTI: Dropped %zu activity records\n", dropped);
    }
    free(buffer);
    GMP_LOG_DEBUG("Buffer completion callback ended");
  }
};

inline GmpProfiler* GmpProfiler::s_instance = new GmpProfiler();

#endif  // GMP_PROFILE_H