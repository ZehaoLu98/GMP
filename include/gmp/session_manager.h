#ifndef GMP_SESSION_MANAGER_H
#define GMP_SESSION_MANAGER_H

#include <map>
#include <memory>
#include <vector>
#include <functional>

#include "gmp/session.h"
#include "gmp/data_struct.h"

class SessionManager
{
public:
  SessionManager() = default;

  std::string getSessionName(GmpProfileType type);

  template <typename DerivedSession>
  using AccumulateFunc = std::function<void(DerivedSession *)>;

  // Apply the provided callback function to the active session of that type.
  template <typename DerivedSession>
  GmpResult accumulate(GmpProfileType type, AccumulateFunc<DerivedSession> callback);

  GmpResult reportAllSessions();

  GmpResult startSession(GmpProfileType type, std::unique_ptr<GmpProfileSession> sessionPtr);

  GmpResult endSession(GmpProfileType type);

  std::vector<GmpRangeData> getAllKernelDataOfType(GmpProfileType type);

  std::vector<GmpMemRangeData> getAllMemDataOfType(GmpProfileType type);

private:
  std::map<GmpProfileType, std::vector<std::unique_ptr<GmpProfileSession>>> ActivityMap;
};

// Template function implementations
template <typename DerivedSession>
GmpResult SessionManager::accumulate(GmpProfileType type, AccumulateFunc<DerivedSession> callback)
{
    if (!ActivityMap[type].empty())
    {
        auto &sessionPtr = ActivityMap[type].back();
        if (auto derivedSessionPtr = dynamic_cast<DerivedSession *>(sessionPtr.get()))
        {
            if (sessionPtr->isActive())
            {
                callback(derivedSessionPtr);
            }
            return GmpResult::SUCCESS;
        }
        else
        {
            return GmpResult::ERROR;
        }
    }
    return GmpResult::SUCCESS;
}

#endif // GMP_SESSION_MANAGER_H