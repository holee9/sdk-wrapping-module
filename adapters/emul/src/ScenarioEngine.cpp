#include "ScenarioEngine.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace uxdi {
namespace adapters {
namespace emul {

// ============================================================================
// ScenarioEngine Implementation
// ============================================================================

ScenarioEngine::ScenarioEngine()
    : m_rng(std::random_device{}())
    , m_dist(0.0, 1.0)
{
    m_context.current_state = DetectorState::IDLE;
    m_context.last_action_time = std::chrono::steady_clock::now();
}

bool ScenarioEngine::LoadScenario(const std::string& json_scenario) {
    std::lock_guard<std::mutex> lock(m_mutex);
    return ParseScenario(json_scenario);
}

bool ScenarioEngine::LoadScenarioFromFile(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ifstream file(file_path);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return ParseScenario(buffer.str());
}

void ScenarioEngine::Start() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_running = true;
    m_context.current_action = 0;
    m_context.frames_generated = 0;
    m_context.current_state = DetectorState::IDLE;
    m_context.waiting = false;
    m_context.last_action_time = std::chrono::steady_clock::now();
}

void ScenarioEngine::Stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_running = false;
    m_context.waiting = false;
}

std::optional<FrameData> ScenarioEngine::GetNextFrame() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running) {
        return std::nullopt;
    }

    ProcessWaiting();

    // Execute actions until we find an acquire action or run out of actions
    while (m_context.current_action < m_scenario.actions.size()) {
        auto action = GetCurrentAction();
        if (!action) {
            break;
        }

        if (action->type == ActionType::Acquire) {
            // Generate frame
            auto frame = GenerateFrame();
            m_context.frames_generated++;

            // Check if we've generated enough frames
            if (action->count > 0 &&
                static_cast<int>(m_context.frames_generated) >= action->count) {
                m_context.frames_generated = 0;
                m_context.current_action++;
            }

            return frame;
        }

        // Execute non-acquire actions
        if (!ExecuteAction(*action)) {
            return std::nullopt;
        }

        if (m_context.waiting) {
            return std::nullopt;  // Waiting, no frame this time
        }

        m_context.current_action++;
    }

    // Scenario complete
    if (m_context.current_action >= m_scenario.actions.size()) {
        m_running = false;
    }

    return std::nullopt;
}

DetectorState ScenarioEngine::GetCurrentState() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_context.current_state;
}

std::optional<ErrorCode> ScenarioEngine::GetNextError() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running) {
        return std::nullopt;
    }

    // Check current action for error injection
    auto action = GetCurrentAction();
    if (action && action->type == ActionType::InjectError) {
        if (ShouldInjectError(action->probability)) {
            auto error = stringToErrorCode(action->error);
            if (error) {
                m_context.current_action++;
                return error;
            }
        }
        m_context.current_action++;
    }

    return std::nullopt;
}

bool ScenarioEngine::IsComplete() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_context.current_action >= m_scenario.actions.size();
}

const Scenario& ScenarioEngine::GetScenario() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_scenario;
}

void ScenarioEngine::Reset() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_context.current_action = 0;
    m_context.frames_generated = 0;
    m_context.current_state = DetectorState::IDLE;
    m_context.waiting = false;
    m_context.parameters.clear();
    m_context.last_action_time = std::chrono::steady_clock::now();
}

void ScenarioEngine::SetFrameConfig(uint32_t width, uint32_t height, uint32_t bitDepth) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_frame_width = width;
    m_frame_height = height;
    m_frame_bit_depth = bitDepth;
}

std::string ScenarioEngine::GetParameter(const std::string& name) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_context.parameters.find(name);
    return it != m_context.parameters.end() ? it->second : "";
}

void ScenarioEngine::SetParameter(const std::string& name, const std::string& value) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_context.parameters[name] = value;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

std::optional<ScenarioAction> ScenarioEngine::GetCurrentAction() const {
    if (m_context.current_action < m_scenario.actions.size()) {
        return m_scenario.actions[m_context.current_action];
    }
    return std::nullopt;
}

bool ScenarioEngine::ExecuteAction(const ScenarioAction& action) {
    switch (action.type) {
        case ActionType::Wait:
            m_context.waiting = true;
            m_context.wait_start = std::chrono::steady_clock::now();
            m_context.wait_duration_ms = action.duration_ms;
            return true;

        case ActionType::SetState: {
            auto state = stringToDetectorState(action.state);
            if (state) {
                m_context.current_state = *state;
                return true;
            }
            return false;
        }

        case ActionType::SetParameter:
            m_context.parameters[action.parameter] = action.value;
            return true;

        case ActionType::Calibration:
            // Calibration is a no-op in emulation, just advance state
            m_context.current_state = DetectorState::READY;
            return true;

        case ActionType::Acquire:
        case ActionType::InjectError:
            // These are handled elsewhere
            return true;

        default:
            return false;
    }
}

FrameData ScenarioEngine::GenerateFrame() {
    FrameData frame;
    frame.width = m_frame_width;
    frame.height = m_frame_height;
    frame.bitDepth = m_frame_bit_depth;
    frame.frameNumber = static_cast<uint64_t>(m_context.frames_generated);
    frame.timestamp = std::chrono::duration<double>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    size_t pixel_count = static_cast<size_t>(frame.width) * frame.height;
    size_t bytes_per_pixel = (frame.bitDepth + 7) / 8;
    frame.dataLength = pixel_count * bytes_per_pixel;
    frame.data = std::shared_ptr<uint8_t[]>(new uint8_t[frame.dataLength]);

    // Generate gradient pattern for testing
    if (bytes_per_pixel == 2) {
        // 16-bit grayscale
        uint16_t* pixels = reinterpret_cast<uint16_t*>(frame.data.get());
        for (uint32_t y = 0; y < frame.height; ++y) {
            for (uint32_t x = 0; x < frame.width; ++x) {
                size_t idx = y * frame.width + x;
                // Horizontal gradient with some vertical variation
                uint16_t value = static_cast<uint16_t>(
                    (x * 65535 / frame.width + y * 16384 / frame.height) % 65536);
                pixels[idx] = value;
            }
        }
    } else {
        // 8-bit grayscale
        uint8_t* pixels = frame.data.get();
        for (uint32_t y = 0; y < frame.height; ++y) {
            for (uint32_t x = 0; x < frame.width; ++x) {
                size_t idx = y * frame.width + x;
                uint8_t value = static_cast<uint8_t>(
                    (x * 255 / frame.width + y * 64 / frame.height) % 256);
                pixels[idx] = value;
            }
        }
    }

    return frame;
}

void ScenarioEngine::ProcessWaiting() {
    if (!m_context.waiting) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_context.wait_start).count();

    if (elapsed >= m_context.wait_duration_ms) {
        m_context.waiting = false;
        m_context.current_action++;
    }
}

bool ScenarioEngine::ShouldInjectError(double probability) const {
    if (probability <= 0.0) return false;
    if (probability >= 1.0) return true;
    return m_dist(m_rng) < probability;
}

// ============================================================================
// JSON Parsing - Minimal Parser for Scenario Format
// ============================================================================

bool ScenarioEngine::ParseScenario(const std::string& json) {
    // Clear current scenario
    m_scenario = Scenario();
    m_context = ExecutionContext();

    // Extract scenario name
    auto name = ExtractString(json, "name");
    if (name) {
        m_scenario.name = *name;
    } else {
        m_scenario.name = "Unnamed Scenario";
    }

    // Extract description
    auto desc = ExtractString(json, "description");
    if (desc) {
        m_scenario.description = *desc;
    }

    // Extract actions array
    auto actions_json = ExtractArray(json, "actions");
    if (actions_json.empty()) {
        return true;  // Empty scenario is valid
    }

    // Parse each action
    for (const auto& action_json : actions_json) {
        ScenarioAction action;

        // Extract action type
        auto type_str = ExtractString(action_json, "type");
        if (!type_str) {
            continue;  // Skip invalid actions
        }

        auto type = stringToActionType(*type_str);
        if (!type) {
            continue;  // Skip unknown action types
        }
        action.type = *type;

        // Extract action-specific fields
        switch (action.type) {
            case ActionType::Wait:
                if (auto duration = ExtractInt(action_json, "duration_ms")) {
                    action.duration_ms = *duration;
                }
                break;

            case ActionType::SetState:
                if (auto state = ExtractString(action_json, "state")) {
                    action.state = *state;
                }
                break;

            case ActionType::Acquire:
                if (auto count = ExtractInt(action_json, "count")) {
                    action.count = *count;
                }
                if (auto interval = ExtractInt(action_json, "interval_ms")) {
                    action.interval_ms = *interval;
                }
                break;

            case ActionType::InjectError:
                if (auto error = ExtractString(action_json, "error")) {
                    action.error = *error;
                }
                if (auto prob = ExtractDouble(action_json, "probability")) {
                    action.probability = *prob;
                }
                break;

            case ActionType::SetParameter:
                if (auto param = ExtractString(action_json, "parameter")) {
                    action.parameter = *param;
                }
                if (auto val = ExtractString(action_json, "value")) {
                    action.value = *val;
                }
                break;

            case ActionType::Calibration:
                // Calibration has no additional parameters
                break;
        }

        m_scenario.actions.push_back(action);
    }

    return true;
}

std::string ScenarioEngine::Trim(const std::string& str) const {
    size_t start = 0;
    while (start < str.length() && std::isspace(static_cast<unsigned char>(str[start]))) {
        start++;
    }

    if (start == str.length()) {
        return "";
    }

    size_t end = str.length() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(str[end]))) {
        end--;
    }

    return str.substr(start, end - start + 1);
}

std::optional<std::string> ScenarioEngine::ExtractString(
    const std::string& json, const std::string& key) {

    std::string search_key = "\"" + key + "\"";
    size_t key_pos = json.find(search_key);

    if (key_pos == std::string::npos) {
        return std::nullopt;
    }

    // Find colon after key
    size_t colon_pos = json.find(':', key_pos);
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }

    // Find opening quote of value
    size_t value_start = json.find('\"', colon_pos);
    if (value_start == std::string::npos) {
        return std::nullopt;
    }
    value_start++;  // Skip opening quote

    // Find closing quote
    size_t value_end = value_start;
    bool escaped = false;
    while (value_end < json.length()) {
        if (escaped) {
            escaped = false;
            value_end++;
            continue;
        }
        if (json[value_end] == '\\') {
            escaped = true;
            value_end++;
            continue;
        }
        if (json[value_end] == '\"') {
            break;
        }
        value_end++;
    }

    if (value_end >= json.length()) {
        return std::nullopt;
    }

    std::string value = json.substr(value_start, value_end - value_start);
    return UnescapeString(value);
}

std::optional<int> ScenarioEngine::ExtractInt(
    const std::string& json, const std::string& key) {

    std::string search_key = "\"" + key + "\"";
    size_t key_pos = json.find(search_key);

    if (key_pos == std::string::npos) {
        return std::nullopt;
    }

    // Find colon after key
    size_t colon_pos = json.find(':', key_pos);
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }

    // Skip whitespace after colon
    size_t value_start = colon_pos + 1;
    while (value_start < json.length() &&
           std::isspace(static_cast<unsigned char>(json[value_start]))) {
        value_start++;
    }

    // Parse integer
    size_t value_end = value_start;
    bool negative = false;
    if (value_end < json.length() && json[value_end] == '-') {
        negative = true;
        value_end++;
    }
    while (value_end < json.length() &&
           std::isdigit(static_cast<unsigned char>(json[value_end]))) {
        value_end++;
    }

    if (value_end == value_start) {
        return std::nullopt;
    }

    std::string value_str = json.substr(value_start, value_end - value_start);
    try {
        return std::stoi(value_str);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<double> ScenarioEngine::ExtractDouble(
    const std::string& json, const std::string& key) {

    std::string search_key = "\"" + key + "\"";
    size_t key_pos = json.find(search_key);

    if (key_pos == std::string::npos) {
        return std::nullopt;
    }

    // Find colon after key
    size_t colon_pos = json.find(':', key_pos);
    if (colon_pos == std::string::npos) {
        return std::nullopt;
    }

    // Skip whitespace after colon
    size_t value_start = colon_pos + 1;
    while (value_start < json.length() &&
           std::isspace(static_cast<unsigned char>(json[value_start]))) {
        value_start++;
    }

    // Parse double
    size_t value_end = value_start;
    bool negative = false;
    if (value_end < json.length() && json[value_end] == '-') {
        negative = true;
        value_end++;
    }
    while (value_end < json.length() &&
           (std::isdigit(static_cast<unsigned char>(json[value_end])) ||
            json[value_end] == '.')) {
        value_end++;
    }

    if (value_end == value_start) {
        return std::nullopt;
    }

    std::string value_str = json.substr(value_start, value_end - value_start);
    try {
        return std::stod(value_str);
    } catch (...) {
        return std::nullopt;
    }
}

std::vector<std::string> ScenarioEngine::ExtractArray(
    const std::string& json, const std::string& key) {

    std::vector<std::string> result;
    std::string search_key = "\"" + key + "\"";
    size_t key_pos = json.find(search_key);

    if (key_pos == std::string::npos) {
        return result;
    }

    // Find colon after key
    size_t colon_pos = json.find(':', key_pos);
    if (colon_pos == std::string::npos) {
        return result;
    }

    // Find opening bracket
    size_t bracket_start = json.find('[', colon_pos);
    if (bracket_start == std::string::npos) {
        return result;
    }

    // Find matching closing bracket
    int depth = 1;
    size_t bracket_end = bracket_start + 1;
    while (bracket_end < json.length() && depth > 0) {
        if (json[bracket_end] == '[') depth++;
        if (json[bracket_end] == ']') depth--;
        bracket_end++;
    }

    if (depth != 0) {
        return result;
    }

    std::string array_content = json.substr(bracket_start + 1, bracket_end - bracket_start - 2);

    // Extract objects from array
    size_t pos = 0;
    while (pos < array_content.length()) {
        // Skip whitespace
        while (pos < array_content.length() &&
               std::isspace(static_cast<unsigned char>(array_content[pos]))) {
            pos++;
        }
        if (pos >= array_content.length()) break;

        // Find object start
        if (array_content[pos] == '{') {
            int obj_depth = 1;
            size_t obj_start = pos;
            size_t obj_end = pos + 1;

            while (obj_end < array_content.length() && obj_depth > 0) {
                if (array_content[obj_end] == '{') obj_depth++;
                if (array_content[obj_end] == '}') obj_depth--;
                obj_end++;
            }

            if (obj_depth == 0) {
                result.push_back(array_content.substr(obj_start, obj_end - obj_start));
                pos = obj_end;
                continue;
            }
        }

        pos++;
    }

    return result;
}

std::string ScenarioEngine::UnescapeString(const std::string& str) const {
    std::string result;
    result.reserve(str.length());

    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            switch (str[i + 1]) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case 'r': result += '\r'; break;
                case '\\': result += '\\'; break;
                case '\"': result += '\"'; break;
                default: result += str[i + 1]; break;
            }
            i++;
        } else {
            result += str[i];
        }
    }

    return result;
}

} // namespace emul
} // namespace adapters
} // namespace uxdi
