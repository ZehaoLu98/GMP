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

#endif // GMP_DATA_STRUCT_H