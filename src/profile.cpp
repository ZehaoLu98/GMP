#include "gmp/profile.h"

GmpProfiler *GmpProfiler::instance = nullptr;

#ifdef USE_CUPTI
GmpResult SessionManager::startSession(GmpProfileType type, std::unique_ptr<GmpProfileSession> sessionPtr)
{
    assert(sessionPtr != nullptr);
    if (ActivityMap[type].empty() || !ActivityMap[type].back()->isActive())
    {
        // CUPTI_CALL(cuptiSubscribe(&runtimeSubscriber, (CUpti_CallbackFunc)getTimestampCallback, (void *)&sessionPtr->getRuntimeData()));
        // CUPTI_CALL(cuptiEnableDomain(1, runtimeSubscriber, CUPTI_CB_DOMAIN_RUNTIME_API));
        // sessionPtr->setRuntimeHandle(runtimeSubscriber);

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
#ifdef USE_CUPTI
    GMP_LOG_DEBUG("Ending session");
    if (ActivityMap[type].empty())
    {
        GMP_LOG_ERROR("No active session of type " + std::to_string(static_cast<int>(type)) + " found.");
        return GmpResult::ERROR;
    }
    auto &sessionPtr = ActivityMap[type].back();
    if (sessionPtr->isActive())
    {
        // CUpti_SubscriberHandle subscriber = sessionPtr->getRuntimeSubscriberHandle();
        // CUPTI_CALL(cuptiUnsubscribe(subscriber));

        sessionPtr->report();
        sessionPtr->deactivate();
        GMP_LOG_DEBUG("Session of type " + std::to_string(static_cast<int>(type)) + " ended.");
        return GmpResult::SUCCESS;
    }
    GMP_LOG_WARNING("Session of type " + std::to_string(static_cast<int>(type)) + " is already inactive.");
    return GmpResult::WARNING;
#else
    return GmpResult::SUCCESS;
#endif
}

#endif

GmpProfiler::GmpProfiler()
{
    isEnabled = true;
}

GmpProfiler::~GmpProfiler()
{
#ifdef USE_CUPTI
    CUPTI_CALL(cuptiActivityFlushAll(1));
    CUPTI_CALL(cuptiActivityDisable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL));

    cuptiProfilerHost->TearDown();
#endif
}

GmpResult GmpProfiler::pushRangeProfilerRange(const char *rangeName)
{
#ifdef USE_CUPTI
    if(!isEnabled){
        return GmpResult::SUCCESS;
    }
    if (rangeProfilerTargetPtr)
    {
        cudaDeviceSynchronize();
        CUPTI_API_CALL(rangeProfilerTargetPtr->PushRange(rangeName));
        return GmpResult::SUCCESS;
    }
    else
    {
        GMP_LOG_ERROR("Range profiler target is not initialized.");
        return GmpResult::ERROR;
    }
#else
    return GmpResult::SUCCESS;
#endif
}

GmpResult GmpProfiler::pushRange(const std::string &name, GmpProfileType type)
{
#ifdef USE_CUPTI
    if(!isEnabled){
        return GmpResult::SUCCESS;
    }
    // Remove all the activity records that is before the range.
    cudaDeviceSynchronize();
    cuptiActivityFlushAll(1);
    GMP_LOG_DEBUG("Pushed range for type: " + std::to_string(static_cast<int>(type)) + " with session name: " + name);
    GMP_API_CALL(getInstance()->sessionManager.startSession(type, std::make_unique<GmpConcurrentKernelSession>(name)));
    pushRangeProfilerRange(name.c_str());
    return GmpResult::SUCCESS;
#else
    return GmpResult::SUCCESS;
#endif
}

GmpResult GmpProfiler::popRangeProfilerRange()
{
#ifdef USE_CUPTI
    if(!isEnabled){
        return GmpResult::SUCCESS;
    }
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
#else
    return GmpResult::SUCCESS;
#endif
}

GmpResult GmpProfiler::popRange(const std::string &name, GmpProfileType type)
{
#ifdef USE_CUPTI
    if(!isEnabled){
        return GmpResult::SUCCESS;
    }
    switch (type)
    {
    case GmpProfileType::CONCURRENT_KERNEL:
    {
        cudaDeviceSynchronize();

        // This ensures that all the records of kernels launched
        // within the range are collected to the correct session.
        CUPTI_CALL(cuptiActivityFlushAll(1));
        GMP_LOG_DEBUG("Popped range for type: " + std::to_string(static_cast<int>(type)) + " with session name: " + name);
        GMP_API_CALL(sessionManager.endSession(type));
        popRangeProfilerRange();
        return GmpResult::SUCCESS;
    }
    default:
    {
        GMP_LOG_ERROR("Unsupported profile type: " + std::to_string(static_cast<int>(type)));
        return GmpResult::ERROR;
    }
    }
#else
    return GmpResult::SUCCESS;
#endif
}

void GmpProfiler::printProfilerRanges()
{
#ifdef USE_CUPTI
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

        // cuptiProfilerHost->PrintProfilerRanges();
        GMP_API_CALL(checkActivityAndRangeResultMatch());
        auto activityAllRangeData = sessionManager.getAllKernelDataOfType(GmpProfileType::CONCURRENT_KERNEL);
        
        // cuptiProfilerHost->PrintProfilerRangesWithNames(allKernelData);

        produceOutput();
    }
    else
    {
        GMP_LOG_ERROR("Range profiler host is not initialized.");
    }
#endif
}

void GmpProfiler::produceOutput()
{
    std::string path = "./output/result.csv";
    std::ofstream outputFile(path, std::ios::app);
    if (!outputFile.is_open())
    {
        GMP_LOG_ERROR("Failed to open output file: " + path);
        return;
    }

    auto activityAllRangeData = sessionManager.getAllKernelDataOfType(GmpProfileType::CONCURRENT_KERNEL);

    size_t rangeProfileOffset = 0;
    for (int activityRangeIdx = 0; activityRangeIdx < activityAllRangeData.size(); activityRangeIdx++)
    {
        const auto &activityRange = activityAllRangeData[activityRangeIdx];
        auto kernelNum = activityRange.kernelDataInRange.size();
        auto accumulatedMetrics = cuptiProfilerHost->getMetrics(rangeProfileOffset, kernelNum);
        outputFile.precision(2);
        for (auto metricsPair : accumulatedMetrics)
        {
            outputFile << std::fixed << activityRange.name << "," << metricsPair.first << "," << metricsPair.second << "\n";
        }
        rangeProfileOffset += kernelNum;
    }
    outputFile.close();
}

void GmpProfiler::bufferRequestedImpl(uint8_t **buffer, size_t *size, size_t *maxNumRecords)
{
#ifdef USE_CUPTI
    *size = 16 * 1024;
    *buffer = (uint8_t *)malloc(*size);
    *maxNumRecords = 0;
#endif
}

void GmpProfiler::bufferCompletedImpl(CUcontext ctx, uint32_t streamId,
                                      uint8_t *buffer, size_t size, size_t validSize)
{
#ifdef USE_CUPTI
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
                auto result = sessionManager.accumulate<GmpConcurrentKernelSession>(
                    GmpProfileType::CONCURRENT_KERNEL,
                    [&kernel](GmpConcurrentKernelSession *sessionPtr)
                    {
                        // printf("CUPTI: Kernel \"%s\" launched on stream %u, grid (%u,%u,%u), block (%u,%u,%u)\n",
                        //         kernel->name, kernel->streamId,
                        //         kernel->gridX, kernel->gridY, kernel->gridZ,
                        //         kernel->blockX, kernel->blockY, kernel->blockZ);
                        sessionPtr->num_calls++;
                        GmpKernelData data;
                        data.name = kernel->name;
                        data.grid_size[0] = kernel->gridX;
                        data.grid_size[1] = kernel->gridY;
                        data.grid_size[2] = kernel->gridZ;
                        data.block_size[0] = kernel->blockX;
                        data.block_size[1] = kernel->blockY;
                        data.block_size[2] = kernel->blockZ;
                        sessionPtr->pushKernelData(data);
                    });
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
#endif
}

  void GmpProfiler::init()
  {
#ifdef USE_CUPTI
      CUPTI_CALL(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL));
      CUPTI_CALL(cuptiActivityRegisterCallbacks(&GmpProfiler::bufferRequestedThunk,
                                                &GmpProfiler::bufferCompletedThunk));
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
          ENABLE_USER_RANGE ? CUPTI_UserRange : CUPTI_AutoRange,
          ENABLE_USER_RANGE ? CUPTI_UserReplay : CUPTI_KernelReplay,
          configImage,
          instance->counterDataImage));
#endif
      isInitialized = true;
  }