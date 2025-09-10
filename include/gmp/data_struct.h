#ifndef GMP_DATA_STRUCT_H
#define GMP_DATA_STRUCT_H

#include <cstdint>
#include <driver_types.h>
#include <string>
#include <vector>
#include <map>
#include <cupti_callbacks.h>
#include <cupti_activity.h>
struct ApiRuntimeRecord
{
  std::string functionName;
  std::map<CUpti_CallbackId, uint64_t> startTimestampMp;
};

// Stores the event group and event ID
typedef struct cupti_eventData_st
{
  CUpti_EventGroup eventGroup;
  CUpti_EventID eventId;
} cupti_eventData;

// Stores event data and values collected by the callback
typedef struct RuntimeApiTrace_st
{
  cupti_eventData *eventData;
  uint64_t eventVal;
} RuntimeApiTrace_t;

enum class GmpResult
{
  SUCCESS = 0,
  WARNING = 1,
  ERROR = 2,
};

enum class GmpOutputKernelReduction
{
  SUM = 0,
  MAX = 1,
  MEAN = 2,
};

enum class GmpProfileType
{
  CONCURRENT_KERNEL = 0,
  MEMORY,
};

struct GmpKernelData
{
  std::string name;
  int grid_size[3] = {};
  int block_size[3] = {};
};

struct GmpMemData{
    /**
   * The activity record kind, must be CUPTI_ACTIVITY_KIND_MEMORY2
   */
  CUpti_ActivityKind kind;

  /**
   * The memory operation requested by the user, \ref CUpti_ActivityMemoryOperationType.
   */
  CUpti_ActivityMemoryOperationType memoryOperationType;

  /**
   * The memory kind requested by the user, \ref CUpti_ActivityMemoryKind.
   */
  CUpti_ActivityMemoryKind memoryKind;

  /**
   * The correlation ID of the memory operation. Each memory operation is
   * assigned a unique correlation ID that is identical to the
   * correlation ID in the driver and runtime API activity record that
   * launched the memory operation.
   */
  uint32_t correlationId;

  /**
   * The virtual address of the allocation.
   */
  uint64_t address;

  /**
   * The number of bytes of memory allocated.
   */
  uint64_t bytes;

  /**
   * The start timestamp for the memory operation, in ns.
   */
  uint64_t timestamp;

  /**
   * The program counter of the memory operation.
   */
  uint64_t PC;

  /**
   * The ID of the process to which this record belongs to.
   */
  uint32_t processId;

  /**
   * The ID of the device where the memory operation is taking place.
   */
  uint32_t deviceId;

  /**
   * The ID of the context. If context is NULL, \p contextId is set to CUPTI_INVALID_CONTEXT_ID.
   */
  uint32_t contextId;

  /**
   * The ID of the stream. If memory operation is not async, \p streamId is set to CUPTI_INVALID_STREAM_ID.
   */
  uint32_t streamId;

  /**
   * Variable name. This name is shared across all activity
   * records representing the same symbol, and so should not be
   * modified.
   */
  const char* name;

  /**
   * \p isAsync is set if memory operation happens through async memory APIs.
   */
  uint32_t isAsync;

#ifdef CUPTILP64
  /**
   * Undefined. Reserved for internal use.
   */
  uint32_t pad1;
#endif

  /**
   * The memory pool configuration used for the memory operations.
   */
  struct PACKED_ALIGNMENT {
    /**
     * The type of the memory pool, \ref CUpti_ActivityMemoryPoolType
     */
    CUpti_ActivityMemoryPoolType memoryPoolType;

#ifdef CUPTILP64
    /**
     * Undefined. Reserved for internal use.
     */
    uint32_t pad2;
#endif

    /**
     * The base address of the memory pool.
     */
    uint64_t address;

    /**
     * The release threshold of the memory pool in bytes. \p releaseThreshold is
     * valid for CUPTI_ACTIVITY_MEMORY_POOL_TYPE_LOCAL, \ref CUpti_ActivityMemoryPoolType.
     */
    uint64_t releaseThreshold;

    /**
     * The size of memory pool in bytes and the processId of the memory pools
     * \p size is valid if \p memoryPoolType is
     * CUPTI_ACTIVITY_MEMORY_POOL_TYPE_LOCAL, \ref CUpti_ActivityMemoryPoolType.
     * \p processId is valid if \p memoryPoolType is
     * CUPTI_ACTIVITY_MEMORY_POOL_TYPE_IMPORTED, \ref CUpti_ActivityMemoryPoolType
     */
    union {
      uint64_t size;
      uint64_t processId;
    } pool;

    /**
     * The utilized size of the memory pool. \p utilizedSize is
     * valid for CUPTI_ACTIVITY_MEMORY_POOL_TYPE_LOCAL, \ref CUpti_ActivityMemoryPoolType.
     */
    uint64_t utilizedSize;
  } memoryPoolConfig;

    /**
     * The shared object or binary that the memory allocation request comes from.
     */
    const char* source;
};

struct GmpRangeData
{
  std::string name;
  std::vector<GmpKernelData> kernelDataInRange;
};

struct GmpMemRangeData
{
  std::string name;
  std::vector<GmpMemData> memDataInRange;
};


#endif // GMP_DATA_STRUCT_H