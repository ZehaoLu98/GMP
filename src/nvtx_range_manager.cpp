#include "gmp/nvtx_range_manager.h"

#ifdef ENABLE_NVTX
// NvtxRangeManager method implementations
nvtxRangeId_t NvtxRangeManager::startRange(const std::string& name) {
    nvtxRangeId_t rangeId = nvtxRangeStartA(name.c_str());
    activeRanges_.push(rangeId);
    rangeNameMap_[rangeId] = name;
    return rangeId;
}

bool NvtxRangeManager::endRange(const std::string& expectedName) {
    if (activeRanges_.empty()) {
        return false;
    }
    
    nvtxRangeId_t rangeId = activeRanges_.top();
    activeRanges_.pop();
    
    // Optional: verify the name matches what we expect
    if (!expectedName.empty() && rangeNameMap_[rangeId] != expectedName) {
        // Log warning but still end the range
        // Note: In a production system, you might want to handle this differently
    }
    
    nvtxRangeEnd(rangeId);
    rangeNameMap_.erase(rangeId);
    return true;
}

size_t NvtxRangeManager::getActiveRangeCount() const {
    return activeRanges_.size();
}

void NvtxRangeManager::clearAllRanges() {
    while (!activeRanges_.empty()) {
        nvtxRangeId_t rangeId = activeRanges_.top();
        activeRanges_.pop();
        nvtxRangeEnd(rangeId);
        rangeNameMap_.erase(rangeId);
    }
}
#endif // ENABLE_NVTX