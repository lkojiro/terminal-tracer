#pragma once
#include <cmath>
#include "vec3.hpp"

// Column-major 4x4 matrix — m[col][row]. This matches the convention
// used in most graphics references (and GLSL), which makes it easier
// to translate formulas from textbooks/papers directly into code.
struct Mat4 {
    float m[4][4] = {};

    static Mat4 identity() {
        Mat4 r;
        for (int i = 0; i < 4; i++) r.m[i][i] = 1.0f;
        return r;
    }

    // Matrix multiplication is pure mechanical boilerplate — implemented
    // for you. Everything interesting is in *which* matrices you build
    // and *what order* you multiply them in (that order is one of the
    // most common sources of bugs in a first rasterizer — worth
    // understanding why it matters before you start chaining these).
    Mat4 operator*(const Mat4& o) const {
        Mat4 r;
        for (int col = 0; col < 4; col++) {
            for (int row = 0; row < 4; row++) {
                float sum = 0.0f;
                for (int k = 0; k < 4; k++) {
                    sum += m[k][row] * o.m[col][k];
                }
                r.m[col][row] = sum;
            }
        }
        return r;
    }

    Vec4 operator*(const Vec4& v) const {
        Vec4 r;
        r.x = m[0][0]*v.x + m[1][0]*v.y + m[2][0]*v.z + m[3][0]*v.w;
        r.y = m[0][1]*v.x + m[1][1]*v.y + m[2][1]*v.z + m[3][1]*v.w;
        r.z = m[0][2]*v.x + m[1][2]*v.y + m[2][2]*v.z + m[3][2]*v.w;
        r.w = m[0][3]*v.x + m[1][3]*v.y + m[2][3]*v.z + m[3][3]*v.w;
        return r;
    }

    static Mat4 translate(float x, float y, float z) {
        Mat4 r = identity();
        r.m[3][0] = x;
        r.m[3][1] = y;
        r.m[3][2] = z;
        return r;
    }

    static Mat4 scale(float x, float y, float z) {
        Mat4 r = identity();
        r.m[0][0] = x;
        r.m[1][1] = y;
        r.m[2][2] = z;
        return r;
    }

    // --- TODO(you): rotateY ---
    // Build a rotation matrix around the Y axis for angle `radians`.
    // This is what will make the cube spin for your week-1 milestone.
    //
    // Think about: which two axes does a Y-rotation actually mix
    // together, and where do sin/cos and their signs need to go so
    // the rotation goes the direction you expect? Sketch it on paper
    // with a unit vector on the X axis before you write the matrix —
    // it'll save you a debugging headache later.
    static Mat4 rotateY(float radians) {
        (void)radians;
        return identity(); // replace with your rotation matrix
    }

    // --- TODO(you): perspective projection ---
    // Build a perspective projection matrix from:
    //   fovYRadians - vertical field of view
    //   aspect      - width/height of your "screen" (terminal aspect,
    //                 careful: terminal characters are NOT square!)
    //   nearZ, farZ - clipping planes
    //
    // This is the single most important matrix in the whole pipeline —
    // it's what puts the "3D" in your renderer, by making things
    // further away appear smaller and encoding depth into w for the
    // perspective divide. Worth deriving it from the similar-triangles
    // argument rather than just copying a formula, since this is
    // exactly the kind of thing a graphics interview will ask you to
    // explain or derive on a whiteboard.
    static Mat4 perspective(float fovYRadians, float aspect, float nearZ, float farZ) {
        (void)fovYRadians; (void)aspect; (void)nearZ; (void)farZ;
        return identity(); // replace with your projection matrix
    }

    // --- TODO(you): lookAt / view matrix ---
    // Build a view matrix from an eye position, a target to look at,
    // and an up vector. Conceptually: construct an orthonormal basis
    // (right, up, forward) at the eye, then express world-space points
    // in terms of that basis.
    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        (void)eye; (void)target; (void)up;
        return identity(); // replace with your view matrix
    }
};
