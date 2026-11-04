#pragma once

#include <cmath>

namespace recoil {

// Source 单位：1u = 1 英寸 = 0.0254m；X 前 / Y 左 / Z 上（右手系）。
struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    friend Vec3 operator+(const Vec3& a, const Vec3& b) {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    }
    friend Vec3 operator-(const Vec3& a, const Vec3& b) {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }
    friend Vec3 operator*(const Vec3& v, float s) {
        return {v.x * s, v.y * s, v.z * s};
    }
};

inline float Dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

inline float Length(const Vec3& v) {
    return std::sqrt(Dot(v, v));
}

inline Vec3 Normalize(const Vec3& v) {
    const float len = Length(v);
    if (len <= 0.0f) {
        return {};
    }
    return {v.x / len, v.y / len, v.z / len};
}

// 视图角 -> 单位方向（yaw 绕 Z，pitch 向上为正）。
inline Vec3 DirectionFromAngles(float yaw_degrees, float pitch_degrees) {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kDegToRad = kPi / 180.0f;
    const float yaw = yaw_degrees * kDegToRad;
    const float pitch = pitch_degrees * kDegToRad;
    return {std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch)};
}

} // namespace recoil
