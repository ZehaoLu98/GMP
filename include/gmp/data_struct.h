#ifndef GMP_DATA_STRUCT_H
#define GMP_DATA_STRUCT_H

#include <cstdint>
#include <driver_types.h>
#include <string>
#include <map>
#include <cupti_callbacks.h>
struct ApiRuntimeRecord
{
    std::string functionName;
    std::map<CUpti_CallbackId, uint64_t> startTimestampMp;
};

// Stores the event group and event ID
typedef struct cupti_eventData_st {
  CUpti_EventGroup eventGroup;
  CUpti_EventID eventId;
} cupti_eventData;

// Stores event data and values collected by the callback
typedef struct RuntimeApiTrace_st {
  cupti_eventData *eventData;
  uint64_t eventVal;
} RuntimeApiTrace_t;


enum class GmpResult
{
  SUCCESS = 0,
  WARNING = 1,
  ERROR = 2,
};

enum class GmpProfileType
{
  CONCURRENT_KERNEL = 0,
};

#endif // GMP_DATA_STRUCT_H