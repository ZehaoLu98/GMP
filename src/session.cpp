#include "gmp/session.h"

// GmpProfileSession method implementations
bool GmpProfileSession::isActive() const { 
    return is_active; 
}

void GmpProfileSession::deactivate() { 
    is_active = false; 
}

std::string GmpProfileSession::getSessionName()
{
    return sessionName;
}

void GmpProfileSession::setRuntimeData(const ApiRuntimeRecord &data)
{
    runtimeData = data;
}

const ApiRuntimeRecord &GmpProfileSession::getRuntimeData() const
{
    return runtimeData;
}

#ifdef USE_CUPTI
CUpti_SubscriberHandle GmpProfileSession::getRuntimeSubscriberHandle() const
{
    return runtimeSubscriber;
}

void GmpProfileSession::setRuntimeHandle(CUpti_SubscriberHandle runtimeSubscriber)
{
    this->runtimeSubscriber = runtimeSubscriber;
}
#endif

void GmpProfileSession::pushKernelData(const GmpKernelData &data)
{
    kernelData.push_back(data);
}

void GmpProfileSession::pushMemData(const GmpMemData &data)
{
    memData.push_back(data);
}

std::vector<GmpKernelData> GmpProfileSession::getKernelData() const
{
    return kernelData;
}

std::vector<GmpMemData> GmpProfileSession::getMemData() const
{
    return memData;
}

// GmpConcurrentKernelSession method implementations
GmpConcurrentKernelSession::GmpConcurrentKernelSession(const std::string &sessionName)
    : GmpProfileSession(sessionName) {}

void GmpConcurrentKernelSession::report() const
{
    // GMP_LOG_DEBUG("Session " + sessionName.c_str() + " captured " + std::to_string(num_calls) + " calls");
}

// GmpMemSession method implementations
GmpMemSession::GmpMemSession(const std::string &sessionName)
    : GmpProfileSession(sessionName) {}

void GmpMemSession::report() const
{
    // GMP_LOG_DEBUG("Session " + sessionName.c_str() + " captured " + std::to_string(num_calls) + " calls");
}