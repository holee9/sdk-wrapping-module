#pragma once

#include <cstdint>
#include <cstdlib>

// For mock SDK, always use export when building
#ifdef VAREX_MOCK_SDK_IMPL
#define VAREX_API
#else
#ifdef VAREX_SDK_EXPORTS
#define VAREX_API __declspec(dllexport)
#else
#define VAREX_API __declspec(dllimport)
#endif
#endif

//=============================================================================
// Varex SDK Types and Constants
//=============================================================================

/**
 * @brief Opaque handle to Varex detector
 */
typedef void* VarexHandle;

/**
 * @brief Varex SDK error codes
 */
enum VarexError {
    VAREX_OK = 0,
    VAREX_ERR_NOT_INITIALIZED = -1,
    VAREX_ERR_ALREADY_INITIALIZED = -2,
    VAREX_ERR_INVALID_PARAMETER = -3,
    VAREX_ERR_TIMEOUT = -4,
    VAREX_ERR_HARDWARE = -5,
    VAREX_ERR_COMMUNICATION = -6,
    VAREX_ERR_NOT_SUPPORTED = -7,
    VAREX_ERR_STATE_ERROR = -8,
    VAREX_ERR_OUT_OF_MEMORY = -9
};

/**
 * @brief Varex detector state
 */
enum VarexState {
    VAREX_STATE_IDLE = 0,
    VAREX_STATE_READY = 1,
    VAREX_STATE_ACQUIRING = 2,
    VAREX_STATE_ERROR = 3
};

/**
 * @brief Varex image structure (SDK-owned memory)
 *
 * IMPORTANT: The SDK owns the image buffer. The adapter MUST copy
 * the data immediately after receiving the callback.
 */
typedef struct VarexImage {
    void* data;              ///< Image data pointer (SDK-owned, read-only)
    uint32_t width;          ///< Image width in pixels
    uint32_t height;         ///< Image height in pixels
    uint32_t bitDepth;       ///< Bit depth (typically 16)
    uint64_t frameNumber;    ///< Frame sequence number
    double timestamp;        ///< Unix timestamp in seconds
    uint32_t dataLength;     ///< Buffer size in bytes
} VarexImage;

/**
 * @brief Varex acquisition parameters
 */
typedef struct VarexAcqParams {
    uint32_t width;
    uint32_t height;
    uint32_t offsetX;
    uint32_t offsetY;
    float exposureTimeMs;
    float gain;
    uint32_t binning;
} VarexAcqParams;

/**
 * @brief Varex detector information
 */
typedef struct VarexDetectorInfo {
    char vendor[32];
    char model[32];
    char serialNumber[32];
    char firmwareVersion[16];
    uint32_t maxWidth;
    uint32_t maxHeight;
    uint32_t bitDepth;
} VarexDetectorInfo;

//=============================================================================
// Varex SDK Callback Types
//=============================================================================

/**
 * @brief Image callback function type
 *
 * Called by the SDK when a new frame is available during acquisition.
 * The image data is SDK-owned and MUST be copied immediately.
 *
 * @param image Pointer to SDK-owned image structure
 * @param userContext User-provided context pointer
 */
typedef void (*VarexImageCallback)(const VarexImage* image, void* userContext);

/**
 * @brief State change callback function type
 *
 * Called when the detector state changes.
 *
 * @param newState New detector state
 * @param userContext User-provided context pointer
 */
typedef void (*VarexStateCallback)(VarexState newState, void* userContext);

/**
 * @brief Error callback function type
 *
 * Called when an error occurs.
 *
 * @param error Error code
 * @param errorMessage Error message string
 * @param userContext User-provided context pointer
 */
typedef void (*VarexErrorCallback)(VarexError error, const char* errorMessage, void* userContext);

//=============================================================================
// Varex SDK API Functions (C linkage)
//=============================================================================

extern "C" {

/**
 * @brief Initialize the Varex SDK
 *
 * @return VAREX_OK on success, error code otherwise
 */
VAREX_API VarexError Varex_Initialize();

/**
 * @brief Shutdown the Varex SDK
 *
 * @return VAREX_OK on success, error code otherwise
 */
VAREX_API VarexError Varex_Shutdown();

/**
 * @brief Create a new detector handle
 *
 * @param config Configuration string (can be null or empty for default)
 * @param outHandle Output pointer to receive the detector handle
 * @return VAREX_OK on success, error code otherwise
 */
VAREX_API VarexError Varex_CreateDetector(const char* config, VarexHandle* outHandle);

/**
 * @brief Destroy a detector handle
 *
 * @param handle Detector handle to destroy
 * @return VAREX_OK on success, error code otherwise
 */
VAREX_API VarexError Varex_DestroyDetector(VarexHandle handle);

/**
 * @brief Initialize the detector
 *
 * @param handle Detector handle
 * @return VAREX_OK on success, error code otherwise
 */
VAREX_API VarexError Varex_InitializeDetector(VarexHandle handle);

/**
 * @brief Shutdown the detector
 *
 * @param handle Detector handle
 * @return VAREX_OK on success, error code otherwise
 */
VAREX_API VarexError Varex_ShutdownDetector(VarexHandle handle);

/**
 * @brief Get detector information
 *
 * @param handle Detector handle
 * @param outInfo Output pointer to receive detector information
 * @return VAREX_OK on success, error code otherwise
 */
VAREX_API VarexError Varex_GetDetectorInfo(VarexHandle handle, VarexDetectorInfo* outInfo);

/**
 * @brief Get current detector state
 *
 * @param handle Detector handle
 * @param outState Output pointer to receive current state
 * @return VAREX_OK on success, error code otherwise
 */
VAREX_API VarexError Varex_GetState(VarexHandle handle, VarexState* outState);

/**
 * @brief Set acquisition parameters
 *
 * @param handle Detector handle
 * @param params Acquisition parameters to set
 * @return VAREX_OK on success, error code otherwise
 */
VAREX_API VarexError Varex_SetAcquisitionParams(VarexHandle handle, const VarexAcqParams* params);

/**
 * @brief Get acquisition parameters
 *
 * @param handle Detector handle
 * @param outParams Output pointer to receive current parameters
 * @return VAREX_OK on success, error code otherwise
 */
VAREX_API VarexError Varex_GetAcquisitionParams(VarexHandle handle, VarexAcqParams* outParams);

/**
 * @brief Register callbacks for event notifications
 *
 * @param handle Detector handle
 * @param imageCallback Callback for image notifications
 * @param stateCallback Callback for state change notifications
 * @param errorCallback Callback for error notifications
 * @param userContext User context pointer passed to callbacks
 * @return VAREX_OK on success, error code otherwise
 */
VAREX_API VarexError Varex_RegisterCallbacks(
    VarexHandle handle,
    VarexImageCallback imageCallback,
    VarexStateCallback stateCallback,
    VarexErrorCallback errorCallback,
    void* userContext
);

/**
 * @brief Start image acquisition
 *
 * After calling this function, the SDK will begin delivering images
 * through the registered image callback.
 *
 * @param handle Detector handle
 * @return VAREX_OK on success, error code otherwise
 */
VAREX_API VarexError Varex_StartAcquisition(VarexHandle handle);

/**
 * @brief Stop image acquisition
 *
 * @param handle Detector handle
 * @return VAREX_OK on success, error code otherwise
 */
VAREX_API VarexError Varex_StopAcquisition(VarexHandle handle);

/**
 * @brief Check if acquisition is active
 *
 * @param handle Detector handle
 * @param outAcquiring Output pointer to receive acquisition status
 * @return VAREX_OK on success, error code otherwise
 */
VAREX_API VarexError Varex_IsAcquiring(VarexHandle handle, int* outAcquiring);

/**
 * @brief Convert error code to string
 *
 * @param error Error code
 * @return Error message string
 */
VAREX_API const char* Varex_ErrorToString(VarexError error);

/**
 * @brief Convert state to string
 *
 * @param state State value
 * @return State string
 */
VAREX_API const char* Varex_StateToString(VarexState state);

} // extern "C"
