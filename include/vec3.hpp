#pragma once
#include <cmath>

// A basic 3D vector. This is pure boilerplate — the operations here
// are the same in every graphics codebase, so it's implemented for
// you. The interesting math starts in mat4.hpp and in your
// rasterization loop in main.cpp.
struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;

    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator-() const { return {-x, -y, -z}; }

    float dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }

    Vec3 cross(const Vec3& o) const {
        return {
            y * o.z - z * o.y,
            z * o.x - x * o.z,
            x * o.y - y * o.x
        };
    }

    float length() const { return std::sqrt(dot(*this)); }

    Vec3 normalized() const {
        float len = length();
        if (len < 1e-8f) return {0, 0, 0};
        return {x / len, y / len, z / len};
    }
};

// A 4-component vector, used for homogeneous coordinates (the "w"
// component matters a lot once you get to the perspective divide —
// worth reviewing why w isn't always 1 before you implement that step).
struct Vec4 {
    float x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;

    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vec4(const Vec3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}

    // Needed to linearly interpolate a clip-space vertex (all 4
    // components together, so a new vertex's w comes along for free) when
    // clipping a triangle against the near/far planes -- see
    // clipTriangleNearFar() in render.hpp.
    Vec4 operator+(const Vec4& o) const { return {x + o.x, y + o.y, z + o.z, w + o.w}; }
    Vec4 operator-(const Vec4& o) const { return {x - o.x, y - o.y, z - o.z, w - o.w}; }
    Vec4 operator*(float s) const { return {x * s, y * s, z * s, w * s}; }

    Vec3 toVec3() const { return {x, y, z}; }
};
