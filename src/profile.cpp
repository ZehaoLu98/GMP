#include "gmp/profile.h"
#include <nvtx3/nvtx3.hpp>

GmpProfiler *GmpProfiler::instance = nullptr;

// Create a c style array of char* from std::vector<std::string>
std::vector<const char*> createCStyleStringArray(const std::vector<std::string>& strVec) {
    std::vector<const char*> cStrArray;
    for (const auto& str : strVec) {
        cStrArray.push_back(str.c_str());
    }
    return cStrArray;
}


#ifdef USE_CUPTI
GmpResult SessionManager::startSession(GmpProfileType type, std::unique_ptr<GmpProfileSession> sessionPtr)
{
    assert(sessionPtr != nullptr);
    if (ActivityMap[type].empty() || !ActivityMap[type].back()->isActive())
    {
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
#ifdef ENABLE_NVTX
    // Clean up any remaining NVTX ranges
    if (isEnabled)
    {
        nvtxManager_.clearAllRanges();
    }
#endif
#ifdef USE_CUPTI
    CUPTI_CALL(cuptiActivityFlushAll(1));
    CUPTI_CALL(cuptiActivityDisable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL));

    cuptiProfilerHost->TearDown();
#endif
}

GmpResult GmpProfiler::pushRange(const std::string &name, GmpProfileType type)
{
#ifdef ENABLE_NVTX
    if (isEnabled)
    {
        std::cout << "Pushing NVTX range: " << name << std::endl;
        nvtxManager_.startRange(name);
    }
#endif
#ifdef USE_CUPTI
    if (!isEnabled)
    {
        return GmpResult::SUCCESS;
    }
    // Remove all the activity records that is before the range.
    cudaDeviceSynchronize();
    cuptiActivityFlushAll(1);
    GMP_LOG_DEBUG("Pushed range for type: " + std::to_string(static_cast<int>(type)) + " with session name: " + name);

    switch (type)
    {
    case GmpProfileType::CONCURRENT_KERNEL:
        GMP_API_CALL(getInstance()->sessionManager.startSession(type, std::make_unique<GmpConcurrentKernelSession>(name)));
        pushRangeProfilerRange(name.c_str());
        break;
    case GmpProfileType::MEMORY:
        GMP_API_CALL(getInstance()->sessionManager.startSession(type, std::make_unique<GmpMemSession>(name)));
        break;
    default:
        GMP_LOG_ERROR("Unsupported profile type: " + std::to_string(static_cast<int>(type)));
        return GmpResult::ERROR;
    }

    return GmpResult::SUCCESS;
#else
    return GmpResult::SUCCESS;
#endif
}

GmpResult GmpProfiler::pushRangeProfilerRange(const char *rangeName)
{
#ifdef USE_CUPTI
    if (!isEnabled)
    {
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

GmpResult GmpProfiler::popRangeProfilerRange()
{
#ifdef USE_CUPTI
    if (!isEnabled)
    {
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
#ifdef ENABLE_NVTX
    if (isEnabled)
    {
        nvtxManager_.endRange(name);
    }
#endif
#ifdef USE_CUPTI
    if (!isEnabled)
    {
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
    case GmpProfileType::MEMORY:
    {
        cudaDeviceSynchronize();

        // This ensures that all the memory activity records
        // within the range are collected to the correct session.
        CUPTI_CALL(cuptiActivityFlushAll(1));
        GMP_LOG_DEBUG("Popped memory range for type: " + std::to_string(static_cast<int>(type)) + " with session name: " + name);
        GMP_API_CALL(sessionManager.endSession(type));
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

void GmpProfiler::printProfilerRanges(GmpOutputKernelReduction option)
{
    std::vector<const char*> c_metrics = createCStyleStringArray(metrics);
#ifdef USE_CUPTI
    if (cuptiProfilerHost)
    {
        // Evaluate the results
        size_t numRanges = 0;
        CUPTI_API_CALL(cuptiProfilerHost->GetNumOfRanges(counterDataImage, numRanges));
        printf("Number of ranges: %zu\n", numRanges);
        for (size_t rangeIndex = 0; rangeIndex < numRanges; ++rangeIndex)
        {
            CUPTI_API_CALL(cuptiProfilerHost->EvaluateCounterData(rangeIndex, c_metrics, counterDataImage));
        }

        // cuptiProfilerHost->PrintProfilerRanges();
        GMP_API_CALL(checkActivityAndRangeResultMatch());
        auto activityAllRangeData = sessionManager.getAllKernelDataOfType(GmpProfileType::CONCURRENT_KERNEL);
        cuptiProfilerHost->PrintProfilerRangesWithNames(activityAllRangeData);
        produceOutput(option);
    }
    else
    {
        GMP_LOG_ERROR("Range profiler host is not initialized.");
    }
#endif
}

void GmpProfiler::printMemoryActivity()
{
#ifdef USE_CUPTI
    if (!isEnabled)
    {
        printf("GMP Profiler is disabled.\n");
        return;
    }

    printf("\n=== Memory Activity Report ===\n");

    // Get memory data from MEMORY type sessions
    auto allMemRangeData = sessionManager.getAllMemDataOfType(GmpProfileType::MEMORY);

    if (allMemRangeData.empty())
    {
        printf("No memory activity ranges found.\n");
        return;
    }

    printf("Total memory activity ranges: %zu\n\n", allMemRangeData.size());

    for (size_t rangeIdx = 0; rangeIdx < allMemRangeData.size(); rangeIdx++)
    {
        const auto &memRange = allMemRangeData[rangeIdx];
        printf("Range %zu: %s\n", rangeIdx + 1, memRange.name.c_str());
        printf("  Memory operations: %zu\n", memRange.memDataInRange.size());

        if (memRange.memDataInRange.empty())
        {
            printf("  No memory operations recorded.\n\n");
            continue;
        }

        // Categorize memory operations
        uint64_t totalBytesAllocated = 0;
        uint64_t totalBytesFreed = 0;
        uint64_t totalBytesTransferred = 0;
        size_t allocCount = 0;
        size_t freeCount = 0;
        size_t transferCount = 0;

        for (const auto &memData : memRange.memDataInRange)
        {
            switch (memData.memoryOperationType)
            {
            case CUPTI_ACTIVITY_MEMORY_OPERATION_TYPE_ALLOCATION:
                totalBytesAllocated += memData.bytes;
                allocCount++;
                break;
            case CUPTI_ACTIVITY_MEMORY_OPERATION_TYPE_RELEASE:
                totalBytesFreed += memData.bytes;
                freeCount++;
                break;
            default:
                break;
            }
        }

        // Print summary statistics
        printf("  Summary:\n");
        printf("    Allocations: %zu operations, %llu bytes (%.2f MB)\n",
               allocCount, totalBytesAllocated, totalBytesAllocated / 1024.0 / 1024.0);
        printf("    Deallocations: %zu operations, %llu bytes (%.2f MB)\n",
               freeCount, totalBytesFreed, totalBytesFreed / 1024.0 / 1024.0);

        // Print detailed memory operations
        printf("  Detailed operations:\n");
        for (size_t i = 0; i < memRange.memDataInRange.size(); i++)
        {
            const auto &memData = memRange.memDataInRange[i];

            const char *opType = "";
            switch (memData.memoryOperationType)
            {
            case CUPTI_ACTIVITY_MEMORY_OPERATION_TYPE_ALLOCATION:
                opType = "ALLOC";
                break;
            case CUPTI_ACTIVITY_MEMORY_OPERATION_TYPE_RELEASE:
                opType = "FREE";
                break;
            default:
                opType = "UNKNOWN";
                break;
            }

            const char *memKind = "";
            switch (memData.memoryKind)
            {
            case CUPTI_ACTIVITY_MEMORY_KIND_DEVICE:
                memKind = "DEVICE";
                break;
            case CUPTI_ACTIVITY_MEMORY_KIND_MANAGED:
                memKind = "MANAGED";
                break;
            case CUPTI_ACTIVITY_MEMORY_KIND_PINNED:
                memKind = "PINNED";
                break;
            default:
                memKind = "UNKNOWN";
                break;
            }

            printf("    [%zu] %s %s: %llu bytes at 0x%016llx",
                   i + 1, opType, memKind, memData.bytes, memData.address);

            if (memData.name && strlen(memData.name) > 0)
            {
                printf(" (%s)", memData.name);
            }

            if (memData.isAsync)
            {
                printf(" [ASYNC, Stream %u]", memData.streamId);
            }

            printf(" [Device %u, Context %u, Correlation %u]\n",
                   memData.deviceId, memData.contextId, memData.correlationId);
        }

        printf("\n");
    }

    printf("=== End Memory Activity Report ===\n\n");
#else
    printf("CUPTI support is not enabled. Memory activity profiling is not available.\n");
#endif
}

std::vector<GmpMemRangeData> GmpProfiler::getMemoryActivity()
{
#ifdef USE_CUPTI
    if (!isEnabled)
    {
        return std::vector<GmpMemRangeData>();
    }
    return sessionManager.getAllMemDataOfType(GmpProfileType::MEMORY);
#else
    return std::vector<GmpMemRangeData>();
#endif
}

void GmpProfiler::produceOutput(GmpOutputKernelReduction option)
{
#ifdef USE_CUPTI
    std::string path = "./output/result.csv";

    auto sumFunc = [](const std::vector<ProfilerRange> &ranges, size_t startIndex, size_t size)
    {
        std::unordered_map<std::string, double> combinedMetrics;
        for (size_t i = startIndex; i < startIndex + size && i < ranges.size(); ++i)
        {
            for (const auto &metric : ranges[i].metricValues)
            {
                combinedMetrics[metric.first] += metric.second;
            }
        }
        return combinedMetrics;
    };

    auto maxFunc = [](const std::vector<ProfilerRange> &ranges, size_t startIndex, size_t size)
    {
        std::unordered_map<std::string, double> maxMetrics;
        for (size_t i = startIndex; i < startIndex + size && i < ranges.size(); ++i)
        {
            for (const auto &metric : ranges[i].metricValues)
            {
                if (maxMetrics.find(metric.first) == maxMetrics.end() || metric.second > maxMetrics[metric.first])
                {
                    maxMetrics[metric.first] = metric.second;
                }
            }
        }
        return maxMetrics;
    };

    auto meanFunc = [](const std::vector<ProfilerRange> &ranges, size_t startIndex, size_t size)
    {
        std::unordered_map<std::string, double> meanMetrics;
        for (size_t i = startIndex; i < startIndex + size && i < ranges.size(); ++i)
        {
            for (const auto &metric : ranges[i].metricValues)
            {
                meanMetrics[metric.first] += metric.second;
            }
        }
        for (auto &metric : meanMetrics)
        {
            metric.second /= size;
        }
        return meanMetrics;
    };

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
        if (kernelNum == 0)
        {
            GMP_LOG_DEBUG("Skipping kernel reduction for range '" + activityRange.name + "' because it contains no kernel records.");
            continue;
        }
        outputFile.precision(2);

        std::unordered_map<std::string, double> reducedMetrics;
        switch (option)
        {
        case GmpOutputKernelReduction::SUM:
            reducedMetrics = cuptiProfilerHost->getMetrics(rangeProfileOffset, kernelNum, sumFunc);
            break;
        case GmpOutputKernelReduction::MAX:
            reducedMetrics = cuptiProfilerHost->getMetrics(rangeProfileOffset, kernelNum, maxFunc);
            break;
        case GmpOutputKernelReduction::MEAN:
            reducedMetrics = cuptiProfilerHost->getMetrics(rangeProfileOffset, kernelNum, meanFunc);
            break;
        default:
            break;
        }

        for (auto metricsPair : reducedMetrics)
        {
            outputFile << std::fixed << activityRange.name << "," << metricsPair.first << "," << metricsPair.second << "\n";
        }
        rangeProfileOffset += kernelNum;
    }
    outputFile.close();
#endif
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
            else if (record->kind == CUPTI_ACTIVITY_KIND_MEMORY2)
            {
                auto *memRecord = (CUpti_ActivityMemory4 *)record;

                auto result = sessionManager.accumulate<GmpMemSession>(
                    GmpProfileType::MEMORY,
                    [&memRecord](GmpMemSession *sessionPtr)
                    {
                        // printf("CUPTI: Kernel \"%s\" launched on stream %u, grid (%u,%u,%u), block (%u,%u,%u)\n",
                        //         kernel->name, kernel->streamId,
                        //         kernel->gridX, kernel->gridY, kernel->gridZ,
                        //         kernel->blockX, kernel->blockY, kernel->blockZ);
                        sessionPtr->num_calls++;
                        GmpMemData data;
                        data.name = memRecord->name;
                        data.source = memRecord->source;
                        data.memoryOperationType = memRecord->memoryOperationType;
                        data.memoryKind = memRecord->memoryKind;
                        data.correlationId = memRecord->correlationId;
                        data.address = memRecord->address;
                        data.bytes = memRecord->bytes;
                        data.timestamp = memRecord->timestamp;
                        data.PC = memRecord->PC;
                        data.processId = memRecord->processId;
                        data.deviceId = memRecord->deviceId;
                        data.contextId = memRecord->contextId;
                        data.streamId = memRecord->streamId;
                        data.isAsync = memRecord->isAsync;

                        sessionPtr->pushMemData(data);
                    });
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
    // create a copy of metrics as c strings
    std::vector<const char*> c_metrics = createCStyleStringArray(metrics);

#ifdef USE_CUPTI

    // Initialize CUPTI Activity API
    CUPTI_CALL(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_CONCURRENT_KERNEL));
    CUPTI_CALL(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_MEMORY2));
    CUPTI_CALL(cuptiActivityRegisterCallbacks(&GmpProfiler::bufferRequestedThunk,
                                              &GmpProfiler::bufferCompletedThunk));
    instance->cuptiProfilerHost = std::make_shared<CuptiProfilerHost>();
    cuInit(0);

    CUresult init_result = cuDriverGetVersion(nullptr);
    if (init_result != CUDA_SUCCESS)
    {
        printf("Initializing CUDA driver...\n");
        cuInit(0);
    }
    else
    {
        printf("CUDA driver already initialized\n");
    }

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

    // Retain current context
    CUcontext cuContext;
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
    CUPTI_CALL(instance->cuptiProfilerHost->CreateConfigImage(c_metrics, configImage));

    // Enable Range profiler
    CUPTI_CALL(instance->rangeProfilerTargetPtr->EnableRangeProfiler());

    // Create CounterData Image
    CUPTI_CALL(instance->rangeProfilerTargetPtr->CreateCounterDataImage(c_metrics, instance->counterDataImage));

    CUPTI_CALL(instance->rangeProfilerTargetPtr->SetConfig(
        ENABLE_USER_RANGE ? CUPTI_UserRange : CUPTI_AutoRange,
        ENABLE_USER_RANGE ? CUPTI_UserReplay : CUPTI_KernelReplay,
        configImage,
        instance->counterDataImage));
#endif
    isInitialized = true;
}

GmpResult GmpProfiler::checkActivityAndRangeResultMatch()
{
#ifdef USE_CUPTI
    if (!isEnabled)
    {
        return GmpResult::SUCCESS;
    }

    auto allRangeActivityData = sessionManager.getAllKernelDataOfType(GmpProfileType::CONCURRENT_KERNEL);
    size_t activityRecordCount = 0;
    for (auto &rangeData : allRangeActivityData)
    {
        activityRecordCount += rangeData.kernelDataInRange.size();
    }

    size_t kernelInRangeProfilerRange = 0;
    cuptiProfilerHost->GetNumOfRanges(counterDataImage, kernelInRangeProfilerRange);
    if (activityRecordCount != kernelInRangeProfilerRange)
    {
        GMP_LOG_ERROR("Kernel activity range and range profiler range do not match.");
        return GmpResult::ERROR;
    }
#endif
    return GmpResult::SUCCESS;
}
