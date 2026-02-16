#include "uxdi/DetectorManager.h"
#include "uxdi/DetectorFactory.h"
#include <algorithm>
#include <stdexcept>

namespace uxdi {

DetectorManager::DetectorManager()
    : m_nextDetectorId(1)
{
}

DetectorManager::~DetectorManager() {
    // Destroy all detectors on shutdown
    DestroyAllDetectors();
}

size_t DetectorManager::CreateDetector(size_t adapterId, const std::string& config) {
    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        // Use DetectorFactory to create the detector
        auto detector = DetectorFactory::CreateDetector(adapterId, config);

        if (!detector) {
            return 0; // Failed to create detector
        }

        // Assign a unique detector ID
        size_t detectorId = m_nextDetectorId++;

        // Add detector entry to registry
        m_detectors.emplace_back(detectorId, adapterId, std::move(detector));

        return detectorId;
    }
    catch (const std::exception&) {
        // DetectorFactory::CreateDetector throws on invalid adapterId or creation failure
        return 0;
    }
}

void DetectorManager::DestroyDetector(size_t detectorId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = FindDetectorEntry(detectorId);
    if (it != m_detectors.end()) {
        // Remove from vector (unique_ptr auto-deletes the detector)
        m_detectors.erase(it);
    }
    // If detectorId not found, silently ignore (idempotent operation)
}

IDetector* DetectorManager::GetDetector(size_t detectorId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = FindDetectorEntry(detectorId);
    if (it != m_detectors.end()) {
        return it->detector.get();
    }
    return nullptr;
}

bool DetectorManager::AddListener(size_t detectorId, IDetectorListener* listener) {
    if (!listener) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = FindDetectorEntry(detectorId);
    if (it == m_detectors.end()) {
        return false; // Detector not found
    }

    // Check for duplicate listener
    auto& listeners = it->listeners;
    if (std::find(listeners.begin(), listeners.end(), listener) != listeners.end()) {
        return false; // Already registered
    }

    // Add listener to the list
    listeners.push_back(listener);
    return true;
}

bool DetectorManager::RemoveListener(size_t detectorId, IDetectorListener* listener) {
    if (!listener) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = FindDetectorEntry(detectorId);
    if (it == m_detectors.end()) {
        return false; // Detector not found
    }

    auto& listeners = it->listeners;
    auto listenerIt = std::find(listeners.begin(), listeners.end(), listener);

    if (listenerIt != listeners.end()) {
        listeners.erase(listenerIt);
        return true;
    }

    return false; // Listener not found
}

size_t DetectorManager::RemoveAllListeners(size_t detectorId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = FindDetectorEntry(detectorId);
    if (it == m_detectors.end()) {
        return 0; // Detector not found
    }

    size_t count = it->listeners.size();
    it->listeners.clear();
    return count;
}

DetectorState DetectorManager::GetState(size_t detectorId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = FindDetectorEntry(detectorId);
    if (it != m_detectors.end() && it->detector) {
        return it->detector->getState();
    }
    return DetectorState::UNKNOWN;
}

DetectorInfo DetectorManager::GetInfo(size_t detectorId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = FindDetectorEntry(detectorId);
    if (it != m_detectors.end() && it->detector) {
        return it->detector->getDetectorInfo();
    }
    return DetectorInfo{}; // Return empty info if not found
}

void DetectorManager::DestroyAllDetectors() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Clear all detectors (unique_ptrs auto-delete)
    m_detectors.clear();
}

size_t DetectorManager::GetDetectorCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_detectors.size();
}

std::vector<size_t> DetectorManager::GetDetectorIds() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<size_t> ids;
    ids.reserve(m_detectors.size());

    for (const auto& entry : m_detectors) {
        ids.push_back(entry.id);
    }

    return ids;
}

bool DetectorManager::IsValidDetector(size_t detectorId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return FindDetectorEntry(detectorId) != m_detectors.end();
}

std::vector<DetectorManager::DetectorEntry>::iterator
DetectorManager::FindDetectorEntry(size_t detectorId) {
    return std::find_if(m_detectors.begin(), m_detectors.end(),
        [detectorId](const DetectorEntry& entry) {
            return entry.id == detectorId;
        });
}

std::vector<DetectorManager::DetectorEntry>::const_iterator
DetectorManager::FindDetectorEntry(size_t detectorId) const {
    return std::find_if(m_detectors.begin(), m_detectors.end(),
        [detectorId](const DetectorEntry& entry) {
            return entry.id == detectorId;
        });
}

} // namespace uxdi
