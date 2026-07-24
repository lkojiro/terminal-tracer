#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <cstdio>

#include "vec3.hpp"
#include "mat4.hpp"

// ---------------------------------------------------------------
// Terminal framebuffer: just a grid of chars we print each frame.
// This part is plumbing, not graphics theory — implemented for you
// so you can focus on the pipeline below.
// ---------------------------------------------------------------
struct Framebuffer {
    int width, height;
    std::vector<char> chars;

    Framebuffer(int w, int h) : width(w), height(h), chars(w * h, ' ') {}

    void clear() {
        std::fill(chars.begin(), chars.end(), ' ');
    }

    void set(int x, int y, char c) {
        if (x < 0 || x >= width || y < 0 || y >= height) return;
        chars[y * width + x] = c;
    }

    void present() const {
        // Move cursor to top-left instead of clearing the whole
        // terminal each frame — clearing causes visible flicker.
        std::string out = "\x1b[H";
        out.reserve(chars.size() + height + 16);
        for (int y = 0; y < height; y++) {
            out.append(&chars[y * width], width);
            out.push_back('\n');
        }
        std::fwrite(out.data(), 1, out.size(), stdout);
        std::fflush(stdout);
    }
};

// ---------------------------------------------------------------
// TODO(you): Bresenham (or similar) line drawing between two
// screen-space points. This is the "rasterization" part of a
// rasterizer — given two integer pixel coordinates, decide which
// pixels in between get lit up. Bresenham's algorithm is the
// classic approach; worth understanding why it avoids floating
// point division per-pixel.
// ---------------------------------------------------------------
void drawLine(Framebuffer& fb, int x0, int y0, int x1, int y1, char c) {
    (void)fb; (void)x0; (void)y0; (void)x1; (void)y1; (void)c;
    // replace with your line-drawing implementation
}

// A unit cube, defined by its 8 corners. This is just data — no
// need to reinvent it, the geometry itself isn't the learning goal.
static const std::vector<Vec3> cubeVertices = {
    {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
    {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1},
};

// Each pair of indices is one edge of the cube, for wireframe drawing.
static const std::vector<std::pair<int, int>> cubeEdges = {
    {0,1}, {1,2}, {2,3}, {3,0}, // back face
    {4,5}, {5,6}, {6,7}, {7,4}, // front face
    {0,4}, {1,5}, {2,6}, {3,7}, // connecting edges
};

int main() {
    const int screenWidth = 80;
    const int screenHeight = 40;
    Framebuffer fb(screenWidth, screenHeight);

    // Hide cursor for a cleaner animation; restore it on exit.
    std::fwrite("\x1b[?25l\x1b[2J", 1, 9, stdout);

    float angle = 0.0f;

    for (int frame = 0; frame < 300; frame++) {
        fb.clear();

        // -----------------------------------------------------
        // TODO(you): the actual MVP pipeline. Roughly, per vertex:
        //
        //   1. Build a model matrix (Mat4::rotateY(angle) once you've
        //      implemented it) to spin the cube.
        //   2. Build a view matrix (Mat4::lookAt) placing a "camera"
        //      a few units back from the origin, looking at the cube.
        //   3. Build a projection matrix (Mat4::perspective).
        //   4. For each vertex: transform by model, then view, then
        //      projection, giving a clip-space Vec4.
        //   5. Perspective divide: divide x, y, z by w to get
        //      normalized device coordinates in [-1, 1].
        //   6. Viewport transform: map NDC x/y into pixel coordinates
        //      (remember the framebuffer's y grows downward, and
        //      terminal characters are roughly twice as tall as they
        //      are wide — you'll likely want to scale x and y
        //      differently to avoid a squashed-looking cube).
        //   7. For each edge in cubeEdges, call drawLine() between
        //      the two transformed screen-space points.
        //
        // For now this loop does nothing, so you'll just see a
        // blank screen until you fill it in.
        // -----------------------------------------------------
        (void)cubeVertices;
        (void)cubeEdges;
        (void)angle;

        fb.present();
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
        angle += 0.03f;
    }

    // Restore cursor.
    std::fwrite("\x1b[?25h", 1, 6, stdout);
    return 0;
}
