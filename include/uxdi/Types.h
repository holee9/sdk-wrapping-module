#pragma once

#include "uxdi_export.h"
#include <cstdint>
#include <string>
#include <memory>

namespace uxdi {

// Forward declarations
class IDetector;
class IDetectorListener;
class IDetectorSynchronous;

// Detector state enumeration
enum class DetectorState {
    UNKNOWN,
    IDLE,
    INITIALIZING,
    READY,
    ACQUIRING,
    STOPPING,
    ERROR
};

// Detector information structure
struct DetectorInfo {
    std::string vendor{};
    std::string model{};
    std::string serialNumber{};
    std::string firmwareVersion{};
    uint32_t maxWidth{};
    uint32_t maxHeight{};
    uint32_t bitDepth{};
};

// Acquisition parameters
struct AcquisitionParams {
    uint32_t width{};
    uint32_t height{};
    uint32_t offsetX{};
    uint32_t offsetY{};
    float exposureTimeMs{};  // Exposure time in milliseconds
    float gain{};           // Detector gain factor
    uint32_t binning{};     // Binning factor (1, 2, 4, etc.)
};

// Image data structure (zero-copy via shared_ptr)
struct ImageData {
    uint32_t width{};
    uint32_t height{};
    uint32_t bitDepth{};
    uint64_t frameNumber{};
    double timestamp{};  // Unix timestamp in seconds
    std::shared_ptr<uint8_t[]> data{};  // Zero-copy image buffer
    size_t dataLength{};                // Buffer size in bytes
};

// Error codes
enum class ErrorCode {
    SUCCESS = 0,
    UNKNOWN_ERROR,
    NOT_INITIALIZED,
    ALREADY_INITIALIZED,
    INVALID_PARAMETER,
    TIMEOUT,
    HARDWARE_ERROR,
    COMMUNICATION_ERROR,
    NOT_SUPPORTED,
    STATE_ERROR,
    OUT_OF_MEMORY
};

// Error information structure
struct ErrorInfo {
    ErrorCode code{};
    std::string message{};
    std::string details{};  // Additional error details
};

} // namespace uxdi
