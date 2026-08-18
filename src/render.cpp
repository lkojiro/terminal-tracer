#include "render.hpp"

#include <string>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <limits>

// Forward-declared so Framebuffer::resolveMSAA() (below) can use it;
// defined further down alongside the rest of the line-drawing code it
// was originally written for.
static char shadeChar(float coverage);

// --- Framebuffer ---------------------------------------------------

Framebuffer::Framebuffer(int w, int h)
    : width(w), height(h), chars(w * h, ' '),
      depth(w * h, std::numeric_limits<float>::infinity()),
      sampleDepth(w * h * kMSAASamples, std::numeric_limits<float>::infinity()),
      sampleIntensity(w * h * kMSAASamples, 0.0f) {}

void Framebuffer::clear() {
    std::fill(chars.begin(), chars.end(), ' ');
    std::fill(depth.begin(), depth.end(), std::numeric_limits<float>::infinity());
    std::fill(sampleDepth.begin(), sampleDepth.end(), std::numeric_limits<float>::infinity());
    std::fill(sampleIntensity.begin(), sampleIntensity.end(), 0.0f);
}

void Framebuffer::set(int x, int y, char c) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    chars[y * width + x] = c;
}

void Framebuffer::setDepthTested(int x, int y, float d, char c) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    int idx = y * width + x;
    if (d < depth[idx]) {
        depth[idx] = d;
        chars[idx] = c;
    }
}

void Framebuffer::setSampleDepthTested(int x, int y, int sampleIdx, float d, float intensity) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    int idx = (y * width + x) * kMSAASamples + sampleIdx;
    if (d < sampleDepth[idx]) {
        sampleDepth[idx] = d;
        sampleIntensity[idx] = intensity;
    }
}

void Framebuffer::resolveMSAA() {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int base = (y * width + x) * kMSAASamples;
            float sum = 0.0f;
            bool anyCovered = false;
            float nearest = std::numeric_limits<float>::infinity();
            for (int s = 0; s < kMSAASamples; s++) {
                // An uncovered sample is still sitting at its cleared
                // sentinel depth (infinity) -- no triangle ever claimed
                // it this frame -- so it contributes 0 (background).
                if (sampleDepth[base + s] < std::numeric_limits<float>::infinity()) {
                    sum += sampleIntensity[base + s];
                    nearest = std::min(nearest, sampleDepth[base + s]);
                    anyCovered = true;
                }
            }
            // Leave pixels no triangle touched alone this frame, so this
            // composes with plain fb.set()/setDepthTested() writes rather
            // than stomping them back to blank.
            if (!anyCovered) continue;

            int idx = y * width + x;
            chars[idx] = shadeChar(sum / kMSAASamples);
            depth[idx] = nearest;
        }
    }
}

void Framebuffer::present() const {
    // Move cursor to top-left instead of clearing the whole
    // terminal each frame — clearing causes visible flicker.
    std::string out = "\x1b[H";
    out.reserve(chars.size() + height + 16);
    for (int y = 0; y < height; y++) {
        out.append(&chars[y * width], width);
        if (y != height - 1) out.push_back('\n'); // no trailing '\n' -- would scroll the last row off
    }
    std::fwrite(out.data(), 1, out.size(), stdout);
    std::fflush(stdout);
}

// --- Line drawing ----------------------------------------------------

void drawLine(Framebuffer& fb, int x0, int y0, int x1, int y1, char c) {
    (void)fb; (void)x0; (void)y0; (void)x1; (void)y1; (void)c;
    // replace with your line-drawing implementation
}

void drawLineBresenham(Framebuffer& fb, int x0, int y0, int x1, int y1, char c) {
    int dx = std::abs(x1 - x0);
    int sx = (x0 < x1) ? 1 : -1;
    int dy = -std::abs(y1 - y0);
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    int x = x0, y = y0;
    while (true) {
        fb.set(x, y, c);
        if (x == x1 && y == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { // step in x
            err += dy;
            x += sx;
        }
        if (e2 <= dx) { // step in y
            err += dx;
            y += sy;
        }
    }
}

// Maps a coverage fraction in [0,1] to one of a small set of ASCII
// "density" glyphs, so Wu's algorithm has something to antialias
// *into* even though a terminal cell can't do real alpha blending.
// File-local -- not part of render.hpp's public interface.
static char shadeChar(float coverage) {
    // ASCII-art density ramp from Paul Bourke
    static const char ramp[] = {
        ' ', '.', '\'', '`', '^', '"', ',', ':', ';', 'I', 'l', '!',
        'i', '>', '<', '~', '+', '_', '-', '?', ']', '[', '}', '{',
        '1', ')', '(', '|', '/', 't', 'f', 'j', 'r', 'x', 'n', 'u',
        'v', 'c', 'z', 'X', 'Y', 'U', 'J', 'C', 'L', 'Q', '0', 'O',
        'Z', 'm', 'w', 'q', 'p', 'd', 'b', 'k', 'h', 'a', 'o', '*',
        '#', 'M', 'W', '&', '8', '%', 'B', '@', '$'
    };
    constexpr int rampMax = int(sizeof(ramp) / sizeof(ramp[0])) - 1;
    if (coverage <= 0.0f) return ' ';
    int idx = std::clamp(int(coverage * (rampMax + 1)), 1, rampMax);
    return ramp[idx];
}

void drawLineWu(Framebuffer& fb, int x0, int y0, int x1, int y1) {
    auto ipart  = [](float x) { return std::floor(x); };
    auto fpart  = [](float x) { return x - std::floor(x); };
    auto rfpart = [&](float x) { return 1.0f - fpart(x); };
    auto plot   = [&](int x, int y, float brightness) {
        fb.set(x, y, shadeChar(brightness));
    };

    bool steep = std::abs(y1 - y0) > std::abs(x1 - x0);
    if (steep) { std::swap(x0, y0); std::swap(x1, y1); }
    if (x0 > x1) { std::swap(x0, x1); std::swap(y0, y1); }

    float dx = float(x1 - x0);
    float dy = float(y1 - y0);
    float gradient = (dx == 0.0f) ? 1.0f : dy / dx;

    // First endpoint.
    float xend = std::round(float(x0));
    float yend = y0 + gradient * (xend - x0);
    float xgap = rfpart(x0 + 0.5f);
    int xpxl1 = int(xend);
    int ypxl1 = int(ipart(yend));
    if (steep) {
        plot(ypxl1,     xpxl1, rfpart(yend) * xgap);
        plot(ypxl1 + 1, xpxl1, fpart(yend)  * xgap);
    } else {
        plot(xpxl1, ypxl1,     rfpart(yend) * xgap);
        plot(xpxl1, ypxl1 + 1, fpart(yend)  * xgap);
    }
    float intery = yend + gradient;

    // Second endpoint.
    xend = std::round(float(x1));
    yend = y1 + gradient * (xend - x1);
    xgap = fpart(x1 + 0.5f);
    int xpxl2 = int(xend);
    int ypxl2 = int(ipart(yend));
    if (steep) {
        plot(ypxl2,     xpxl2, rfpart(yend) * xgap);
        plot(ypxl2 + 1, xpxl2, fpart(yend)  * xgap);
    } else {
        plot(xpxl2, ypxl2,     rfpart(yend) * xgap);
        plot(xpxl2, ypxl2 + 1, fpart(yend)  * xgap);
    }

    // Interior pixels: step one column (or row, if steep) at a time,
    // splitting brightness between the two pixels straddling `intery`.
    if (steep) {
        for (int x = xpxl1 + 1; x <= xpxl2 - 1; x++) {
            plot(int(ipart(intery)),     x, rfpart(intery));
            plot(int(ipart(intery)) + 1, x, fpart(intery));
            intery += gradient;
        }
    } else {
        for (int x = xpxl1 + 1; x <= xpxl2 - 1; x++) {
            plot(x, int(ipart(intery)),     rfpart(intery));
            plot(x, int(ipart(intery)) + 1, fpart(intery));
            intery += gradient;
        }
    }
}

// --- Triangle rasterization / lighting (stubs) ------------------------

// test if point (px, py) is to the 'right' of line a-b (return a float so we can do line testing + barycentric coordinates for free
// < 0 -> point to the left, = 0 -> on the line, > 0 -> to the right
//
// Takes the point as raw coordinates rather than a ScreenVertex so it
// can also be called with an MSAA subsample position, which isn't
// pixel-grid-aligned and doesn't need a depth of its own.
float edgeFunction(ScreenVertex a, ScreenVertex b, float px, float py) {
    return (px - a.x) * (b.y - a.y) - (py - a.y) * (b.x - a.x);
}

float edgeFunction(ScreenVertex a, ScreenVertex b, ScreenVertex c) {
    return edgeFunction(a, b, (float)c.x, (float)c.y);
}

// is edge a->b a "top" or "left" edge of the triangle it belongs to?
// top: exactly horizontal, running right-to-left. left: running downward.
// (screen space has y increasing downward, so these fall out of which way
// dx/dy point rather than the usual up-is-up intuition.)
static bool isTopLeft(ScreenVertex a, ScreenVertex b) {
    int dx = b.x - a.x;
    int dy = b.y - a.y;
    bool top = (dy == 0) && (dx < 0);
    bool left = dy > 0;
    return top || left;
}

// is point (px, py) inside triangle (a, b, c) ** in ccw order
// Points exactly on a top or left edge count as inside; points exactly on
// any other edge don't (the "top-left" fill rule). Without this, a pixel
// sitting precisely on an edge shared by two triangles would either be
// drawn twice (both triangles claim it) or not at all (both skip it),
// depending on rounding -- top-left rule guarantees exactly one of the
// two triangles sharing that edge claims it.
bool isInside(float px, float py, ScreenVertex a, ScreenVertex b, ScreenVertex c) {
    float e0 = edgeFunction(a, b, px, py);
    float e1 = edgeFunction(b, c, px, py);
    float e2 = edgeFunction(c, a, px, py);

    bool inside = true;
    inside &= isTopLeft(a, b) ? (e0 >= 0) : (e0 > 0);
    inside &= isTopLeft(b, c) ? (e1 >= 0) : (e1 > 0);
    inside &= isTopLeft(c, a) ? (e2 >= 0) : (e2 > 0);

    return inside;
}

bool isInside(ScreenVertex p, ScreenVertex a, ScreenVertex b, ScreenVertex c) {
    return isInside((float)p.x, (float)p.y, a, b, c);
}

// --- Near/far clipping ------------------------------------------------

// Signed distance from the near plane in clip space: positive (kept) on
// the camera side of it. At exactly the near plane, z == -w (derivable
// from Mat4::perspective's z/w rows at view-space z = -nearZ) -- so
// "in front of the near plane" is z >= -w, i.e. z + w >= 0.
static float nearSignedDistance(const Vec4& v) { return v.z + v.w; }

// Signed distance from the far plane in clip space: positive (kept) on
// the camera side of it. By the same derivation, z == w at exactly the
// far plane, so "nearer than the far plane" is z <= w, i.e. w - z >= 0.
static float farSignedDistance(const Vec4& v) { return v.w - v.z; }

// Sutherland-Hodgman: clips a (convex, since it's always a triangle or a
// clipped triangle) polygon against a single plane, given as a signed
// distance function that's positive on the side to keep. Walks the
// polygon's edges in order; a vertex on the inside is kept as-is, and
// wherever consecutive vertices are on opposite sides, the edge is
// replaced by the interpolated point that lands exactly on the plane --
// interpolating the whole Vec4 together means the new point's w (and so
// its later perspective divide) comes out correct automatically.
static std::vector<Vec4> clipPolygonAgainstPlane(const std::vector<Vec4>& polygon, float (*signedDistance)(const Vec4&)) {
    if (polygon.empty()) return {};

    std::vector<Vec4> out;
    for (size_t i = 0; i < polygon.size(); i++) {
        const Vec4& current = polygon[i];
        const Vec4& next = polygon[(i + 1) % polygon.size()];
        float dCurrent = signedDistance(current);
        float dNext = signedDistance(next);
        bool currentInside = dCurrent >= 0.0f;
        bool nextInside = dNext >= 0.0f;

        if (currentInside) out.push_back(current);

        if (currentInside != nextInside) {
            // The edge crosses the plane -- dCurrent and dNext have
            // opposite signs here, so this never divides by zero.
            float t = dCurrent / (dCurrent - dNext);
            out.push_back(current + (next - current) * t);
        }
    }
    return out;
}

std::vector<std::array<Vec4, 3>> clipTriangleNearFar(const Vec4& a, const Vec4& b, const Vec4& c) {
    std::vector<Vec4> polygon = {a, b, c};
    polygon = clipPolygonAgainstPlane(polygon, nearSignedDistance);
    polygon = clipPolygonAgainstPlane(polygon, farSignedDistance);

    // Fan-triangulate whatever came out (0 vertices, or a triangle,
    // quad, ... up to a heptagon in the worst case) -- same approach as
    // obj_loader.cpp's n-gon faces, and for the same reason: it
    // preserves the polygon's winding order.
    std::vector<std::array<Vec4, 3>> triangles;
    for (size_t i = 1; i + 1 < polygon.size(); i++) {
        triangles.push_back({polygon[0], polygon[i], polygon[i + 1]});
    }
    return triangles;
}

void fillTriangle(Framebuffer& fb, const ScreenVertex& a, const ScreenVertex& b, const ScreenVertex& c, float intensity) {
    // render()'s viewport step flips y (NDC grows up, screen rows grow
    // down), which would normally reverse a triangle's 2D winding on its
    // way from model space to screen space. But edgeFunction's term order
    // is already the mirror image of the textbook orient2d formula, so
    // that flip and this one cancel out -- a triangle wound CCW in model
    // space (see faceNormal / cubeTriangles) still satisfies isInside as
    // (a, b, c), unswapped, once it lands in screen space.
    float area = edgeFunction(a, b, c);
    if (area == 0.0f) return; // zero-area triangle -- nothing to fill, and dividing by it below would be UB

    int minX = std::max(0, std::min({a.x, b.x, c.x}));
    int maxX = std::min(fb.width - 1, std::max({a.x, b.x, c.x}));
    int minY = std::max(0, std::min({a.y, b.y, c.y}));
    int maxY = std::min(fb.height - 1, std::max({a.y, b.y, c.y}));

    // z isn't affine in screen space (perspective divide makes it
    // non-linear), but 1/z is -- so interpolate the vertices' inverse
    // depths and flip back at the end, rather than interpolating depth
    // directly.
    float invZa = 1.0f / a.depth;
    float invZb = 1.0f / b.depth;
    float invZc = 1.0f / c.depth;

    // MSAA: a 2x2 grid of subsample offsets around each pixel's sample
    // point, symmetric so their average lands back on that point. Each
    // subsample is tested and depth-resolved independently, so a pixel
    // that's only partially covered by this triangle (its edge crosses
    // through the pixel) ends up with only some of its samples set --
    // that's what resolveMSAA() turns into a partial-coverage glyph.
    static constexpr float offsets[Framebuffer::kMSAASamples][2] = {
        {-0.25f, -0.25f}, {0.25f, -0.25f}, {-0.25f, 0.25f}, {0.25f, 0.25f}
    };

    for (int y = minY; y <= maxY; y++) {
        for (int x = minX; x <= maxX; x++) {
            for (int s = 0; s < Framebuffer::kMSAASamples; s++) {
                float sx = x + offsets[s][0];
                float sy = y + offsets[s][1];
                if (!isInside(sx, sy, a, b, c)) continue;

                // Barycentric weights: each vertex's weight is the signed
                // area of the sub-triangle formed by the sample point and
                // that vertex's opposite edge, normalized by the
                // triangle's total area.
                float w0 = edgeFunction(b, c, sx, sy) / area; // weight of a
                float w1 = edgeFunction(c, a, sx, sy) / area; // weight of b
                float w2 = edgeFunction(a, b, sx, sy) / area; // weight of c

                float invZ = w0 * invZa + w1 * invZb + w2 * invZc;
                fb.setSampleDepthTested(x, y, s, 1.0f / invZ, intensity);
            }
        }
    }
}

Vec3 faceNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
    // a, b, c counter-clockwise
    Vec3 ab = b - a;
    Vec3 ac = c - a;

    return ab.cross(ac);
}

float lambertIntensity(const Vec3& normal, const Vec3& lightDir) {
    Vec3 normalized_normal = normal.normalized();
    Vec3 normalized_lightDir = lightDir.normalized();
    
    // take the dot of the normalized vectors to clamp between [-1, 1]
    float dot = normalized_normal.dot(normalized_lightDir);
    
    // clamp to [0, 1], negative numbers dropped
    return std::max(0.0f, dot);
}

// --- Camera ------------------------------------------------------------

Vec3 Camera::forward() const { return (pos - target).normalized(); }
Vec3 Camera::right() const { return up.cross(forward()).normalized(); }

Mat4 Camera::viewMatrix() const { return Mat4::lookAt(pos, target, up); }
Mat4 Camera::projMatrix(float aspect) const { return Mat4::perspective(fovY, aspect, nearZ, farZ); }

// --- Per-frame render pipeline ------------------------------------------

void render(Framebuffer& fb, const std::vector<Vec3>& vertices,
            const std::vector<std::pair<int, int>>& edges,
            const std::vector<std::array<int, 3>>& triangles,
            const Mat4& model, const Camera& camera) {
    (void)edges; // solid path below supersedes wireframe; unused for now

    Mat4 view = camera.viewMatrix();
    // project with our camera fov and the screen aspect ratio (uncorrected --
    // char-aspect correction happens below, in the viewport step)
    Mat4 proj = camera.projMatrix((float)fb.width / fb.height);
    Mat4 mvp = proj * view * model;

    // A fixed world-space directional light, biased toward the camera
    // (which sits on +Z looking at the origin) so that whichever faces
    // survive back-face culling are generally the ones catching the
    // light too -- an all-side light would leave whatever's currently
    // facing the camera dark for a big chunk of the spin. Still offset
    // off-axis (up and to the side) so shading isn't perfectly flat.
    static const Vec3 lightDir{0.3f, 0.5f, 1.0f};

    // Transform each vertex through model -> view -> proj once per frame,
    // stopping short of the perspective divide: clipping (below, per
    // triangle) has to happen on these clip-space positions, before
    // dividing by w, not after. Also keep the model-space -> world-space
    // position around per vertex: faceNormal/back-face culling/lighting
    // all need real (pre-projection) positions, not clip space.
    std::vector<Vec3> worldPositions;
    std::vector<Vec4> clipPositions;
    worldPositions.reserve(vertices.size());
    clipPositions.reserve(vertices.size());
    for (const Vec3& v : vertices) {
        Vec4 world = model * Vec4(v, 1.0f);
        worldPositions.push_back(Vec3(world.x, world.y, world.z));
        clipPositions.push_back(mvp * Vec4(v, 1.0f));
    }

    // Perspective divide + viewport transform for one clip-space vertex,
    // shared between however many pieces a triangle gets clipped into.
    auto toScreenVertex = [&](const Vec4& clip) -> ScreenVertex {
        // Perspective divide: clip space -> normalized device coords.
        float ndcX = clip.x / clip.w;
        float ndcY = clip.y / clip.w;

        // Viewport transform: NDC [-1,1] -> pixel coords, centered on
        // the framebuffer. y is flipped (NDC grows up, framebuffer rows
        // grow down) and compressed by 0.5 to correct for terminal
        // characters being roughly twice as tall as they are wide.
        int px = int(fb.width / 2.0f + ndcX * (fb.width / 2.0f));
        int py = int(fb.height / 2.0f - ndcY * (fb.height / 2.0f) * 0.5f);

        // clip.w is the view-space depth (distance in front of the
        // camera) baked in by the perspective projection -- smaller is
        // closer, matching setDepthTested()'s convention, and its
        // reciprocal is exactly what fillTriangle interpolates for
        // perspective-correct depth. Guaranteed positive here: every
        // vertex reaching this point survived clipTriangleNearFar's near
        // test (z >= -w), which also holds w > 0 as long as nearZ > 0.
        return ScreenVertex{px, py, clip.w};
    };

    // Shade and rasterize each triangle, nearest-first ordering handled
    // by fillTriangle's z-buffer test rather than by us.
    for (const std::array<int, 3>& tri : triangles) {
        const Vec3& wa = worldPositions[tri[0]];
        const Vec3& wb = worldPositions[tri[1]];
        const Vec3& wc = worldPositions[tri[2]];

        Vec3 normal = faceNormal(wa, wb, wc);

        // Back-face cull: skip triangles whose outward normal points away
        // from the camera -- we'd only be looking at their back side.
        // Cheaper than clipping, and unaffected by it, so it goes first.
        Vec3 toCamera = camera.pos - wa;
        if (normal.dot(toCamera) <= 0.0f) continue;

        // Leave this as a raw intensity rather than converting to a char
        // here: fillTriangle's MSAA samples get resolved (and quantized to
        // a glyph) per-pixel across possibly multiple triangles, not
        // per-triangle. It's computed once per original face and reused
        // for every clipped piece below -- clipping only cuts the
        // geometry down, it doesn't change which way the face is lit.
        float intensity = lambertIntensity(normal, lightDir);

        auto clipped = clipTriangleNearFar(clipPositions[tri[0]], clipPositions[tri[1]], clipPositions[tri[2]]);
        for (const std::array<Vec4, 3>& piece : clipped) {
            fillTriangle(fb, toScreenVertex(piece[0]), toScreenVertex(piece[1]), toScreenVertex(piece[2]), intensity);
        }
    }

    // All triangles are in; blend each pixel's covered MSAA samples into
    // its final antialiased glyph.
    fb.resolveMSAA();
}
