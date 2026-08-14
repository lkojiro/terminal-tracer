#pragma once
#include <vector>
#include <utility>

#include "vec3.hpp"
#include "mat4.hpp"

// ---------------------------------------------------------------
// Terminal framebuffer: a grid of chars we print each frame, plus a
// per-pixel depth buffer for solid triangle fill. This part is
// plumbing, not graphics theory — implemented for you so you can
// focus on the pipeline below. See render.cpp for definitions.
// ---------------------------------------------------------------
struct Framebuffer {
    int width, height;
    std::vector<char> chars;
    std::vector<float> depth; // per-pixel z-buffer, for solid triangle fill

    Framebuffer(int w, int h);

    void clear();

    void set(int x, int y, char c);

    // Depth-tested set: only writes if `d` is closer than what's already
    // at (x,y) (smaller = closer, matching NDC z where -1 is the near
    // plane). Updates the depth buffer on write so later triangles get
    // correctly occluded.
    void setDepthTested(int x, int y, float d, char c);

    void present() const;
};

// ---------------------------------------------------------------
// TODO(you): Bresenham (or similar) line drawing between two
// screen-space points. This is the "rasterization" part of a
// rasterizer — given two integer pixel coordinates, decide which
// pixels in between get lit up. Bresenham's algorithm is the
// classic approach; worth understanding why it avoids floating
// point division per-pixel.
// ---------------------------------------------------------------
void drawLine(Framebuffer& fb, int x0, int y0, int x1, int y1, char c);

// bresenham's algorithm
//
// Integer-only variant (Zingl's formulation): a single error term
// tracks how far the ideal line has drifted from the pixel grid, and
// gets nudged by dx/dy each step. Handles all octants uniformly (no
// separate steep/shallow cases) by working in the +x/+y-normalized
// step directions sx/sy.
void drawLineBresenham(Framebuffer& fb, int x0, int y0, int x1, int y1, char c);

// xiaolin wu's algorithm
//
// Same idea as Bresenham but instead of picking one pixel per step,
// it splits the line's true (fractional) position between the two
// pixels straddling it, weighted by how close the line passes to
// each. That fractional "coverage" is what gives antialiasing —
// quantized into a small ASCII density ramp (shadeChar(), file-local
// to render.cpp) since a terminal cell can't be partially lit.
void drawLineWu(Framebuffer& fb, int x0, int y0, int x1, int y1);

// A screen-space vertex, ready for triangle rasterization: pixel
// coordinates plus a depth value for the z-buffer test. Interpolating
// this depth across the triangle (via barycentric weights) is what lets
// fillTriangle() resolve overlapping triangles correctly.
struct ScreenVertex {
    int x, y;
    float depth; // pick a convention: NDC z is simplest to start with;
                 // 1/w gives perspective-correct interpolation later
};

// ---------------------------------------------------------------
// TODO(you): fill a triangle given three already-projected/viewport-
// transformed screen vertices. This is the solid-rendering counterpart
// to drawLine -- instead of tracing an outline, decide which pixels
// are *inside* the triangle and paint them, using
// fb.setDepthTested() so nearer triangles correctly occlude farther
// ones instead of just overwriting in draw order.
//
// Classic approach (Pineda's edge-function algorithm): for a candidate
// pixel (x,y), compute the signed area of each of the triangle's three
// edges relative to that point (a 2D cross product per edge — same
// shape of calculation as a dot/cross product, just in 2D). The point
// is inside iff all three signs agree (consistently >=0 or <=0,
// depending on the triangle's winding order). Those same three
// edge-function values, normalized by the triangle's total area, ARE
// the barycentric weights (w0, w1, w2) -- useful for interpolating
// depth (and later, per-vertex normals/colors) across the triangle:
// depth = w0*a.depth + w1*b.depth + w2*c.depth.
//
// Iterate only the triangle's screen-space bounding box (clamped to
// the framebuffer dimensions), not the whole screen.
// ---------------------------------------------------------------
void fillTriangle(Framebuffer& fb, const ScreenVertex& a, const ScreenVertex& b, const ScreenVertex& c, char shade);

// TODO(you): a triangle's face normal, from its three vertex
// positions (world-space, or model-space if you're consistent about
// which space you compute/use it in) -- cross product of two edges.
// Used for both back-face culling (dot the normal against the
// direction to the camera; skip triangles facing away) and Lambertian
// lighting (dot the normal against the light direction). Mind
// winding: cubeTriangles in main.cpp is wound counter-clockwise as
// viewed from outside the cube, so a consistent edge order here
// should come out pointing outward, away from the cube's center.
Vec3 faceNormal(const Vec3& a, const Vec3& b, const Vec3& c);

// TODO(you): basic Lambertian shading -- how lit a surface looks is
// proportional to the cosine of the angle between its normal and the
// direction toward the light, i.e. dot(normal, lightDir), clamped to
// [0,1] (a face angled away from the light shouldn't go negative).
// Feed the result into shadeChar() to pick a character.
float lambertIntensity(const Vec3& normal, const Vec3& lightDir);

// ---------------------------------------------------------------
// Camera: eye position, look-at target, up vector, and projection
// params (fov/near/far). right()/forward() derive the camera's
// basis vectors from pos/target/up each time they're called, the
// same way lookAt() derives them internally -- so arrow-key
// rotation axes stay camera-relative even if pos/target ever move.
// ---------------------------------------------------------------
struct Camera {
    Vec3 pos;
    Vec3 target;
    Vec3 up;
    float fovY;
    float nearZ, farZ;

    Vec3 forward() const;
    Vec3 right() const;

    Mat4 viewMatrix() const;
    Mat4 projMatrix(float aspect) const;
};

// ---------------------------------------------------------------
// Full per-frame pipeline for one wireframe mesh: model -> view ->
// proj -> perspective divide -> viewport, then draw edges between
// the transformed screen-space points.
// ---------------------------------------------------------------
void render(Framebuffer& fb, const std::vector<Vec3>& vertices,
            const std::vector<std::pair<int, int>>& edges,
            const Mat4& model, const Camera& camera);
