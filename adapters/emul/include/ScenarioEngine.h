#pragma once

#include "uxdi/Types.h"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <mutex>
#include <chrono>
#include <random>

namespace uxdi {
namespace adapters {
namespace emul {

/**
 * @brief Action types for scenario execution
 */
enum class ActionType {
    Wait,           ///< Pause execution for specified duration
    SetState,       ///< Change detector state
    Acquire,        ///< Generate frames
    InjectError,    ///< Simulate errors
    SetParameter,   ///< Modify detector parameters
    Calibration     ///< Simulate calibration sequence
};

/**
 * @brief Convert action type to string
 */
inline const char* actionTypeToString(ActionType type) {
    switch (type) {
        case ActionType::Wait: return "wait";
        case ActionType::SetState: return "set_state";
        case ActionType::Acquire: return "acquire";
        case ActionType::InjectError: return "inject_error";
        case ActionType::SetParameter: return "set_parameter";
        case ActionType::Calibration: return "calibration";
        default: return "unknown";
    }
}

/**
 * @brief Convert string to action type
 */
inline std::optional<ActionType> stringToActionType(const std::string& str) {
    if (str == "wait") return ActionType::Wait;
    if (str == "set_state") return ActionType::SetState;
    if (str == "acquire") return ActionType::Acquire;
    if (str == "inject_error") return ActionType::InjectError;
    if (str == "set_parameter") return ActionType::SetParameter;
    if (str == "calibration") return ActionType::Calibration;
    return std::nullopt;
}

/**
 * @brief Convert string to detector state
 */
inline std::optional<DetectorState> stringToDetectorState(const std::string& str) {
    if (str == "unknown") return DetectorState::UNKNOWN;
    if (str == "idle") return DetectorState::IDLE;
    if (str == "initializing") return DetectorState::INITIALIZING;
    if (str == "ready") return DetectorState::READY;
    if (str == "acquiring") return DetectorState::ACQUIRING;
    if (str == "stopping") return DetectorState::STOPPING;
    if (str == "error") return DetectorState::ERROR;
    return std::nullopt;
}

/**
 * @brief Convert string to error code
 */
inline std::optional<ErrorCode> stringToErrorCode(const std::string& str) {
    if (str == "timeout") return ErrorCode::TIMEOUT;
    if (str == "hardware_error") return ErrorCode::HARDWARE_ERROR;
    if (str == "communication_error") return ErrorCode::COMMUNICATION_ERROR;
    if (str == "invalid_parameter") return ErrorCode::INVALID_PARAMETER;
    if (str == "state_error") return ErrorCode::STATE_ERROR;
    if (str == "not_supported") return ErrorCode::NOT_SUPPORTED;
    if (str == "out_of_memory") return ErrorCode::OUT_OF_MEMORY;
    if (str == "not_initialized") return ErrorCode::NOT_INITIALIZED;
    if (str == "unknown_error") return ErrorCode::UNKNOWN_ERROR;
    return std::nullopt;
}

/**
 * @brief Scenario action structure
 */
struct ScenarioAction {
    ActionType type;
    int duration_ms = 0;
    std::string state;
    int count = 0;
    int interval_ms = 0;
    std::string error;
    double probability = 0.0;
    std::string parameter;
    std::string value;
};

/**
 * @brief Scenario definition
 */
struct Scenario {
    std::string name;
    std::string description;
    std::vector<ScenarioAction> actions;
};

/**
 * @brief Execution context for scenario
 */
struct ExecutionContext {
    size_t current_action = 0;
    size_t frames_generated = 0;
    DetectorState current_state = DetectorState::IDLE;
    std::unordered_map<std::string, std::string> parameters;
    std::chrono::steady_clock::time_point last_action_time;
    bool waiting = false;
    std::chrono::steady_clock::time_point wait_start;
    int wait_duration_ms = 0;
};

/**
 * @brief Frame data for generated frames
 */
struct FrameData {
    uint32_t width;
    uint32_t height;
    uint32_t bitDepth;
    uint64_t frameNumber;
    double timestamp;
    std::shared_ptr<uint8_t[]> data;
    size_t dataLength;
};

/**
 * @brief ScenarioEngine - Scripted test scenario execution system
 *
 * Provides DSL-based test scenario execution for EmulAdapter.
 * Supports configurable test patterns, error injection, and state management.
 */
class ScenarioEngine {
public:
    ScenarioEngine();
    ~ScenarioEngine() = default;

    // Non-copyable, non-movable
    ScenarioEngine(const ScenarioEngine&) = delete;
    ScenarioEngine& operator=(const ScenarioEngine&) = delete;
    ScenarioEngine(ScenarioEngine&&) = delete;
    ScenarioEngine& operator=(ScenarioEngine&&) = delete;

    /**
     * @brief Load scenario from JSON string
     * @param json_scenario JSON string containing scenario definition
     * @return true if loading succeeded
     */
    bool LoadScenario(const std::string& json_scenario);

    /**
     * @brief Load scenario from file
     * @param file_path Path to scenario JSON file
     * @return true if loading succeeded
     */
    bool LoadScenarioFromFile(const std::string& file_path);

    /**
     * @brief Start scenario execution
     */
    void Start();

    /**
     * @brief Stop scenario execution
     */
    void Stop();

    /**
     * @brief Get next frame based on scenario
     * @return FrameData if frame should be generated, nullopt otherwise
     */
    std::optional<FrameData> GetNextFrame();

    /**
     * @brief Get current detector state
     * @return Current detector state
     */
    DetectorState GetCurrentState() const;

    /**
     * @brief Inject error based on scenario
     * @return ErrorCode if error should be injected, nullopt otherwise
     */
    std::optional<ErrorCode> GetNextError();

    /**
     * @brief Check if scenario is complete
     * @return true if all actions have been executed
     */
    bool IsComplete() const;

    /**
     * @brief Get scenario information
     * @return Current scenario definition
     */
    const Scenario& GetScenario() const;

    /**
     * @brief Reset execution to beginning
     */
    void Reset();

    /**
     * @brief Set detector info for frame generation
     * @param width Frame width
     * @param height Frame height
     * @param bitDepth Frame bit depth
     */
    void SetFrameConfig(uint32_t width, uint32_t height, uint32_t bitDepth);

    /**
     * @brief Get parameter value
     * @param name Parameter name
     * @return Parameter value or empty string if not found
     */
    std::string GetParameter(const std::string& name) const;

    /**
     * @brief Set parameter value
     * @param name Parameter name
     * @param value Parameter value
     */
    void SetParameter(const std::string& name, const std::string& value);

private:
    Scenario m_scenario;
    ExecutionContext m_context;
    mutable std::mutex m_mutex;
    bool m_running = false;

    // Frame configuration
    uint32_t m_frame_width = 1024;
    uint32_t m_frame_height = 1024;
    uint32_t m_frame_bit_depth = 16;

    // Random number generation for error injection
    mutable std::mt19937 m_rng;
    mutable std::uniform_real_distribution<double> m_dist;

    // Helper methods
    std::optional<ScenarioAction> GetCurrentAction() const;
    bool ExecuteAction(const ScenarioAction& action);
    FrameData GenerateFrame();
    void ProcessWaiting();
    bool ShouldInjectError(double probability) const;

    // JSON parsing helpers
    bool ParseScenario(const std::string& json);
    std::string Trim(const std::string& str) const;
    std::optional<std::string> ExtractString(const std::string& json, const std::string& key);
    std::optional<int> ExtractInt(const std::string& json, const std::string& key);
    std::optional<double> ExtractDouble(const std::string& json, const std::string& key);
    std::vector<std::string> ExtractArray(const std::string& json, const std::string& key);
    std::string UnescapeString(const std::string& str) const;
};

} // namespace emul
} // namespace adapters
} // namespace uxdi
