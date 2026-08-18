#pragma once
#include <vector>
#include <utility>
#include <array>

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

    // MSAA support for fillTriangle: each pixel is split into
    // kMSAASamples sub-pixel sample points, each independently
    // depth-tested against the geometry that lands on it. A triangle
    // edge crossing the middle of a pixel then only covers some of that
    // pixel's samples -- resolveMSAA() averages each pixel's covered
    // samples' shaded intensity into one antialiased glyph (uncovered
    // samples count as 0, i.e. background), instead of the pixel being
    // all-or-nothing the way a single-sample test would leave it.
    static constexpr int kMSAASamples = 4;
    std::vector<float> sampleDepth;     // kMSAASamples entries per pixel
    std::vector<float> sampleIntensity; // kMSAASamples entries per pixel

    Framebuffer(int w, int h);

    void clear();

    void set(int x, int y, char c);

    // Depth-tested set: only writes if `d` is closer than what's already
    // at (x,y) (smaller = closer, matching NDC z where -1 is the near
    // plane). Updates the depth buffer on write so later triangles get
    // correctly occluded.
    void setDepthTested(int x, int y, float d, char c);

    // Depth-tested write to a single MSAA subsample of pixel (x,y).
    // sampleIdx must be in [0, kMSAASamples). Used by fillTriangle
    // instead of setDepthTested so edge coverage survives to resolve time.
    void setSampleDepthTested(int x, int y, int sampleIdx, float d, float intensity);

    // Resolves this frame's MSAA subsamples into `chars`: for each pixel
    // with at least one covered sample, averages the covered samples'
    // intensity (uncovered ones count as 0) and maps it through
    // shadeChar(). Pixels with no covered samples are left alone, so this
    // composes with plain fb.set()/setDepthTested() writes (e.g. a future
    // wireframe pass) rather than overwriting them. Call once per frame,
    // after all fillTriangle calls and before present().
    void resolveMSAA();

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
// Fill a triangle given three already-projected/viewport-transformed
// screen vertices. This is the solid-rendering counterpart to
// drawLine -- instead of tracing an outline, decide which pixels are
// *inside* the triangle and paint them, MSAA-sampled and depth-tested
// via fb.setSampleDepthTested() so nearer triangles correctly occlude
// farther ones instead of just overwriting in draw order.
//
// Classic approach (Pineda's edge-function algorithm): for a candidate
// sample point, compute the signed area of each of the triangle's three
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
//
// `intensity` is the triangle's already-computed (flat) shading value
// in [0,1] rather than a final char -- resolveMSAA() is what quantizes
// down to a glyph, once per pixel, after blending together whatever
// mix of samples (this triangle's, another triangle's, or background)
// ended up covering it. Quantizing per-triangle instead would make
// antialiasing at a shared or silhouette edge meaningless: there'd be
// no continuous value left to blend.
// ---------------------------------------------------------------
float edgeFunction(ScreenVertex a, ScreenVertex b, ScreenVertex c);
// Same, but for a sample point that isn't pixel-grid-aligned (an MSAA
// subsample), which only needs a position, not a full ScreenVertex.
float edgeFunction(ScreenVertex a, ScreenVertex b, float px, float py);

bool isInside(ScreenVertex p, ScreenVertex a, ScreenVertex b, ScreenVertex c);
bool isInside(float px, float py, ScreenVertex a, ScreenVertex b, ScreenVertex c);

void fillTriangle(Framebuffer& fb, const ScreenVertex& a, const ScreenVertex& b, const ScreenVertex& c, float intensity);

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
// Full per-frame pipeline for one solid-shaded mesh: model -> view ->
// proj -> perspective divide -> viewport, per the usual stages, then
// per triangle: back-face cull (faceNormal vs. direction to camera),
// Lambertian-shade the survivors (lambertIntensity -> shadeChar), and
// rasterize with fillTriangle so the z-buffer resolves overlaps.
// `edges` is unused by this solid path (kept for callers that still
// want wireframe via drawLine*) -- pass an empty vector if you don't.
// ---------------------------------------------------------------
void render(Framebuffer& fb, const std::vector<Vec3>& vertices,
            const std::vector<std::pair<int, int>>& edges,
            const std::vector<std::array<int, 3>>& triangles,
            const Mat4& model, const Camera& camera);
