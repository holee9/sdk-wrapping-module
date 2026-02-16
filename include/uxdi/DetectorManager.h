#pragma once

#include <uxdi/IDetector.h>
#include <uxdi/IDetectorListener.h>
#include <uxdi/Types.h>
#include <uxdi/uxdi_export.h>
#include <uxdi/DetectorFactory.h>  // For DetectorFactoryDeleter

#include <memory>
#include <vector>
#include <mutex>
#include <string>

namespace uxdi {

/**
 * @brief Manager class for detector lifecycle and listener management
 *
 * DetectorManager provides a high-level API for managing multiple detector instances.
 * It handles detector creation, destruction, and maintains a registry of listeners
 * for each detector. All operations are thread-safe.
 */
class UXDI_API DetectorManager {
public:
    // Constructor/Destructor
    DetectorManager();
    ~DetectorManager();

    // Non-copyable, non-movable
    DetectorManager(const DetectorManager&) = delete;
    DetectorManager& operator=(const DetectorManager&) = delete;
    DetectorManager(DetectorManager&&) = delete;
    DetectorManager& operator=(DetectorManager&&) = delete;

    /**
     * @brief Create a detector instance from the specified adapter
     *
     * Creates a new detector using DetectorFactory with the given adapter ID
     * and configuration. The detector is assigned a unique ID for later reference.
     *
     * @param adapterId ID from DetectorFactory::LoadAdapter
     * @param config JSON configuration string for the detector
     * @return Detector ID for referencing this detector (0 on failure)
     */
    size_t CreateDetector(size_t adapterId, const std::string& config);

    /**
     * @brief Destroy a detector instance
     *
     * Removes the detector from the registry and releases all resources.
     * All listeners associated with this detector are automatically removed.
     *
     * @param detectorId ID returned from CreateDetector
     */
    void DestroyDetector(size_t detectorId);

    /**
     * @brief Get detector interface by ID
     *
     * Returns the raw pointer to the IDetector interface for direct access.
     * The detector remains owned by the manager.
     *
     * @param detectorId ID returned from CreateDetector
     * @return Pointer to IDetector interface (nullptr if not found)
     */
    IDetector* GetDetector(size_t detectorId);

    /**
     * @brief Add a listener for detector events
     *
     * Registers a listener to receive callbacks from the detector.
     * Multiple listeners can be registered per detector.
     * Duplicate listener registration is ignored.
     *
     * @param detectorId ID returned from CreateDetector
     * @param listener Pointer to listener implementation
     * @return true if listener was added, false if detector not found or duplicate
     */
    bool AddListener(size_t detectorId, IDetectorListener* listener);

    /**
     * @brief Remove a specific listener
     *
     * Unregisters a previously added listener from receiving detector events.
     *
     * @param detectorId ID returned from CreateDetector
     * @param listener Pointer to listener implementation
     * @return true if listener was removed, false if not found
     */
    bool RemoveListener(size_t detectorId, IDetectorListener* listener);

    /**
     * @brief Remove all listeners for a detector
     *
     * Clears all listeners associated with the specified detector.
     *
     * @param detectorId ID returned from CreateDetector
     * @return Number of listeners removed
     */
    size_t RemoveAllListeners(size_t detectorId);

    /**
     * @brief Get the current state of a detector
     *
     * Thread-safe accessor to the detector's current state.
     *
     * @param detectorId ID returned from CreateDetector
     * @return Current detector state (UNKNOWN if detector not found)
     */
    DetectorState GetState(size_t detectorId);

    /**
     * @brief Get detector information
     *
     * Returns the detector's information structure containing vendor,
     * model, serial number, and capabilities.
     *
     * @param detectorId ID returned from CreateDetector
     * @return DetectorInfo structure (empty if detector not found)
     */
    DetectorInfo GetInfo(size_t detectorId);

    /**
     * @brief Destroy all detector instances
     *
     * Convenience method to clean up all managed detectors.
     * Useful for application shutdown.
     */
    void DestroyAllDetectors();

    /**
     * @brief Get the number of managed detectors
     *
     * @return Current number of detector instances
     */
    size_t GetDetectorCount() const;

    /**
     * @brief Get all detector IDs
     *
     * Returns a list of all valid detector IDs.
     *
     * @return Vector of detector IDs
     */
    std::vector<size_t> GetDetectorIds() const;

    /**
     * @brief Check if a detector ID is valid
     *
     * @param detectorId ID to check
     * @return true if detector exists and is valid
     */
    bool IsValidDetector(size_t detectorId) const;

private:
    // Internal detector entry structure
    struct DetectorEntry {
        size_t id;                           // Unique detector ID
        size_t adapterId;                    // Adapter ID used for creation
        std::unique_ptr<IDetector, DetectorFactoryDeleter> detector; // Detector instance (owning)
        std::vector<IDetectorListener*> listeners; // Non-owning listener pointers

        DetectorEntry(size_t id_, size_t adapterId_, std::unique_ptr<IDetector, DetectorFactoryDeleter> detector_)
            : id(id_), adapterId(adapterId_), detector(std::move(detector_)) {}
    };

    // Helper to find detector entry by ID
    std::vector<DetectorEntry>::iterator FindDetectorEntry(size_t detectorId);
    std::vector<DetectorEntry>::const_iterator FindDetectorEntry(size_t detectorId) const;

    // Member variables
    std::vector<DetectorEntry> m_detectors;
    mutable std::mutex m_mutex;
    size_t m_nextDetectorId;
};

} // namespace uxdi
