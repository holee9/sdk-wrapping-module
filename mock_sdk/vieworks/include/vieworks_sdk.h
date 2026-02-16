#pragma once

#include <cstdint>
#include <cstdlib>

// For mock SDK, always use export when building
#ifdef VIEWORKS_MOCK_SDK_IMPL
#define VIEWORKS_API
#else
#ifdef VIEWORKS_SDK_EXPORTS
#define VIEWORKS_API __declspec(dllexport)
#else
#define VIEWORKS_API __declspec(dllimport)
#endif
#endif

//=============================================================================
// Vieworks SDK Types and Constants
//=============================================================================

/**
 * @brief Opaque handle to Vieworks detector
 */
typedef void* VieworksHandle;

/**
 * @brief Vieworks SDK status codes
 */
enum VieworksStatus {
    VIEWORKS_OK = 0,
    VIEWORKS_ERR_NOT_INITIALIZED = -1,
    VIEWORKS_ERR_ALREADY_INITIALIZED = -2,
    VIEWORKS_ERR_INVALID_PARAMETER = -3,
    VIEWORKS_ERR_TIMEOUT = -4,
    VIEWORKS_ERR_HARDWARE = -5,
    VIEWORKS_ERR_COMMUNICATION = -6,
    VIEWORKS_ERR_NOT_SUPPORTED = -7,
    VIEWORKS_ERR_STATE_ERROR = -8,
    VIEWORKS_ERR_OUT_OF_MEMORY = -9
};

/**
 * @brief Vieworks detector state
 */
enum VieworksState {
    VIEWORKS_STATE_STANDBY = 0,
    VIEWORKS_STATE_READY = 1,
    VIEWORKS_STATE_EXPOSING = 2,
    VIEWORKS_STATE_READING = 3,
    VIEWORKS_STATE_ERROR = 4
};

/**
 * @brief Vieworks frame structure
 *
 * The buffer remains valid until the next ReadFrame call or StopAcquisition.
 * This allows for zero-copy optimization - the adapter can use the buffer
 * directly without copying, but must ensure it doesn't call ReadFrame again
 * before processing the current frame.
 */
typedef struct VieworksFrame {
    void* data;              ///< Image data pointer (stable until next ReadFrame)
    uint32_t width;          ///< Image width in pixels
    uint32_t height;         ///< Image height in pixels
    uint32_t bitDepth;       ///< Bit depth (typically 16)
    uint64_t frameNumber;    ///< Frame sequence number
    double timestamp;        ///< Unix timestamp in seconds
    uint32_t dataLength;     ///< Buffer size in bytes
} VieworksFrame;

/**
 * @brief Vieworks acquisition parameters
 */
typedef struct VieworksAcqParams {
    uint32_t width;
    uint32_t height;
    uint32_t offsetX;
    uint32_t offsetY;
    float exposureTimeMs;
    float gain;
    uint32_t binning;
} VieworksAcqParams;

/**
 * @brief Vieworks detector information
 */
typedef struct VieworksDetectorInfo {
    char vendor[32];
    char model[32];
    char serialNumber[32];
    char firmwareVersion[16];
    uint32_t maxWidth;
    uint32_t maxHeight;
    uint32_t bitDepth;
} VieworksDetectorInfo;

//=============================================================================
// Vieworks SDK API Functions (Polling-based, C linkage)
//=============================================================================

extern "C" {

/**
 * @brief Initialize the Vieworks SDK
 *
 * @return VIEWORKS_OK on success, error code otherwise
 */
VIEWORKS_API VieworksStatus Vieworks_Initialize();

/**
 * @brief Shutdown the Vieworks SDK
 *
 * @return VIEWORKS_OK on success, error code otherwise
 */
VIEWORKS_API VieworksStatus Vieworks_Shutdown();

/**
 * @brief Create a new detector handle
 *
 * @param config Configuration string (can be null or empty for default)
 * @param outHandle Output pointer to receive the detector handle
 * @return VIEWORKS_OK on success, error code otherwise
 */
VIEWORKS_API VieworksStatus Vieworks_CreateDetector(const char* config, VieworksHandle* outHandle);

/**
 * @brief Destroy a detector handle
 *
 * @param handle Detector handle to destroy
 * @return VIEWORKS_OK on success, error code otherwise
 */
VIEWORKS_API VieworksStatus Vieworks_DestroyDetector(VieworksHandle handle);

/**
 * @brief Initialize the detector
 *
 * @param handle Detector handle
 * @return VIEWORKS_OK on success, error code otherwise
 */
VIEWORKS_API VieworksStatus Vieworks_InitializeDetector(VieworksHandle handle);

/**
 * @brief Shutdown the detector
 *
 * @param handle Detector handle
 * @return VIEWORKS_OK on success, error code otherwise
 */
VIEWORKS_API VieworksStatus Vieworks_ShutdownDetector(VieworksHandle handle);

/**
 * @brief Get detector information
 *
 * @param handle Detector handle
 * @param outInfo Output pointer to receive detector information
 * @return VIEWORKS_OK on success, error code otherwise
 */
VIEWORKS_API VieworksStatus Vieworks_GetDetectorInfo(VieworksHandle handle, VieworksDetectorInfo* outInfo);

/**
 * @brief Get current detector state
 *
 * @param handle Detector handle
 * @param outState Output pointer to receive current state
 * @return VIEWORKS_OK on success, error code otherwise
 */
VIEWORKS_API VieworksStatus Vieworks_GetState(VieworksHandle handle, VieworksState* outState);

/**
 * @brief Set acquisition parameters
 *
 * @param handle Detector handle
 * @param params Acquisition parameters to set
 * @return VIEWORKS_OK on success, error code otherwise
 */
VIEWORKS_API VieworksStatus Vieworks_SetAcquisitionParams(VieworksHandle handle, const VieworksAcqParams* params);

/**
 * @brief Get acquisition parameters
 *
 * @param handle Detector handle
 * @param outParams Output pointer to receive current parameters
 * @return VIEWORKS_OK on success, error code otherwise
 */
VIEWORKS_API VieworksStatus Vieworks_GetAcquisitionParams(VieworksHandle handle, VieworksAcqParams* outParams);

/**
 * @brief Start image acquisition
 *
 * After calling this function, the detector will begin capturing frames.
 * Use GetFrameReady and ReadFrame to retrieve frames.
 *
 * @param handle Detector handle
 * @return VIEWORKS_OK on success, error code otherwise
 */
VIEWORKS_API VieworksStatus Vieworks_StartAcquisition(VieworksHandle handle);

/**
 * @brief Stop image acquisition
 *
 * @param handle Detector handle
 * @return VIEWORKS_OK on success, error code otherwise
 */
VIEWORKS_API VieworksStatus Vieworks_StopAcquisition(VieworksHandle handle);

/**
 * @brief Check if a frame is ready to read
 *
 * @param handle Detector handle
 * @param outReady Output pointer to receive frame ready status
 * @return VIEWORKS_OK on success, error code otherwise
 */
VIEWORKS_API VieworksStatus Vieworks_GetFrameReady(VieworksHandle handle, int* outReady);

/**
 * @brief Read the next available frame
 *
 * The frame buffer remains valid until the next ReadFrame call.
 * This allows for zero-copy optimization when processing frames quickly.
 *
 * @param handle Detector handle
 * @param outFrame Output pointer to receive frame data
 * @return VIEWORKS_OK on success, error code otherwise
 */
VIEWORKS_API VieworksStatus Vieworks_ReadFrame(VieworksHandle handle, VieworksFrame* outFrame);

/**
 * @brief Check if acquisition is active
 *
 * @param handle Detector handle
 * @param outAcquiring Output pointer to receive acquisition status
 * @return VIEWORKS_OK on success, error code otherwise
 */
VIEWORKS_API VieworksStatus Vieworks_IsAcquiring(VieworksHandle handle, int* outAcquiring);

/**
 * @brief Convert status code to string
 *
 * @param status Status code
 * @return Status message string
 */
VIEWORKS_API const char* Vieworks_StatusToString(VieworksStatus status);

/**
 * @brief Convert state to string
 *
 * @param state State value
 * @return State string
 */
VIEWORKS_API const char* Vieworks_StateToString(VieworksState state);

} // extern "C"
