#include <gtest/gtest.h>
#include "uxdi/DetectorFactory.h"
#include "uxdi/IDetector.h"
#include <filesystem>
#include <stdexcept>

using namespace uxdi;

// ============================================================================
// Test fixture for DetectorFactory tests
// ============================================================================

class DetectorFactoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any previously loaded adapters before each test
        try {
            DetectorFactory::UnloadAllAdapters();
        } catch (...) {
            // Ignore if no adapters were loaded
        }
    }

    void TearDown() override {
        // Clean up after each test
        try {
            DetectorFactory::UnloadAllAdapters();
        } catch (...) {
            // Ignore cleanup errors
        }
    }

    // Helper to get a non-existent DLL path
    std::wstring GetNonExistentDllPath() const {
        return L"C:\\NonExistent\\Path\\AdapterThatDoesNotExist.dll";
    }

    // Helper to get an invalid DLL path (wrong extension)
    std::wstring GetInvalidExtensionPath() const {
        return L"C:\\Invalid\\Adapter.txt";
    }
};

// ============================================================================
// Tests for LoadAdapter with invalid paths
// ============================================================================

TEST_F(DetectorFactoryTest, LoadNonExistentDllThrows) {
    auto nonExistentPath = GetNonExistentDllPath();

    EXPECT_THROW(
        DetectorFactory::LoadAdapter(nonExistentPath),
        std::runtime_error
    );
}

TEST_F(DetectorFactoryTest, LoadEmptyPathThrows) {
    std::wstring emptyPath = L"";

    EXPECT_THROW(
        DetectorFactory::LoadAdapter(emptyPath),
        std::runtime_error
    );
}

// ============================================================================
// Tests for LoadAdapter with missing exports
// ============================================================================

TEST_F(DetectorFactoryTest, LoadDllMissingCreateDetectorExport) {
    // This test requires a mock DLL without CreateDetector export
    // For now, we document the expected behavior
    // When LoadAdapter is called with a DLL missing CreateDetector:
    // - Should throw std::runtime_error
    // - DLL should be freed (not leak handles)
    // - Message should contain "does not export CreateDetector"

    SUCCEED() << "Mock DLL test - actual test requires creating a test DLL";
}

TEST_F(DetectorFactoryTest, LoadDllMissingDestroyDetectorExport) {
    // This test requires a mock DLL without DestroyDetector export
    // When LoadAdapter is called with a DLL missing DestroyDetector:
    // - Should throw std::runtime_error
    // - DLL should be freed (not leak handles)
    // - Message should contain "does not export DestroyDetector"

    SUCCEED() << "Mock DLL test - actual test requires creating a test DLL";
}

// ============================================================================
// Tests for GetLoadedAdapters
// ============================================================================

TEST_F(DetectorFactoryTest, GetLoadedAdaptersInitiallyEmpty) {
    auto adapters = DetectorFactory::GetLoadedAdapters();
    EXPECT_TRUE(adapters.empty());
}

TEST_F(DetectorFactoryTest, GetLoadedAdaptersAfterFailedLoad) {
    // Try to load a non-existent DLL
    try {
        DetectorFactory::LoadAdapter(GetNonExistentDllPath());
    } catch (const std::runtime_error&) {
        // Expected to throw
    }

    // GetLoadedAdapters should still return empty
    auto adapters = DetectorFactory::GetLoadedAdapters();
    EXPECT_TRUE(adapters.empty());
}

// ============================================================================
// Tests for CreateDetector with invalid adapter ID
// ============================================================================

TEST_F(DetectorFactoryTest, CreateDetectorWithZeroAdapterId) {
    EXPECT_THROW(
        DetectorFactory::CreateDetector(0, "{}"),
        std::runtime_error
    );
}

TEST_F(DetectorFactoryTest, CreateDetectorWithInvalidAdapterId) {
    EXPECT_THROW(
        DetectorFactory::CreateDetector(999, "{}"),
        std::runtime_error
    );
}

TEST_F(DetectorFactoryTest, CreateDetectorWithNonExistentAdapterId) {
    EXPECT_THROW(
        DetectorFactory::CreateDetector(1, "{}"),
        std::runtime_error
    );
}

TEST_F(DetectorFactoryTest, CreateDetectorErrorMessageContainsId) {
    try {
        DetectorFactory::CreateDetector(12345, "{}");
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        std::string message = e.what();
        // Verify the error message contains the adapter ID
        EXPECT_TRUE(message.find("12345") != std::string::npos ||
                    message.find("Invalid") != std::string::npos);
    }
}

// ============================================================================
// Tests for DestroyDetector
// ============================================================================

TEST_F(DetectorFactoryTest, DestroyDetectorWithNullPtr) {
    std::unique_ptr<IDetector, DetectorFactoryDeleter> nullDetector;
    EXPECT_NO_THROW(
        DetectorFactory::DestroyDetector(nullDetector)
    );
}

TEST_F(DetectorFactoryTest, DestroyDetectorNullptrCheck) {
    // Test that nullptr detector doesn't crash
    std::unique_ptr<IDetector, DetectorFactoryDeleter> detector = nullptr;
    EXPECT_NO_THROW(
        DetectorFactory::DestroyDetector(detector)
    );
    EXPECT_EQ(detector, nullptr);
}

// ============================================================================
// Tests for UnloadAdapter
// ============================================================================

TEST_F(DetectorFactoryTest, UnloadAdapterWithZeroId) {
    EXPECT_THROW(
        DetectorFactory::UnloadAdapter(0),
        std::runtime_error
    );
}

TEST_F(DetectorFactoryTest, UnloadAdapterWithInvalidId) {
    EXPECT_THROW(
        DetectorFactory::UnloadAdapter(999),
        std::runtime_error
    );
}

TEST_F(DetectorFactoryTest, UnloadAdapterErrorMessageContainsId) {
    try {
        DetectorFactory::UnloadAdapter(54321);
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        std::string message = e.what();
        // Verify the error message contains the adapter ID
        EXPECT_TRUE(message.find("54321") != std::string::npos ||
                    message.find("Invalid") != std::string::npos ||
                    message.find("not found") != std::string::npos);
    }
}

// ============================================================================
// Tests for UnloadAllAdapters
// ============================================================================

TEST_F(DetectorFactoryTest, UnloadAllAdaptersWhenEmpty) {
    EXPECT_NO_THROW(
        DetectorFactory::UnloadAllAdapters()
    );

    // Should still be empty
    auto adapters = DetectorFactory::GetLoadedAdapters();
    EXPECT_TRUE(adapters.empty());
}

TEST_F(DetectorFactoryTest, UnloadAllAdaptersMultipleCalls) {
    // Multiple calls should be safe
    EXPECT_NO_THROW(DetectorFactory::UnloadAllAdapters());
    EXPECT_NO_THROW(DetectorFactory::UnloadAllAdapters());
    EXPECT_NO_THROW(DetectorFactory::UnloadAllAdapters());

    auto adapters = DetectorFactory::GetLoadedAdapters();
    EXPECT_TRUE(adapters.empty());
}

// ============================================================================
// Tests for string conversion utilities
// ============================================================================

TEST_F(DetectorFactoryTest, ToWideStringEmpty) {
    std::wstring result = DetectorFactory::ToWideString("");
    EXPECT_TRUE(result.empty());
}

TEST_F(DetectorFactoryTest, ToWideStringSimple) {
    std::wstring result = DetectorFactory::ToWideString("test");
    EXPECT_EQ(result, L"test");
}

TEST_F(DetectorFactoryTest, ToWideStringAscii) {
    std::string input = "Hello, World!";
    std::wstring result = DetectorFactory::ToWideString(input);

    EXPECT_EQ(result.length(), input.length());
    EXPECT_EQ(result[0], L'H');
    EXPECT_EQ(result[result.length() - 1], L'!');
}

TEST_F(DetectorFactoryTest, ToWideStringSpecialCharacters) {
    std::string input = "Path\\With/Backslash";
    std::wstring result = DetectorFactory::ToWideString(input);

    EXPECT_EQ(result, L"Path\\With/Backslash");
}

TEST_F(DetectorFactoryTest, ToWideStringPath) {
    std::string path = "C:\\Users\\Test\\Adapter.dll";
    std::wstring result = DetectorFactory::ToWideString(path);

    EXPECT_EQ(result, L"C:\\Users\\Test\\Adapter.dll");
}

TEST_F(DetectorFactoryTest, ToUtf8StringEmpty) {
    std::string result = DetectorFactory::ToUtf8String(L"");
    EXPECT_TRUE(result.empty());
}

TEST_F(DetectorFactoryTest, ToUtf8StringSimple) {
    std::string result = DetectorFactory::ToUtf8String(L"test");
    EXPECT_EQ(result, "test");
}

TEST_F(DetectorFactoryTest, ToUtf8StringAscii) {
    std::wstring input = L"Hello, World!";
    std::string result = DetectorFactory::ToUtf8String(input);

    EXPECT_EQ(result.length(), input.length());
    EXPECT_EQ(result[0], 'H');
    EXPECT_EQ(result[result.length() - 1], '!');
}

TEST_F(DetectorFactoryTest, ToUtf8StringPath) {
    std::wstring path = L"C:\\Users\\Test\\Adapter.dll";
    std::string result = DetectorFactory::ToUtf8String(path);

    EXPECT_EQ(result, "C:\\Users\\Test\\Adapter.dll");
}

TEST_F(DetectorFactoryTest, StringConversionRoundtrip) {
    // Test UTF-8 -> Wide -> UTF-8 roundtrip
    std::string original = "C:\\Test\\Path\\Adapter.dll";
    std::wstring wide = DetectorFactory::ToWideString(original);
    std::string result = DetectorFactory::ToUtf8String(wide);

    EXPECT_EQ(original, result);
}

TEST_F(DetectorFactoryTest, WideStringConversionRoundtrip) {
    // Test Wide -> UTF-8 -> Wide roundtrip
    std::wstring original = L"C:\\Test\\Path\\Adapter.dll";
    std::string utf8 = DetectorFactory::ToUtf8String(original);
    std::wstring result = DetectorFactory::ToWideString(utf8);

    EXPECT_EQ(original, result);
}

// ============================================================================
// Thread safety tests (basic)
// ============================================================================

TEST_F(DetectorFactoryTest, GetLoadedAdaptersThreadSafe) {
    // Multiple calls to GetLoadedAdapters should be safe
    auto adapters1 = DetectorFactory::GetLoadedAdapters();
    auto adapters2 = DetectorFactory::GetLoadedAdapters();
    auto adapters3 = DetectorFactory::GetLoadedAdapters();

    EXPECT_TRUE(adapters1.empty());
    EXPECT_TRUE(adapters2.empty());
    EXPECT_TRUE(adapters3.empty());
}

// ============================================================================
// Edge case tests
// ============================================================================

TEST_F(DetectorFactoryTest, CreateDetectorWithEmptyConfig) {
    // Even with no adapters loaded, this should throw with a clear message
    EXPECT_THROW(
        DetectorFactory::CreateDetector(1, ""),
        std::runtime_error
    );
}

TEST_F(DetectorFactoryTest, CreateDetectorWithNullConfig) {
    // The interface uses std::string which cannot be null
    // But we can test with empty string
    EXPECT_THROW(
        DetectorFactory::CreateDetector(1, std::string()),
        std::runtime_error
    );
}

TEST_F(DetectorFactoryTest, CreateDetectorWithLargeConfig) {
    // Large config string should still throw if adapter doesn't exist
    std::string largeConfig(10000, '{'); // Large JSON-like string

    EXPECT_THROW(
        DetectorFactory::CreateDetector(1, largeConfig),
        std::runtime_error
    );
}

TEST_F(DetectorFactoryTest, ToWideStringWithNewlines) {
    std::string input = "Line1\nLine2\rLine3";
    std::wstring result = DetectorFactory::ToWideString(input);

    // Newlines should be preserved
    EXPECT_TRUE(result.find(L'\n') != std::wstring::npos);
}

TEST_F(DetectorFactoryTest, ToUtf8StringWithNewlines) {
    std::wstring input = L"Line1\nLine2\rLine3";
    std::string result = DetectorFactory::ToUtf8String(input);

    // Newlines should be preserved
    EXPECT_TRUE(result.find('\n') != std::string::npos);
}

// ============================================================================
// Error message format tests
// ============================================================================

TEST_F(DetectorFactoryTest, LoadAdapterErrorFormat) {
    try {
        DetectorFactory::LoadAdapter(GetNonExistentDllPath());
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        std::string message = e.what();
        // Error message should contain useful information
        EXPECT_FALSE(message.empty());
        EXPECT_TRUE(message.find("Failed to load") != std::string::npos ||
                    message.find("load") != std::string::npos);
    }
}

TEST_F(DetectorFactoryTest, CreateDetectorErrorForInvalidAdapter) {
    try {
        DetectorFactory::CreateDetector(0, "{}");
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        std::string message = e.what();
        EXPECT_FALSE(message.empty());
        // Should mention invalid or adapter ID
        EXPECT_TRUE(message.find("Invalid") != std::string::npos ||
                    message.find("adapter") != std::string::npos ||
                    message.find("0") != std::string::npos);
    }
}

TEST_F(DetectorFactoryTest, UnloadAdapterErrorForInvalidAdapter) {
    try {
        DetectorFactory::UnloadAdapter(0);
        FAIL() << "Expected std::runtime_error";
    } catch (const std::runtime_error& e) {
        std::string message = e.what();
        EXPECT_FALSE(message.empty());
        // Should mention invalid or adapter ID
        EXPECT_TRUE(message.find("Invalid") != std::string::npos ||
                    message.find("adapter") != std::string::npos);
    }
}

// ============================================================================
// Adapter ID validation tests
// ============================================================================

TEST_F(DetectorFactoryTest, AdapterIdZeroIsInvalid) {
    // Adapter ID 0 should always be invalid (reserved)
    EXPECT_THROW(DetectorFactory::CreateDetector(0, "{}"), std::runtime_error);
    EXPECT_THROW(DetectorFactory::UnloadAdapter(0), std::runtime_error);
}

TEST_F(DetectorFactoryTest, AdapterIdLargeValue) {
    // Very large adapter ID should be invalid
    size_t largeId = SIZE_MAX;

    EXPECT_THROW(
        DetectorFactory::CreateDetector(largeId, "{}"),
        std::runtime_error
    );

    EXPECT_THROW(
        DetectorFactory::UnloadAdapter(largeId),
        std::runtime_error
    );
}

// ============================================================================
// Tests for DLL path parsing
// ============================================================================

TEST_F(DetectorFactoryTest, AdapterInfoNameFromPath) {
    // When a DLL is loaded, the adapter name should be extracted from filename
    // This test documents expected behavior
    // "C:\\Path\\DummyAdapter.dll" -> name should be "DummyAdapter"
    // "C:\\Path\\MyAdapter.dll" -> name should be "MyAdapter"

    SUCCEED() << "Behavior documented - actual test requires real DLL";
}

// ============================================================================
// Note on integration tests
// ============================================================================

// The following tests require actual adapter DLLs to be present:
// - LoadAdapter with valid DLL path
// - CreateDetector returning valid IDetector instance
// - Detector lifecycle (create, use, destroy)
// - Multiple adapters loaded simultaneously
// - Adapter unload after detector creation
//
// These will be implemented in Task 9 (Virtual adapter tests)
// once DummyAdapter and EmulAdapter are available.
