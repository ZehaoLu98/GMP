#include "gmp/callback.h"

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

void CUPTIAPI getEventValueCallback(void *userdata, CUpti_CallbackDomain domain,
                                    CUpti_CallbackId cbid, const CUpti_CallbackData *cbInfo)
{
  // Only process callbacks for kernel launches
  if ((cbid != CUPTI_RUNTIME_TRACE_CBID_cudaLaunch_v3020) &&
      (cbid != CUPTI_RUNTIME_TRACE_CBID_cudaLaunchKernel_v7000)) {
    return;
  }

  RuntimeApiTrace_t *traceData = (RuntimeApiTrace_t*)userdata;
  
  // When entering the CUDA runtime function (before kernel launches)
  if (cbInfo->callbackSite == CUPTI_API_ENTER) {
    // Synchronize device to ensure clean event collection
    cudaDeviceSynchronize();
    
    // Set collection mode to kernel-level
    CUPTI_CALL(cuptiSetEventCollectionMode(cbInfo->context, CUPTI_EVENT_COLLECTION_MODE_KERNEL));

    // Enable the event group to start collecting data
    CUPTI_CALL(cuptiEventGroupEnable(traceData->eventData->eventGroup));
  }
  
  // When exiting the CUDA runtime function (after kernel completes)
  if (cbInfo->callbackSite == CUPTI_API_EXIT) {
    // Determine how many instances of the event occurred
    uint32_t numInstances = 0;
    size_t valueSize = 0;
    size_t bytesRead = 0;
    CUPTI_CALL(cuptiEventGroupGetAttribute(traceData->eventData->eventGroup, 
                               CUPTI_EVENT_GROUP_ATTR_INSTANCE_COUNT, 
                               &valueSize, &numInstances));
    
    // Allocate space for event values
    uint64_t *values = (uint64_t *) malloc(sizeof(uint64_t) * numInstances);
    
    // Make sure kernel is done
    cudaDeviceSynchronize();
    
    // Read the event values
    CUPTI_CALL(cuptiEventGroupReadEvent(traceData->eventData->eventGroup, 
                            CUPTI_EVENT_READ_FLAG_NONE, 
                            traceData->eventData->eventId, 
                            &bytesRead, values));
    
    // Aggregate values across all instances
    traceData->eventVal = 0;
    for (int i=0; i<numInstances; i++) {
      traceData->eventVal += values[i];
    }
    
    // Clean up
    free(values);
    cuptiEventGroupDisable(traceData->eventData->eventGroup);
  }
}