#pragma once

#include <cstdint>

namespace recoil {

enum class CommandKind : uint8_t {
    Fire,
    Reload,
    Stop,
};

struct Command {
    CommandKind kind = CommandKind::Stop;
    uint8_t player = 0;
    float yaw_degrees = 0.0f;
    float pitch_degrees = 0.0f;
};

} // namespace recoil
