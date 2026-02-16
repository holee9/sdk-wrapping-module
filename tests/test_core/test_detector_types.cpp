#include <gtest/gtest.h>
#include "uxdi/Types.h"
#include <string>
#include <cstring>

using namespace uxdi;

// ============================================================================
// Tests for DetectorState enum
// ============================================================================

TEST(DetectorTypes, DetectorStateValues) {
    // Test that enum values have correct underlying values
    EXPECT_EQ(static_cast<int>(DetectorState::UNKNOWN), 0);
    EXPECT_EQ(static_cast<int>(DetectorState::IDLE), 1);
    EXPECT_EQ(static_cast<int>(DetectorState::INITIALIZING), 2);
    EXPECT_EQ(static_cast<int>(DetectorState::READY), 3);
    EXPECT_EQ(static_cast<int>(DetectorState::ACQUIRING), 4);
    EXPECT_EQ(static_cast<int>(DetectorState::STOPPING), 5);
    EXPECT_EQ(static_cast<int>(DetectorState::ERROR), 6);
}

TEST(DetectorTypes, DetectorStateComparable) {
    // Test that state values are comparable
    EXPECT_LT(DetectorState::UNKNOWN, DetectorState::IDLE);
    EXPECT_LT(DetectorState::IDLE, DetectorState::READY);
    EXPECT_LT(DetectorState::READY, DetectorState::ACQUIRING);
    EXPECT_GT(DetectorState::ERROR, DetectorState::UNKNOWN);
}

// ============================================================================
// Tests for ErrorCode enum
// ============================================================================

TEST(DetectorTypes, ErrorCodeValues) {
    // Test error code enum values
    EXPECT_EQ(static_cast<int>(ErrorCode::SUCCESS), 0);
    EXPECT_EQ(static_cast<int>(ErrorCode::UNKNOWN_ERROR), 1);
    EXPECT_EQ(static_cast<int>(ErrorCode::NOT_INITIALIZED), 2);
    EXPECT_EQ(static_cast<int>(ErrorCode::ALREADY_INITIALIZED), 3);
    EXPECT_EQ(static_cast<int>(ErrorCode::INVALID_PARAMETER), 4);
    EXPECT_EQ(static_cast<int>(ErrorCode::TIMEOUT), 5);
    EXPECT_EQ(static_cast<int>(ErrorCode::HARDWARE_ERROR), 6);
    EXPECT_EQ(static_cast<int>(ErrorCode::COMMUNICATION_ERROR), 7);
    EXPECT_EQ(static_cast<int>(ErrorCode::NOT_SUPPORTED), 8);
    EXPECT_EQ(static_cast<int>(ErrorCode::STATE_ERROR), 9);
    EXPECT_EQ(static_cast<int>(ErrorCode::OUT_OF_MEMORY), 10);
}

TEST(DetectorTypes, SuccessIsZero) {
    // SUCCESS should always be zero for idiomatic error checking
    ErrorCode code = ErrorCode::SUCCESS;
    EXPECT_EQ(static_cast<int>(code), 0);
}

// ============================================================================
// Tests for DetectorInfo struct
// ============================================================================

TEST(DetectorTypes, DetectorInfoDefaultConstruction) {
    // Test default construction yields empty strings
    DetectorInfo info;
    EXPECT_TRUE(info.vendor.empty());
    EXPECT_TRUE(info.model.empty());
    EXPECT_TRUE(info.serialNumber.empty());
    EXPECT_TRUE(info.firmwareVersion.empty());
    EXPECT_EQ(info.maxWidth, 0);
    EXPECT_EQ(info.maxHeight, 0);
    EXPECT_EQ(info.bitDepth, 0);
}

TEST(DetectorTypes, DetectorInfoConstruction) {
    // Test construction with values
    DetectorInfo info{
        "TestVendor",
        "TestModel",
        "SN12345",
        "1.0.0",
        2048,
        2048,
        16
    };

    EXPECT_EQ(info.vendor, "TestVendor");
    EXPECT_EQ(info.model, "TestModel");
    EXPECT_EQ(info.serialNumber, "SN12345");
    EXPECT_EQ(info.firmwareVersion, "1.0.0");
    EXPECT_EQ(info.maxWidth, 2048);
    EXPECT_EQ(info.maxHeight, 2048);
    EXPECT_EQ(info.bitDepth, 16);
}

TEST(DetectorTypes, DetectorInfoCopyConstruction) {
    // Test copy construction
    DetectorInfo info1{
        "Vendor1",
        "Model1",
        "SN001",
        "2.0",
        1024,
        768,
        14
    };

    DetectorInfo info2 = info1;

    EXPECT_EQ(info2.vendor, "Vendor1");
    EXPECT_EQ(info2.model, "Model1");
    EXPECT_EQ(info2.serialNumber, "SN001");
    EXPECT_EQ(info2.maxWidth, 1024);
}

TEST(DetectorTypes, DetectorInfoAssignment) {
    // Test copy assignment
    DetectorInfo info1{"V1", "M1", "SN1", "1.0", 100, 100, 8};
    DetectorInfo info2;

    info2 = info1;

    EXPECT_EQ(info2.vendor, "V1");
    EXPECT_EQ(info2.model, "M1");
    EXPECT_EQ(info2.bitDepth, 8);
}

// ============================================================================
// Tests for AcquisitionParams struct
// ============================================================================

TEST(DetectorTypes, AcquisitionParamsDefaultConstruction) {
    // Test default construction
    AcquisitionParams params;
    EXPECT_EQ(params.width, 0);
    EXPECT_EQ(params.height, 0);
    EXPECT_EQ(params.offsetX, 0);
    EXPECT_EQ(params.offsetY, 0);
    EXPECT_FLOAT_EQ(params.exposureTimeMs, 0.0f);
    EXPECT_FLOAT_EQ(params.gain, 0.0f);
    EXPECT_EQ(params.binning, 0);
}

TEST(DetectorTypes, AcquisitionParamsConstruction) {
    // Test construction with values
    AcquisitionParams params{
        1024,
        768,
        0,
        0,
        100.0f,
        1.5f,
        1
    };

    EXPECT_EQ(params.width, 1024);
    EXPECT_EQ(params.height, 768);
    EXPECT_EQ(params.offsetX, 0);
    EXPECT_EQ(params.offsetY, 0);
    EXPECT_FLOAT_EQ(params.exposureTimeMs, 100.0f);
    EXPECT_FLOAT_EQ(params.gain, 1.5f);
    EXPECT_EQ(params.binning, 1);
}

TEST(DetectorTypes, AcquisitionParamsBinningValues) {
    // Test common binning values
    AcquisitionParams params;
    params.binning = 1;
    EXPECT_EQ(params.binning, 1);

    params.binning = 2;
    EXPECT_EQ(params.binning, 2);

    params.binning = 4;
    EXPECT_EQ(params.binning, 4);
}

// ============================================================================
// Tests for ImageData struct
// ============================================================================

TEST(DetectorTypes, ImageDataDefaultConstruction) {
    // Test default construction
    ImageData image;
    EXPECT_EQ(image.width, 0);
    EXPECT_EQ(image.height, 0);
    EXPECT_EQ(image.bitDepth, 0);
    EXPECT_EQ(image.frameNumber, 0);
    EXPECT_EQ(image.timestamp, 0.0);
    EXPECT_EQ(image.data, nullptr);
    EXPECT_EQ(image.dataLength, 0);
}

TEST(DetectorTypes, ImageDataConstruction) {
    // Test construction with values
    const size_t dataSize = 1024 * 1024;
    auto buffer = std::make_shared<uint8_t[]>(dataSize);
    std::memset(buffer.get(), 0xFF, dataSize);

    ImageData image{
        1024,
        1024,
        16,
        42,
        1234567890.0,
        buffer,
        dataSize
    };

    EXPECT_EQ(image.width, 1024);
    EXPECT_EQ(image.height, 1024);
    EXPECT_EQ(image.bitDepth, 16);
    EXPECT_EQ(image.frameNumber, 42);
    EXPECT_DOUBLE_EQ(image.timestamp, 1234567890.0);
    EXPECT_EQ(image.dataLength, dataSize);
    EXPECT_NE(image.data, nullptr);
}

TEST(DetectorTypes, ImageDataZeroCopy) {
    // Test that shared_ptr enables zero-copy semantics
    const size_t dataSize = 512 * 512;
    auto buffer = std::make_shared<uint8_t[]>(dataSize);

    ImageData image1{512, 512, 8, 1, 0.0, buffer, dataSize};
    ImageData image2 = image1; // Copy - shares the same buffer

    // Both should point to the same underlying buffer
    EXPECT_EQ(image1.data.get(), image2.data.get());

    // Verify data is shared
    image1.data.get()[0] = 42;
    EXPECT_EQ(image2.data.get()[0], 42);
}

TEST(DetectorTypes, ImageDataFrameNumber) {
    // Test frame number tracking
    ImageData image1;
    image1.frameNumber = 0;
    EXPECT_EQ(image1.frameNumber, 0);

    ImageData image2;
    image2.frameNumber = 99999;
    EXPECT_EQ(image2.frameNumber, 99999);
}

TEST(DetectorTypes, ImageDataTimestamp) {
    // Test timestamp values
    ImageData image;
    image.timestamp = 1640000000.0; // Some arbitrary Unix timestamp
    EXPECT_DOUBLE_EQ(image.timestamp, 1640000000.0);
}

// ============================================================================
// Tests for ErrorInfo struct
// ============================================================================

TEST(DetectorTypes, ErrorInfoDefaultConstruction) {
    // Test default construction - default ErrorCode is SUCCESS (0)
    ErrorInfo error;
    EXPECT_EQ(error.code, ErrorCode::SUCCESS);
    EXPECT_TRUE(error.message.empty());
    EXPECT_TRUE(error.details.empty());
}

TEST(DetectorTypes, ErrorInfoConstruction) {
    // Test construction with values
    ErrorInfo error{
        ErrorCode::INVALID_PARAMETER,
        "Invalid parameter value",
        "Parameter 'exposure' must be positive"
    };

    EXPECT_EQ(error.code, ErrorCode::INVALID_PARAMETER);
    EXPECT_EQ(error.message, "Invalid parameter value");
    EXPECT_EQ(error.details, "Parameter 'exposure' must be positive");
}

TEST(DetectorTypes, ErrorInfoWithSuccessCode) {
    // Test error info with SUCCESS code (edge case)
    ErrorInfo error{
        ErrorCode::SUCCESS,
        "No error",
        ""
    };

    EXPECT_EQ(error.code, ErrorCode::SUCCESS);
    EXPECT_EQ(error.message, "No error");
    EXPECT_TRUE(error.details.empty());
}

TEST(DetectorTypes, ErrorInfoCopyable) {
    // Test that ErrorInfo can be copied
    ErrorInfo error1{
        ErrorCode::TIMEOUT,
        "Operation timed out",
        "Timeout after 5000ms"
    };

    ErrorInfo error2 = error1;

    EXPECT_EQ(error2.code, ErrorCode::TIMEOUT);
    EXPECT_EQ(error2.message, "Operation timed out");
    EXPECT_EQ(error2.details, "Timeout after 5000ms");
}

// ============================================================================
// Edge case and validation tests
// ============================================================================

TEST(DetectorTypes, DetectorInfoMaxDimensions) {
    // Test large dimension values
    DetectorInfo info{
        "Vendor",
        "Model",
        "SN",
        "1.0",
        UINT32_MAX,
        UINT32_MAX,
        32
    };

    EXPECT_EQ(info.maxWidth, UINT32_MAX);
    EXPECT_EQ(info.maxHeight, UINT32_MAX);
    EXPECT_EQ(info.bitDepth, 32);
}

TEST(DetectorTypes, AcquisitionParamsFloatPrecision) {
    // Test float precision for exposure and gain
    AcquisitionParams params;
    params.exposureTimeMs = 0.001f; // 1 microsecond
    params.gain = 0.01f;

    EXPECT_NEAR(params.exposureTimeMs, 0.001f, 0.0001f);
    EXPECT_NEAR(params.gain, 0.01f, 0.001f);
}

TEST(DetectorTypes, ImageDataLargeBuffer) {
    // Test with large image buffer (e.g., 4K x 4K x 2 bytes)
    const size_t dataSize = 4096ULL * 4096ULL * 2ULL;
    auto buffer = std::make_shared<uint8_t[]>(dataSize);

    ImageData image{4096, 4096, 16, 0, 0.0, buffer, dataSize};

    EXPECT_EQ(image.width, 4096);
    EXPECT_EQ(image.height, 4096);
    EXPECT_EQ(image.dataLength, dataSize);
}

TEST(DetectorTypes, EmptyStringHandling) {
    // Test that empty strings are handled correctly
    DetectorInfo info;
    info.vendor = "";
    info.model = "";
    info.serialNumber = "";

    EXPECT_TRUE(info.vendor.empty());
    EXPECT_TRUE(info.model.empty());
    EXPECT_TRUE(info.serialNumber.empty());
}

TEST(DetectorTypes, StringsWithSpaces) {
    // Test strings with spaces
    DetectorInfo info{
        "Test Vendor Inc.",
        "Pro Model 2024",
        "SN 123 456",
        "Firmware 1.0 Beta"
    };

    EXPECT_EQ(info.vendor, "Test Vendor Inc.");
    EXPECT_EQ(info.model, "Pro Model 2024");
    EXPECT_EQ(info.serialNumber, "SN 123 456");
    EXPECT_EQ(info.firmwareVersion, "Firmware 1.0 Beta");
}
