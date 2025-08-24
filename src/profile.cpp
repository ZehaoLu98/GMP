#include "gmp/profile.h"

GmpProfiler *GmpProfiler::instance = new GmpProfiler();

GmpResult SessionManager::startSession(GmpProfileType type, std::unique_ptr<GmpProfileSession> sessionPtr)
{
    assert(sessionPtr != nullptr);
    CUpti_SubscriberHandle runtimeSubscriber;
    if (ActivityMap[type].empty() || !ActivityMap[type].back()->isActive())
    {
        CUPTI_CALL(cuptiSubscribe(&runtimeSubscriber, (CUpti_CallbackFunc)getTimestampCallback, (void *)&sessionPtr->getRuntimeData()));
        CUPTI_CALL(cuptiEnableDomain(1, runtimeSubscriber, CUPTI_CB_DOMAIN_RUNTIME_API));
        sessionPtr->setRuntimeHandle(runtimeSubscriber);

        GMP_LOG_DEBUG("Session " + sessionPtr->getSessionName() + " of type " + std::to_string(static_cast<int>(type)) + " added.");

        ActivityMap[type].push_back(std::move(sessionPtr));

        return GmpResult::SUCCESS;
    }
    else
    {
        GMP_LOG_WARNING("Session " + ActivityMap[type].back()->getSessionName() + " of type " + std::to_string(static_cast<int>(type)) + " is already active. Cannot add a new session.");
        return GmpResult::ERROR;
    }
}

GmpResult SessionManager::endSession(GmpProfileType type)
{
    GMP_LOG_DEBUG("Ending session");
    if (ActivityMap[type].empty())
    {
        GMP_LOG_ERROR("No active session of type " + std::to_string(static_cast<int>(type)) + " found.");
        return GmpResult::ERROR;
    }
    auto &sessionPtr = ActivityMap[type].back();
    if (sessionPtr->isActive())
    {
        CUpti_SubscriberHandle subscriber = sessionPtr->getRuntimeSubscriberHandle();
        CUPTI_CALL(cuptiUnsubscribe(subscriber));

        sessionPtr->report();
        sessionPtr->deactivate();
        GMP_LOG_DEBUG("Session of type " + std::to_string(static_cast<int>(type)) + " ended.");
        return GmpResult::SUCCESS;
    }
    GMP_LOG_WARNING("Session of type " + std::to_string(static_cast<int>(type)) + " is already inactive.");
    return GmpResult::WARNING;
}

GmpProfiler::GmpProfiler(int maxNumOfRanges, int minNestingLevel, int numOfNestingLevel)
{
    instance = this;
    // CUPTI_CALL(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL));
    // CUPTI_CALL(cuptiActivityRegisterCallbacks(&GmpProfiler::bufferRequestedThunk,
    //                                           &GmpProfiler::bufferCompletedThunk));

    RangeProfilerConfig config;
    config.maxNumOfRanges = maxNumOfRanges;
    config.minNestingLevel = minNestingLevel;
    config.numOfNestingLevel = numOfNestingLevel;

    cuptiProfilerHost = std::make_shared<CuptiProfilerHost>();

    DRIVER_API_CALL(cuInit(0));
    // Get the current ctx for the device
    CUdevice cuDevice;
    DRIVER_API_CALL(cuDeviceGet(&cuDevice, 0));
    int computeCapabilityMajor = 0, computeCapabilityMinor = 0;
    DRIVER_API_CALL(cuDeviceGetAttribute(&computeCapabilityMajor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MAJOR, cuDevice));
    DRIVER_API_CALL(cuDeviceGetAttribute(&computeCapabilityMinor, CU_DEVICE_ATTRIBUTE_COMPUTE_CAPABILITY_MINOR, cuDevice));
    printf("Compute Capability of Device: %d.%d\n", computeCapabilityMajor, computeCapabilityMinor);

    // Create a context
    CUcontext cuContext;
    DRIVER_API_CALL(cuCtxCreate(&cuContext, 0, cuDevice));
    rangeProfilerTargetPtr = std::make_shared<RangeProfilerTarget>(cuContext, config);

    // Get chip name
    std::string chipName;
    CUPTI_CALL(RangeProfilerTarget::GetChipName(cuDevice, chipName));

    // Get Counter availability image
    std::vector<uint8_t> counterAvailabilityImage;
    CUPTI_CALL(RangeProfilerTarget::GetCounterAvailabilityImage(cuContext, counterAvailabilityImage));

    // Create config image
    std::vector<uint8_t> configImage;
    cuptiProfilerHost->SetUp(chipName, counterAvailabilityImage);
    CUPTI_CALL(cuptiProfilerHost->CreateConfigImage(metrics, configImage));

    // Enable Range profiler
    CUPTI_CALL(rangeProfilerTargetPtr->EnableRangeProfiler());

    // Create CounterData Image
    CUPTI_CALL(rangeProfilerTargetPtr->CreateCounterDataImage(metrics, counterDataImage));

    CUPTI_CALL(rangeProfilerTargetPtr->SetConfig(
        CUPTI_UserRange,
        CUPTI_UserReplay,
        configImage,
        counterDataImage));
}

GmpProfiler::~GmpProfiler()
{
    // CUPTI_CALL(cuptiActivityFlushAll(0));
    // CUPTI_CALL(cuptiActivityDisable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL));

    cuptiProfilerHost->TearDown();
}

// GmpResult GmpProfiler::RangeProfile(char *name, std::function<void()> func)
// {
//     CUPTI_API_CALL(rangeProfilerTargetPtr->PushRange(name));
//     func();
//     CUPTI_API_CALL(rangeProfilerTargetPtr->PopRange());
// }

GmpResult GmpProfiler::pushRange(const char *rangeName)
{
    if (rangeProfilerTargetPtr)
    {
        CUPTI_API_CALL(rangeProfilerTargetPtr->PushRange(rangeName));
        return GmpResult::SUCCESS;
    }
    else
    {
        GMP_LOG_ERROR("Range profiler target is not initialized.");
        return GmpResult::ERROR;
    }
}

GmpResult GmpProfiler::pushRange(const std::string &name, GmpProfileType type)
{
    switch (type)
    {
    case GmpProfileType::CONCURRENT_KERNEL:
        CUPTI_CALL(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL));
        return sessionManager.startSession(
            GmpProfileType::CONCURRENT_KERNEL,
            std::make_unique<GmpConcurrentKernelSession>(name));
    default:
        GMP_LOG_ERROR("Unsupported profile type: " + std::to_string(static_cast<int>(type)));
        return GmpResult::ERROR;
    }
}

GmpResult GmpProfiler::popRange()
{
    if (rangeProfilerTargetPtr)
    {
        CUPTI_API_CALL(rangeProfilerTargetPtr->PopRange());
        return GmpResult::SUCCESS;
    }
    else
    {
        GMP_LOG_ERROR("Range profiler target is not initialized.");
        return GmpResult::ERROR;
    }
}

GmpResult GmpProfiler::popRange(const std::string &name, GmpProfileType type)
{
    switch (type)
    {
    case GmpProfileType::CONCURRENT_KERNEL:
    {
        CUPTI_CALL(cuptiActivityDisable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL));
        GMP_LOG_DEBUG("Flusing all called");
        CUPTI_CALL(cuptiActivityFlushAll(0));
        GMP_LOG_DEBUG("Ending profiling for type: " + std::to_string(static_cast<int>(type)) + " with session name: " + sessionManager.getSessionName(type));
        GMP_API_CALL(sessionManager.endSession(type));
        return GmpResult::SUCCESS;
    }
    default:
    {
        GMP_LOG_ERROR("Unsupported profile type: " + std::to_string(static_cast<int>(type)));
        return GmpResult::ERROR;
    }
    }
}

void GmpProfiler::printProfilerRanges()
{
    if (cuptiProfilerHost)
    {
        // Evaluate the results
        size_t numRanges = 0;
        CUPTI_API_CALL(cuptiProfilerHost->GetNumOfRanges(counterDataImage, numRanges));
        printf("Number of ranges: %zu\n", numRanges);
        for (size_t rangeIndex = 0; rangeIndex < numRanges; ++rangeIndex)
        {
            CUPTI_API_CALL(cuptiProfilerHost->EvaluateCounterData(rangeIndex, metrics, counterDataImage));
        }

        cuptiProfilerHost->PrintProfilerRanges();
    }
    else
    {
        GMP_LOG_ERROR("Range profiler host is not initialized.");
    }
}

void GmpProfiler::bufferRequestedImpl(uint8_t **buffer, size_t *size, size_t *maxNumRecords)
{
    *size = 16 * 1024;
    *buffer = (uint8_t *)malloc(*size);
    *maxNumRecords = 0;
}

void GmpProfiler::bufferCompletedImpl(CUcontext ctx, uint32_t streamId,
                                      uint8_t *buffer, size_t size, size_t validSize)
{
    CUptiResult status;
    CUpti_Activity *record = nullptr;
    GMP_LOG_DEBUG("Buffer completion callback called");
    for (;;)
    {
        status = cuptiActivityGetNextRecord(buffer, validSize, &record);
        if (status == CUPTI_SUCCESS)
        {
            if (record->kind == CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL)
            {
                auto *kernel = (CUpti_ActivityKernel8 *)record;
                printf("CUPTI: Kernel \"%s\" launched on stream %u, grid (%u,%u,%u), block (%u,%u,%u)\n",
                       kernel->name, kernel->streamId,
                       kernel->gridX, kernel->gridY, kernel->gridZ,
                       kernel->blockX, kernel->blockY, kernel->blockZ);
                auto result = sessionManager.accumulate<GmpConcurrentKernelSession>(
                    GmpProfileType::CONCURRENT_KERNEL,
                    [](GmpConcurrentKernelSession *sessionPtr)
                    { sessionPtr->num_calls++; });
                if (result != GmpResult::SUCCESS)
                {
                    GMP_LOG_ERROR("Failed to accumulate concurrent kernel session.");
                }
            }
        }
        else if (status == CUPTI_ERROR_MAX_LIMIT_REACHED)
        {
            break;
        }
        else
        {
            CUPTI_CALL(status);
        }
    }
    size_t dropped = 0;
    cuptiActivityGetNumDroppedRecords(ctx, streamId, &dropped);
    if (dropped != 0)
    {
        printf("CUPTI: Dropped %zu activity records\n", dropped);
    }
    free(buffer);
    GMP_LOG_DEBUG("Buffer completion callback ended");
}