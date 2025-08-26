#ifndef GMP_RANGE_PROFILING_H
#define GMP_RANGE_PROFILING_H
//
// Copyright 2024 NVIDIA Corporation. All rights reserved
//

// System headers
#include <fstream>
#include <iostream>
#include <iomanip>
#include <memory>
#include <vector>
#include <unordered_map>
#include <memory>

// CUPTI headers
#include "helper_cupti.h"
#include <cupti_target.h>
#include <cupti_profiler_target.h>
#include <cupti_profiler_host.h>
#include <cupti_range_profiler.h>
#include <cupti.h>

struct ProfilerRange
{
    size_t rangeIndex;
    std::string rangeName;
    std::unordered_map<std::string, double> metricValues;
};

struct RangeProfilerConfig
{
    size_t maxNumOfRanges = 0;
    size_t numOfNestingLevel = 0;
    size_t minNestingLevel = 0;
};

class CuptiProfilerHost
{
public:
    CuptiProfilerHost() = default;
    ~CuptiProfilerHost() = default;

    void SetUp(std::string chipName, std::vector<uint8_t> &counterAvailibilityImage);
    void TearDown();

    CUptiResult CreateConfigImage(
        std::vector<const char *> metricsList,
        std::vector<uint8_t> &configImage);

    CUptiResult EvaluateCounterData(
        size_t rangeIndex,
        std::vector<const char *> metricsList,
        std::vector<uint8_t> &counterDataImage);

    CUptiResult GetNumOfRanges(
        std::vector<uint8_t> &counterDataImage,
        size_t &numOfRanges);

    void PrintProfilerRanges();

private:
    CUptiResult Initialize(std::vector<uint8_t> &counterAvailibilityImage);
    CUptiResult Deinitialize();

    std::string m_chipName;
    std::vector<ProfilerRange> m_profilerRanges;
    CUpti_Profiler_Host_Object *m_pHostObject = nullptr;
};

using CuptiProfilerHostPtr = std::shared_ptr<CuptiProfilerHost>;

class RangeProfilerTarget
{
public:
    RangeProfilerTarget(CUcontext ctx, RangeProfilerConfig &config);
    ~RangeProfilerTarget();

    CUptiResult EnableRangeProfiler();
    CUptiResult DisableRangeProfiler();

    CUptiResult StartRangeProfiler();
    CUptiResult StopRangeProfiler();

    CUptiResult PushRange(const char *rangeName);
    CUptiResult PopRange();

    CUptiResult SetConfig(
        CUpti_ProfilerRange range,
        CUpti_ProfilerReplayMode replayMode,
        std::vector<uint8_t> &configImage,
        std::vector<uint8_t> &counterDataImage);

    CUptiResult DecodeCounterData();
    CUptiResult CreateCounterDataImage(std::vector<const char *> &metrics, std::vector<uint8_t> &counterDataImage);

    bool IsAllPassSubmitted() const { return bIsAllPassSubmitted; }
    static CUptiResult GetChipName(size_t deviceIndex, std::string &chipName);
    static CUptiResult GetCounterAvailabilityImage(CUcontext ctx, std::vector<uint8_t> &counterAvailabilityImage);

private:
    CUcontext m_context = nullptr;
    size_t isProfilingActive = 0;
    bool bIsAllPassSubmitted = false;

    std::vector<const char *> metricNames = {};
    std::vector<uint8_t> configImage = {};
    RangeProfilerConfig mConfig = {};
    CUpti_RangeProfiler_Object *rangeProfilerObject = nullptr;
    bool bIsCuptiInitialized = false;
};

using RangeProfilerTargetPtr = std::shared_ptr<RangeProfilerTarget>;

#endif // GMP_RANGE_PROFILING_H