#pragma once

#include <cstdint>
#include <cstdlib>

// For mock SDK, always use export when building
#ifdef ABYZ_MOCK_SDK_IMPL
#define ABYZ_API
#else
#ifdef ABYZ_SDK_EXPORTS
#define ABYZ_API __declspec(dllexport)
#else
#define ABYZ_API __declspec(dllimport)
#endif
#endif

//=============================================================================
// ABYZ SDK Types and Constants
//=============================================================================

/**
 * @brief Opaque handle to ABYZ detector
 */
typedef void* AbyzHandle;

/**
 * @brief ABYZ supported vendors
 */
enum AbyzVendor {
    ABYZ_VENDOR_RAYENCE = 0,
    ABYZ_VENDOR_SAMSUNG = 1,
    ABYZ_VENDOR_DRTECH = 2
};

/**
 * @brief ABYZ SDK error codes
 */
enum AbyzError {
    ABYZ_OK = 0,
    ABYZ_ERR_NOT_INITIALIZED = -1,
    ABYZ_ERR_ALREADY_INITIALIZED = -2,
    ABYZ_ERR_INVALID_PARAMETER = -3,
    ABYZ_ERR_TIMEOUT = -4,
    ABYZ_ERR_HARDWARE = -5,
    ABYZ_ERR_COMMUNICATION = -6,
    ABYZ_ERR_NOT_SUPPORTED = -7,
    ABYZ_ERR_STATE_ERROR = -8,
    ABYZ_ERR_OUT_OF_MEMORY = -9,
    ABYZ_ERR_UNKNOWN_VENDOR = -10
};

/**
 * @brief ABYZ detector state
 */
enum AbyzState {
    ABYZ_STATE_IDLE = 0,
    ABYZ_STATE_READY = 1,
    ABYZ_STATE_ACQUIRING = 2,
    ABYZ_STATE_ERROR = 3
};

/**
 * @brief ABYZ image structure (SDK-owned memory)
 *
 * IMPORTANT: The SDK owns the image buffer. The adapter MUST copy
 * the data immediately after receiving the callback.
 */
typedef struct AbyzImage {
    void* data;              ///< Image data pointer (SDK-owned, read-only)
    uint32_t width;          ///< Image width in pixels
    uint32_t height;         ///< Image height in pixels
    uint32_t bitDepth;       ///< Bit depth (typically 16)
    uint64_t frameNumber;    ///< Frame sequence number
    double timestamp;        ///< Unix timestamp in seconds
    uint32_t dataLength;     ///< Buffer size in bytes
    enum AbyzVendor vendor;  ///< Source vendor
} AbyzImage;

/**
 * @brief ABYZ acquisition parameters
 */
typedef struct AbyzAcqParams {
    uint32_t width;
    uint32_t height;
    uint32_t offsetX;
    uint32_t offsetY;
    float exposureTimeMs;
    float gain;
    uint32_t binning;
} AbyzAcqParams;

/**
 * @brief ABYZ detector information
 */
typedef struct AbyzDetectorInfo {
    enum AbyzVendor vendor;
    char vendorName[32];
    char model[32];
    char serialNumber[32];
    char firmwareVersion[16];
    uint32_t maxWidth;
    uint32_t maxHeight;
    uint32_t bitDepth;
} AbyzDetectorInfo;

//=============================================================================
// ABYZ SDK Callback Types
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
typedef void (*AbyzImageCallback)(const AbyzImage* image, void* userContext);

/**
 * @brief State change callback function type
 *
 * Called when the detector state changes.
 *
 * @param newState New detector state
 * @param userContext User-provided context pointer
 */
typedef void (*AbyzStateCallback)(AbyzState newState, void* userContext);

/**
 * @brief Error callback function type
 *
 * Called when an error occurs.
 *
 * @param error Error code
 * @param errorMessage Error message string
 * @param userContext User-provided context pointer
 */
typedef void (*AbyzErrorCallback)(AbyzError error, const char* errorMessage, void* userContext);

//=============================================================================
// ABYZ SDK API Functions (C linkage)
//=============================================================================

extern "C" {

/**
 * @brief Initialize the ABYZ SDK
 *
 * @return ABYZ_OK on success, error code otherwise
 */
ABYZ_API enum AbyzError Abyz_Initialize(void);

/**
 * @brief Shutdown the ABYZ SDK
 *
 * @return ABYZ_OK on success, error code otherwise
 */
ABYZ_API enum AbyzError Abyz_Shutdown(void);

/**
 * @brief Create a new detector handle
 *
 * @param config Configuration string (JSON format with "vendor" field)
 *               Example: {"vendor": "rayence"} or {"vendor": "samsung"}
 * @param outHandle Output pointer to receive the detector handle
 * @return ABYZ_OK on success, error code otherwise
 */
ABYZ_API enum AbyzError Abyz_CreateDetector(const char* config, AbyzHandle* outHandle);

/**
 * @brief Destroy a detector handle
 *
 * @param handle Detector handle to destroy
 * @return ABYZ_OK on success, error code otherwise
 */
ABYZ_API enum AbyzError Abyz_DestroyDetector(AbyzHandle handle);

/**
 * @brief Initialize the detector
 *
 * @param handle Detector handle
 * @return ABYZ_OK on success, error code otherwise
 */
ABYZ_API enum AbyzError Abyz_InitializeDetector(AbyzHandle handle);

/**
 * @brief Shutdown the detector
 *
 * @param handle Detector handle
 * @return ABYZ_OK on success, error code otherwise
 */
ABYZ_API enum AbyzError Abyz_ShutdownDetector(AbyzHandle handle);

/**
 * @brief Get detector information
 *
 * @param handle Detector handle
 * @param outInfo Output pointer to receive detector information
 * @return ABYZ_OK on success, error code otherwise
 */
ABYZ_API enum AbyzError Abyz_GetDetectorInfo(AbyzHandle handle, struct AbyzDetectorInfo* outInfo);

/**
 * @brief Get current detector state
 *
 * @param handle Detector handle
 * @param outState Output pointer to receive current state
 * @return ABYZ_OK on success, error code otherwise
 */
ABYZ_API enum AbyzError Abyz_GetState(AbyzHandle handle, enum AbyzState* outState);

/**
 * @brief Set acquisition parameters
 *
 * @param handle Detector handle
 * @param params Acquisition parameters to set
 * @return ABYZ_OK on success, error code otherwise
 */
ABYZ_API enum AbyzError Abyz_SetAcquisitionParams(AbyzHandle handle, const struct AbyzAcqParams* params);

/**
 * @brief Get acquisition parameters
 *
 * @param handle Detector handle
 * @param outParams Output pointer to receive current parameters
 * @return ABYZ_OK on success, error code otherwise
 */
ABYZ_API enum AbyzError Abyz_GetAcquisitionParams(AbyzHandle handle, struct AbyzAcqParams* outParams);

/**
 * @brief Register callbacks for event notifications
 *
 * @param handle Detector handle
 * @param imageCallback Callback for image notifications
 * @param stateCallback Callback for state change notifications
 * @param errorCallback Callback for error notifications
 * @param userContext User context pointer passed to callbacks
 * @return ABYZ_OK on success, error code otherwise
 */
ABYZ_API enum AbyzError Abyz_RegisterCallbacks(
    AbyzHandle handle,
    AbyzImageCallback imageCallback,
    AbyzStateCallback stateCallback,
    AbyzErrorCallback errorCallback,
    void* userContext
);

/**
 * @brief Start image acquisition
 *
 * After calling this function, the SDK will begin delivering images
 * through the registered image callback.
 *
 * @param handle Detector handle
 * @return ABYZ_OK on success, error code otherwise
 */
ABYZ_API enum AbyzError Abyz_StartAcquisition(AbyzHandle handle);

/**
 * @brief Stop image acquisition
 *
 * @param handle Detector handle
 * @return ABYZ_OK on success, error code otherwise
 */
ABYZ_API enum AbyzError Abyz_StopAcquisition(AbyzHandle handle);

/**
 * @brief Check if acquisition is active
 *
 * @param handle Detector handle
 * @param outAcquiring Output pointer to receive acquisition status
 * @return ABYZ_OK on success, error code otherwise
 */
ABYZ_API enum AbyzError Abyz_IsAcquiring(AbyzHandle handle, int* outAcquiring);

/**
 * @brief Convert error code to string
 *
 * @param error Error code
 * @return Error message string
 */
ABYZ_API const char* Abyz_ErrorToString(enum AbyzError error);

/**
 * @brief Convert state to string
 *
 * @param state State value
 * @return State string
 */
ABYZ_API const char* Abyz_StateToString(enum AbyzState state);

/**
 * @brief Convert vendor enum to string
 *
 * @param vendor Vendor enum value
 * @return Vendor name string
 */
ABYZ_API const char* Abyz_VendorToString(enum AbyzVendor vendor);

} // extern "C"
