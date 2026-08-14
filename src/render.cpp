#include "render.hpp"

#include <string>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <limits>

// --- Framebuffer ---------------------------------------------------

Framebuffer::Framebuffer(int w, int h)
    : width(w), height(h), chars(w * h, ' '),
      depth(w * h, std::numeric_limits<float>::infinity()) {}

void Framebuffer::clear() {
    std::fill(chars.begin(), chars.end(), ' ');
    std::fill(depth.begin(), depth.end(), std::numeric_limits<float>::infinity());
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

void fillTriangle(Framebuffer& fb, const ScreenVertex& a, const ScreenVertex& b, const ScreenVertex& c, char shade) {
    (void)fb; (void)a; (void)b; (void)c; (void)shade;
    // replace with your triangle rasterization + depth test
}

Vec3 faceNormal(const Vec3& a, const Vec3& b, const Vec3& c) {
    (void)a; (void)b; (void)c;
    return Vec3(0, 0, 0); // replace with your normal calculation
}

float lambertIntensity(const Vec3& normal, const Vec3& lightDir) {
    (void)normal; (void)lightDir;
    return 0.0f; // replace with your lighting calculation
}

// --- Camera ------------------------------------------------------------

Vec3 Camera::forward() const { return (pos - target).normalized(); }
Vec3 Camera::right() const { return up.cross(forward()).normalized(); }

Mat4 Camera::viewMatrix() const { return Mat4::lookAt(pos, target, up); }
Mat4 Camera::projMatrix(float aspect) const { return Mat4::perspective(fovY, aspect, nearZ, farZ); }

// --- Per-frame render pipeline ------------------------------------------

void render(Framebuffer& fb, const std::vector<Vec3>& vertices,
            const std::vector<std::pair<int, int>>& edges,
            const Mat4& model, const Camera& camera) {
    Mat4 view = camera.viewMatrix();
    // project with our camera fov and the screen aspect ratio (uncorrected --
    // char-aspect correction happens below, in the viewport step)
    Mat4 proj = camera.projMatrix((float)fb.width / fb.height);
    Mat4 mvp = proj * view * model;

    // Transform each vertex through the full pipeline (model -> view ->
    // proj -> perspective divide -> viewport) into integer pixel
    // coordinates, once per frame.
    std::vector<std::pair<int, int>> screenPoints;
    screenPoints.reserve(vertices.size());
    for (const Vec3& v : vertices) {
        Vec4 clip = mvp * Vec4(v, 1.0f);

        // Perspective divide: clip space -> normalized device coords.
        float ndcX = clip.x / clip.w;
        float ndcY = clip.y / clip.w;

        // Viewport transform: NDC [-1,1] -> pixel coords, centered on
        // the framebuffer. y is flipped (NDC grows up, framebuffer rows
        // grow down) and compressed by 0.5 to correct for terminal
        // characters being roughly twice as tall as they are wide.
        int px = int(fb.width / 2.0f + ndcX * (fb.width / 2.0f));
        int py = int(fb.height / 2.0f - ndcY * (fb.height / 2.0f) * 0.5f);

        screenPoints.push_back({px, py});
    }

    // Draw each edge between its two transformed screen-space endpoints.
    for (const auto& edge : edges) {
        const auto& [x0, y0] = screenPoints[edge.first];
        const auto& [x1, y1] = screenPoints[edge.second];
//        drawLineBresenham(fb, x0, y0, x1, y1, '#');
        drawLineWu(fb, x0, y0, x1, y1);
    }
}
