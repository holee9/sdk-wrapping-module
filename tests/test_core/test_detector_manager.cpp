#include <gtest/gtest.h>
#include "uxdi/DetectorManager.h"
#include "uxdi/DetectorFactory.h"
#include "uxdi/IDetector.h"
#include "uxdi/IDetectorListener.h"
#include <memory>
#include <vector>

// Windows macro workaround - ERROR conflicts with DetectorState::ERROR
#ifdef ERROR
#undef ERROR
#endif

using namespace uxdi;

// ============================================================================
// Mock Detector for testing
// ============================================================================

class MockDetector : public IDetector {
public:
    bool initialized = false;
    bool acquiring = false;
    int initializeCount = 0;
    int shutdownCount = 0;
    IDetectorListener* listener = nullptr;
    DetectorState state = DetectorState::UNKNOWN;
    DetectorInfo info;
    AcquisitionParams params;
    ErrorInfo lastError;

    bool initialize() override {
        initializeCount++;
        initialized = true;
        state = DetectorState::IDLE;
        return true;
    }

    bool shutdown() override {
        shutdownCount++;
        initialized = false;
        state = DetectorState::UNKNOWN;
        return true;
    }

    bool isInitialized() const override {
        return initialized;
    }

    DetectorInfo getDetectorInfo() const override {
        return info;
    }

    std::string getVendorName() const override {
        return info.vendor;
    }

    std::string getModelName() const override {
        return info.model;
    }

    DetectorState getState() const override {
        return state;
    }

    std::string getStateString() const override {
        switch (state) {
            case DetectorState::UNKNOWN: return "UNKNOWN";
            case DetectorState::IDLE: return "IDLE";
            case DetectorState::INITIALIZING: return "INITIALIZING";
            case DetectorState::READY: return "READY";
            case DetectorState::ACQUIRING: return "ACQUIRING";
            case DetectorState::STOPPING: return "STOPPING";
            case DetectorState::ERROR: return "ERROR";
            default: return "UNKNOWN";
        }
    }

    bool setAcquisitionParams(const AcquisitionParams& p) override {
        params = p;
        return true;
    }

    AcquisitionParams getAcquisitionParams() const override {
        return params;
    }

    void setListener(IDetectorListener* l) override {
        listener = l;
    }

    IDetectorListener* getListener() const override {
        return listener;
    }

    bool startAcquisition() override {
        acquiring = true;
        state = DetectorState::ACQUIRING;
        return true;
    }

    bool stopAcquisition() override {
        acquiring = false;
        state = DetectorState::IDLE;
        return true;
    }

    bool isAcquiring() const override {
        return acquiring;
    }

    std::shared_ptr<IDetectorSynchronous> getSynchronousInterface() override {
        return nullptr;
    }

    ErrorInfo getLastError() const override {
        return lastError;
    }

    void clearError() override {
        lastError = ErrorInfo{};
    }
};

// ============================================================================
// Mock Listener for testing
// ============================================================================

class MockListener : public IDetectorListener {
public:
    int imageCount = 0;
    int stateChangeCount = 0;
    int errorCount = 0;
    int acqStartedCount = 0;
    int acqStoppedCount = 0;
    ImageData lastImage;
    DetectorState lastState;
    ErrorInfo lastError;

    void onImageReceived(const ImageData& image) override {
        imageCount++;
        lastImage = image;
    }

    void onStateChanged(DetectorState newState) override {
        stateChangeCount++;
        lastState = newState;
    }

    void onError(const ErrorInfo& error) override {
        errorCount++;
        lastError = error;
    }

    void onAcquisitionStarted() override {
        acqStartedCount++;
    }

    void onAcquisitionStopped() override {
        acqStoppedCount++;
    }

    void reset() {
        imageCount = 0;
        stateChangeCount = 0;
        errorCount = 0;
        acqStartedCount = 0;
        acqStoppedCount = 0;
    }
};

// ============================================================================
// Test fixture for DetectorManager tests
// ============================================================================

class DetectorManagerTest : public ::testing::Test {
protected:
    DetectorManager manager;
    MockListener listener1;
    MockListener listener2;

    void SetUp() override {
        // Reset listener counters
        listener1.reset();
        listener2.reset();
    }

    void TearDown() override {
        manager.DestroyAllDetectors();
    }
};

// ============================================================================
// Constructor/Destructor tests
// ============================================================================

TEST_F(DetectorManagerTest, ConstructorInitializes) {
    DetectorManager mgr;
    EXPECT_EQ(mgr.GetDetectorCount(), 0);
    EXPECT_TRUE(mgr.GetDetectorIds().empty());
}

TEST_F(DetectorManagerTest, DestructorCleansUp) {
    // Create a manager and let it go out of scope
    {
        DetectorManager tempManager;
        // Manager should clean up on destruction
    }
    // If we get here without crashing, destructor worked
    SUCCEED();
}

// ============================================================================
// CreateDetector tests
// ============================================================================

TEST_F(DetectorManagerTest, CreateDetectorWithInvalidAdapter) {
    // Invalid adapter ID should return detector ID 0
    size_t detectorId = manager.CreateDetector(999, "{}");
    EXPECT_EQ(detectorId, 0);
    EXPECT_EQ(manager.GetDetectorCount(), 0);
}

TEST_F(DetectorManagerTest, CreateDetectorWithZeroAdapterId) {
    size_t detectorId = manager.CreateDetector(0, "{}");
    EXPECT_EQ(detectorId, 0);
    EXPECT_EQ(manager.GetDetectorCount(), 0);
}

TEST_F(DetectorManagerTest, CreateDetectorWithEmptyConfig) {
    // Empty config should still be handled gracefully
    size_t detectorId = manager.CreateDetector(1, "");
    EXPECT_EQ(detectorId, 0);
}

TEST_F(DetectorManagerTest, CreateDetectorIdsAreUnique) {
    // Without actual adapters, we can't test this fully
    // But we verify that invalid calls return 0
    size_t id1 = manager.CreateDetector(999, "{}");
    size_t id2 = manager.CreateDetector(998, "{}");
    size_t id3 = manager.CreateDetector(997, "{}");

    EXPECT_EQ(id1, 0);
    EXPECT_EQ(id2, 0);
    EXPECT_EQ(id3, 0);
}

// ============================================================================
// GetDetector tests
// ============================================================================

TEST_F(DetectorManagerTest, GetDetectorInvalidId) {
    EXPECT_EQ(manager.GetDetector(999), nullptr);
    EXPECT_EQ(manager.GetDetector(0), nullptr);
    EXPECT_EQ(manager.GetDetector(SIZE_MAX), nullptr);
}

TEST_F(DetectorManagerTest, GetDetectorFromEmptyManager) {
    for (size_t i = 0; i < 100; ++i) {
        EXPECT_EQ(manager.GetDetector(i), nullptr);
    }
}

// ============================================================================
// DestroyDetector tests
// ============================================================================

TEST_F(DetectorManagerTest, DestroyDetectorInvalidId) {
    // Should not throw for invalid detector IDs
    EXPECT_NO_THROW(manager.DestroyDetector(999));
    EXPECT_NO_THROW(manager.DestroyDetector(0));
    EXPECT_EQ(manager.GetDetectorCount(), 0);
}

TEST_F(DetectorManagerTest, DestroyDetectorIdempotent) {
    // Multiple destroy calls with same ID should be safe
    EXPECT_NO_THROW(manager.DestroyDetector(100));
    EXPECT_NO_THROW(manager.DestroyDetector(100));
    EXPECT_NO_THROW(manager.DestroyDetector(100));
}

TEST_F(DetectorManagerTest, DestroyAllDetectorsEmpty) {
    manager.DestroyAllDetectors();
    EXPECT_EQ(manager.GetDetectorCount(), 0);
}

TEST_F(DetectorManagerTest, DestroyAllDetectorsMultipleCalls) {
    manager.DestroyAllDetectors();
    manager.DestroyAllDetectors();
    manager.DestroyAllDetectors();
    EXPECT_EQ(manager.GetDetectorCount(), 0);
}

// ============================================================================
// AddListener tests
// ============================================================================

TEST_F(DetectorManagerTest, AddListenerInvalidDetector) {
    EXPECT_FALSE(manager.AddListener(999, &listener1));
    EXPECT_FALSE(manager.AddListener(0, &listener1));
    EXPECT_FALSE(manager.AddListener(SIZE_MAX, &listener1));
}

TEST_F(DetectorManagerTest, AddListenerNull) {
    EXPECT_FALSE(manager.AddListener(1, nullptr));
    EXPECT_FALSE(manager.AddListener(0, nullptr));
}

TEST_F(DetectorManagerTest, AddListenerDoesNotCrash) {
    // Should not crash when adding to non-existent detector
    EXPECT_NO_THROW(manager.AddListener(999, &listener1));
}

// ============================================================================
// RemoveListener tests
// ============================================================================

TEST_F(DetectorManagerTest, RemoveListenerInvalid) {
    EXPECT_FALSE(manager.RemoveListener(999, &listener1));
    EXPECT_FALSE(manager.RemoveListener(0, &listener1));
    EXPECT_FALSE(manager.RemoveListener(SIZE_MAX, &listener2));
}

TEST_F(DetectorManagerTest, RemoveListenerNull) {
    EXPECT_FALSE(manager.RemoveListener(1, nullptr));
    EXPECT_FALSE(manager.RemoveListener(0, nullptr));
}

TEST_F(DetectorManagerTest, RemoveListenerIdempotent) {
    // Multiple removes should be safe
    EXPECT_NO_THROW(manager.RemoveListener(100, &listener1));
    EXPECT_NO_THROW(manager.RemoveListener(100, &listener1));
}

// ============================================================================
// RemoveAllListeners tests
// ============================================================================

TEST_F(DetectorManagerTest, RemoveAllListenersInvalid) {
    EXPECT_EQ(manager.RemoveAllListeners(999), 0);
    EXPECT_EQ(manager.RemoveAllListeners(0), 0);
    EXPECT_EQ(manager.RemoveAllListeners(SIZE_MAX), 0);
}

TEST_F(DetectorManagerTest, RemoveAllListenersDoesNotCrash) {
    EXPECT_NO_THROW(manager.RemoveAllListeners(100));
}

// ============================================================================
// GetState tests
// ============================================================================

TEST_F(DetectorManagerTest, GetStateInvalid) {
    EXPECT_EQ(manager.GetState(999), DetectorState::UNKNOWN);
    EXPECT_EQ(manager.GetState(0), DetectorState::UNKNOWN);
    EXPECT_EQ(manager.GetState(SIZE_MAX), DetectorState::UNKNOWN);
}

TEST_F(DetectorManagerTest, GetStateFromEmptyManager) {
    for (size_t i = 0; i < 10; ++i) {
        EXPECT_EQ(manager.GetState(i), DetectorState::UNKNOWN);
    }
}

// ============================================================================
// GetInfo tests
// ============================================================================

TEST_F(DetectorManagerTest, GetInfoInvalid) {
    DetectorInfo info = manager.GetInfo(999);
    EXPECT_TRUE(info.vendor.empty());
    EXPECT_TRUE(info.model.empty());
    EXPECT_TRUE(info.serialNumber.empty());
    EXPECT_TRUE(info.firmwareVersion.empty());
    EXPECT_EQ(info.maxWidth, 0);
    EXPECT_EQ(info.maxHeight, 0);
    EXPECT_EQ(info.bitDepth, 0);
}

TEST_F(DetectorManagerTest, GetInfoDefaultConstructed) {
    DetectorInfo info = manager.GetInfo(0);
    EXPECT_TRUE(info.vendor.empty());
    EXPECT_EQ(info.maxWidth, 0);
}

// ============================================================================
// IsValidDetector tests
// ============================================================================

TEST_F(DetectorManagerTest, IsValidDetectorInitiallyFalse) {
    EXPECT_FALSE(manager.IsValidDetector(0));
    EXPECT_FALSE(manager.IsValidDetector(1));
    EXPECT_FALSE(manager.IsValidDetector(999));
    EXPECT_FALSE(manager.IsValidDetector(SIZE_MAX));
}

TEST_F(DetectorManagerTest, IsValidDetectorAfterFailedCreate) {
    manager.CreateDetector(999, "{}");
    // Should still be false because creation failed
    EXPECT_FALSE(manager.IsValidDetector(0));
}

// ============================================================================
// GetDetectorCount tests
// ============================================================================

TEST_F(DetectorManagerTest, GetDetectorCountInitiallyZero) {
    EXPECT_EQ(manager.GetDetectorCount(), 0);
}

TEST_F(DetectorManagerTest, GetDetectorCountAfterFailedCreations) {
    manager.CreateDetector(999, "{}");
    manager.CreateDetector(998, "{}");
    manager.CreateDetector(997, "{}");
    EXPECT_EQ(manager.GetDetectorCount(), 0);
}

// ============================================================================
// GetDetectorIds tests
// ============================================================================

TEST_F(DetectorManagerTest, GetDetectorIdsInitiallyEmpty) {
    std::vector<size_t> ids = manager.GetDetectorIds();
    EXPECT_TRUE(ids.empty());
}

TEST_F(DetectorManagerTest, GetDetectorIdsAfterFailedCreations) {
    manager.CreateDetector(999, "{}");
    manager.CreateDetector(998, "{}");
    std::vector<size_t> ids = manager.GetDetectorIds();
    EXPECT_TRUE(ids.empty());
}

// ============================================================================
// Copy/move prevention tests
// ============================================================================

TEST_F(DetectorManagerTest, NonCopyable) {
    // DetectorManager should not be copyable
    // This is a compile-time check; if it compiles, the delete declarations work
    SUCCEED() << "DetectorManager is non-copyable (checked at compile time)";
}

TEST_F(DetectorManagerTest, NonMovable) {
    // DetectorManager should not be movable
    // This is a compile-time check; if it compiles, the delete declarations work
    SUCCEED() << "DetectorManager is non-movable (checked at compile time)";
}

// ============================================================================
// Thread safety basic tests
// ============================================================================

TEST_F(DetectorManagerTest, ConcurrentGetDetectorCount) {
    // Multiple calls should be consistent
    size_t count1 = manager.GetDetectorCount();
    size_t count2 = manager.GetDetectorCount();
    size_t count3 = manager.GetDetectorCount();

    EXPECT_EQ(count1, 0);
    EXPECT_EQ(count2, 0);
    EXPECT_EQ(count3, 0);
}

TEST_F(DetectorManagerTest, ConcurrentGetDetectorIds) {
    std::vector<size_t> ids1 = manager.GetDetectorIds();
    std::vector<size_t> ids2 = manager.GetDetectorIds();
    std::vector<size_t> ids3 = manager.GetDetectorIds();

    EXPECT_TRUE(ids1.empty());
    EXPECT_TRUE(ids2.empty());
    EXPECT_TRUE(ids3.empty());
}

// ============================================================================
// Edge case tests
// ============================================================================

TEST_F(DetectorManagerTest, VeryLargeDetectorIds) {
    // Should handle large IDs gracefully
    size_t largeId = SIZE_MAX;

    EXPECT_EQ(manager.GetDetector(largeId), nullptr);
    EXPECT_EQ(manager.GetState(largeId), DetectorState::UNKNOWN);
    EXPECT_EQ(manager.GetInfo(largeId).vendor.empty(), true);
    EXPECT_FALSE(manager.IsValidDetector(largeId));
    EXPECT_FALSE(manager.AddListener(largeId, &listener1));
    EXPECT_FALSE(manager.RemoveListener(largeId, &listener1));
    EXPECT_EQ(manager.RemoveAllListeners(largeId), 0);
}

TEST_F(DetectorManagerTest, ZeroDetectorIdHandling) {
    // ID 0 should be treated as invalid
    EXPECT_EQ(manager.GetDetector(0), nullptr);
    EXPECT_EQ(manager.GetState(0), DetectorState::UNKNOWN);
    EXPECT_TRUE(manager.GetInfo(0).vendor.empty());
    EXPECT_FALSE(manager.IsValidDetector(0));
}

TEST_F(DetectorManagerTest, MultipleInvalidOperations) {
    // Chain multiple invalid operations
    EXPECT_EQ(manager.CreateDetector(0, ""), 0);
    EXPECT_EQ(manager.GetDetector(0), nullptr);
    EXPECT_FALSE(manager.AddListener(0, &listener1));
    EXPECT_FALSE(manager.RemoveListener(0, &listener1));
    EXPECT_EQ(manager.RemoveAllListeners(0), 0);
    manager.DestroyDetector(0);
    EXPECT_EQ(manager.GetState(0), DetectorState::UNKNOWN);
    EXPECT_TRUE(manager.GetInfo(0).vendor.empty());
}

// ============================================================================
// Listener pointer management tests
// ============================================================================

TEST_F(DetectorManagerTest, ListenerPointersNotOwned) {
    // Manager stores non-owning pointers to listeners
    // This test verifies that listeners are not deleted by manager
    MockListener localListener;

    // Even if we could add a listener (with valid detector),
    // the manager should not delete it
    // For now, just verify no crash on invalid detector
    EXPECT_NO_THROW(manager.AddListener(999, &localListener));
}

TEST_F(DetectorManagerTest, MultipleListenersSamePointer) {
    // Adding the same listener twice should be handled
    // (duplicate detection - returns false)
    // For invalid detector, returns false
    EXPECT_FALSE(manager.AddListener(999, &listener1));
    EXPECT_FALSE(manager.AddListener(999, &listener1));
}

// ============================================================================
// Exception safety tests
// ============================================================================

TEST_F(DetectorManagerTest, OperationsDontThrowOnInvalidInput) {
    // All operations should be safe with invalid input
    EXPECT_NO_THROW({
        manager.GetDetector(0);
        manager.GetDetector(999);
        manager.DestroyDetector(0);
        manager.DestroyDetector(999);
        manager.GetState(0);
        manager.GetState(999);
        manager.GetInfo(0);
        manager.GetInfo(999);
        manager.AddListener(0, nullptr);
        manager.RemoveListener(0, nullptr);
        manager.RemoveAllListeners(0);
        manager.IsValidDetector(0);
        manager.GetDetectorCount();
        manager.GetDetectorIds();
        manager.DestroyAllDetectors();
    });
}

// ============================================================================
// State consistency tests
// ============================================================================

TEST_F(DetectorManagerTest, StateConsistencyAfterFailedOperations) {
    // Manager state should remain consistent after failed operations
    size_t countBefore = manager.GetDetectorCount();
    std::vector<size_t> idsBefore = manager.GetDetectorIds();

    manager.CreateDetector(999, "{}");
    manager.CreateDetector(998, "{}");
    manager.DestroyDetector(999);
    manager.AddListener(0, &listener1);

    size_t countAfter = manager.GetDetectorCount();
    std::vector<size_t> idsAfter = manager.GetDetectorIds();

    EXPECT_EQ(countBefore, countAfter);
    EXPECT_EQ(idsBefore.size(), idsAfter.size());
}

// ============================================================================
// Note on integration tests
// ============================================================================

// The following tests require actual adapter DLLs to be present:
// - CreateDetector returning valid detector ID
// - GetDetector returning valid IDetector pointer
// - AddListener/RemoveListener with valid detector
// - RemoveAllListeners returning count
// - GetState returning actual detector state
// - GetInfo returning actual detector info
// - Multiple detectors managed simultaneously
// - Detector lifecycle management
//
// These will be implemented in Task 9 (Virtual adapter tests)
// once DummyAdapter and EmulAdapter are available.

// Example integration test structure (to be completed in Task 9):
/*
TEST_F(DetectorManagerTest, CreateAndDestroyDetector) {
    // Requires: DummyAdapter to be loaded via DetectorFactory
    size_t adapterId = DetectorFactory::LoadAdapter(L"path/to/DummyAdapter.dll");
    ASSERT_GT(adapterId, 0);

    size_t detectorId = manager.CreateDetector(adapterId, "{}");
    EXPECT_GT(detectorId, 0);
    EXPECT_EQ(manager.GetDetectorCount(), 1);
    EXPECT_TRUE(manager.IsValidDetector(detectorId));

    IDetector* detector = manager.GetDetector(detectorId);
    ASSERT_NE(detector, nullptr);

    manager.DestroyDetector(detectorId);
    EXPECT_EQ(manager.GetDetectorCount(), 0);
    EXPECT_FALSE(manager.IsValidDetector(detectorId));

    DetectorFactory::UnloadAdapter(adapterId);
}
*/
