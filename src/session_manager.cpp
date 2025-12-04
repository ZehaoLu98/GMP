#include "gmp/session_manager.h"
#include "gmp/log.h"
#include <cassert>

#ifdef USE_CUPTI
#include <cupti.h>
#endif

// SessionManager method implementations
std::string SessionManager::getSessionName(GmpProfileType type)
{
    if (ActivityMap.find(type) != ActivityMap.end() && !ActivityMap[type].empty())
    {
        return ActivityMap[type].back()->getSessionName();
    }
    else
    {
        GMP_LOG_ERROR("No active session of type " + std::to_string(static_cast<int>(type)) + " found.");
        return "";
    }
}

GmpResult SessionManager::reportAllSessions()
{
    for (auto &pair : ActivityMap)
    {
        for (const auto &sessionPtr : pair.second)
        {
            sessionPtr->report();
        }
    }
    return GmpResult::SUCCESS;
}

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

std::vector<GmpRangeData> SessionManager::getAllKernelDataOfType(GmpProfileType type)
{
    std::vector<GmpRangeData> allKernelData;
    for (const auto &sessionPtr : ActivityMap[type])
    {
        auto dataInRange = sessionPtr->getKernelData();
        allKernelData.push_back({sessionPtr->getSessionName(), dataInRange});
    }
    return allKernelData;
}

std::vector<GmpMemRangeData> SessionManager::getAllMemDataOfType(GmpProfileType type)
{
    std::vector<GmpMemRangeData> allMemData;
    for (const auto &sessionPtr : ActivityMap[type])
    {
        auto dataInRange = sessionPtr->getMemData();
        allMemData.push_back({sessionPtr->getSessionName(), dataInRange});
    }
    return allMemData;
}

// Explicit template instantiations
template GmpResult SessionManager::accumulate<GmpConcurrentKernelSession>(GmpProfileType, AccumulateFunc<GmpConcurrentKernelSession>);
template GmpResult SessionManager::accumulate<GmpMemSession>(GmpProfileType, AccumulateFunc<GmpMemSession>);