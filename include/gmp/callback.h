#ifndef GMP_CALLBACK_H
#define GMP_CALLBACK_H

#include <cupti.h>
#include <cupti_events.h>
#include <stdio.h>
#include <cassert>
#include "gmp/util.h"
#include "gmp/data_struct.h"
#include "gmp/log.h"

void CUPTIAPI getTimestampCallback(void *userdata, CUpti_CallbackDomain domain,
                                   CUpti_CallbackId cbid, const CUpti_CallbackData *cbInfo)
{
    ApiRuntimeRecord *traceData = (ApiRuntimeRecord *)userdata;
    traceData->functionName = cbInfo->symbolName;
    // Only process runtime API callbacks
    if (domain != CUPTI_CB_DOMAIN_RUNTIME_API)
        return;

    if (cbid == CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020 ||
        cbid == CUPTI_RUNTIME_TRACE_CBID_cudaLaunchKernel_v7000)
    {
        uint64_t timestamp;
        CUPTI_CALL(cuptiGetTimestamp(&timestamp));

        if (cbInfo->callbackSite == CUPTI_API_ENTER)
        {
            GMP_LOG_DEBUG("CBID " + std::to_string(cbid) + " Entered function");
            traceData->startTimestampMp[cbid] = timestamp;
        }
        else if (cbInfo->callbackSite == CUPTI_API_EXIT)
        {
            assert(traceData->startTimestampMp.find(cbid) != traceData->startTimestampMp.end());
            GMP_LOG_DEBUG("CBID " + std::to_string(cbid) + " Kernel " + std::string(cbInfo->symbolName) + " completed after " +
                          std::to_string(timestamp - traceData->startTimestampMp[cbid]) + " nanoseconds.");
            traceData->startTimestampMp.erase(cbid);
        }
    }
}

#endif // GMP_CALLBACK_H