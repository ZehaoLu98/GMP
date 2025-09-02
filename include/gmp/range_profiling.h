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

#include "gmp/data_struct.h"

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

    void PrintProfilerRangesWithNames(const std::vector<GmpRangeData>& rangeDataVec)
    {
        size_t currProfilerKernelCounter = 0;
        for(int rangeIndex = 0; rangeIndex < rangeDataVec.size(); ++rangeIndex) {
            auto& rangeData = rangeDataVec[rangeIndex];
            std::cout << "Range Name: " << rangeData.name << "\n";
            std::cout << "======================================================================================\n";
            for(auto& kernelData : rangeData.kernelDataInRange) {
                std::cout << "Kernel: " << kernelData.name << 
                "<<<{" << kernelData.grid_size[0] << ", " << kernelData.grid_size[1] << ", " << kernelData.grid_size[2] << "}, {" 
                << kernelData.block_size[0] << ", " << kernelData.block_size[1] << ", " << kernelData.block_size[2] << "} >>>" << "\n";
                const auto &profilerRange = m_profilerRanges[currProfilerKernelCounter];
                std::cout << "-----------------------------------------------------------------------------------\n";
                for (const auto &metric : profilerRange.metricValues)
                {
                    std::cout << std::fixed << std::setprecision(3);
                    std::cout << std::setw(50) << std::left << metric.first;
                    std::cout << std::setw(30) << std::right << metric.second << "\n";
                }
                std::cout << "-----------------------------------------------------------------------------------\n";

                currProfilerKernelCounter++;
            }
        }
    }

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