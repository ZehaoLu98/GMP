#ifndef GMP_SESSION_H
#define GMP_SESSION_H

#include <vector>
#include <string>
#include <chrono>

#ifdef USE_CUPTI
#include <cupti.h>
#endif

#include "gmp/data_struct.h"

// Abstract Node
class GmpProfileSession
{
  using time_t = std::chrono::microseconds;

public:
  GmpProfileSession(const std::string &session_name)
      : sessionName(session_name) {}
  virtual ~GmpProfileSession() = default;
  virtual void report() const = 0;
  bool isActive() const;
  void deactivate();
  std::string getSessionName();

  void setRuntimeData(const ApiRuntimeRecord &data);

  const ApiRuntimeRecord &getRuntimeData() const;

#ifdef USE_CUPTI
  CUpti_SubscriberHandle getRuntimeSubscriberHandle() const;

  void setRuntimeHandle(CUpti_SubscriberHandle runtimeSubscriber);
#endif

  void pushKernelData(const GmpKernelData &data);

  void pushMemData(const GmpMemData &data);

  std::vector<GmpKernelData> getKernelData() const;

  std::vector<GmpMemData> getMemData() const;

protected:
  std::string sessionName;      // Name of the profiling session
  ApiRuntimeRecord runtimeData; // Data structure to hold timing information
#ifdef USE_CUPTI
  CUpti_SubscriberHandle runtimeSubscriber;
  CUcontext context = 0;
#endif
  std::vector<GmpKernelData> kernelData; // Names of kernels launched in this session
  std::vector<GmpMemData> memData;       // Memory operations in this session
  bool is_active = true;
};

// Concrete Node
class GmpConcurrentKernelSession : public GmpProfileSession
{
public:
  GmpConcurrentKernelSession(const std::string &sessionName);

  void report() const override;
  unsigned long long num_calls;

private:
};

class GmpMemSession : public GmpProfileSession
{
public:
  GmpMemSession(const std::string &sessionName);

  void report() const override;
  unsigned long long num_calls;

private:
};

#endif // GMP_SESSION_H