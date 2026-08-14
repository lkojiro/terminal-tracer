#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <utility>
#include <numbers>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>

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
            if (y != height - 1) out.push_back('\n'); // no trailing '\n' -- would scroll the last row off
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

// bresenham's algorithm
//
// Integer-only variant (Zingl's formulation): a single error term
// tracks how far the ideal line has drifted from the pixel grid, and
// gets nudged by dx/dy each step. Handles all octants uniformly (no
// separate steep/shallow cases) by working in the +x/+y-normalized
// step directions sx/sy.
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
// Full coverage always renders as the caller-supplied character `c`.
static char shadeChar(char c, float coverage) {
    static const char ramp[] = {' ', '.', ':', '-', '=', '+', '*'};
    constexpr int rampMax = int(sizeof(ramp) / sizeof(ramp[0])) - 1;
    if (coverage <= 0.0f) return ' ';
    if (coverage >= 1.0f) return c;
    int idx = std::clamp(int(coverage * (rampMax + 1)), 1, rampMax);
    return ramp[idx];
}

// xiaolin wu's algorithm
//
// Same idea as Bresenham but instead of picking one pixel per step,
// it splits the line's true (fractional) position between the two
// pixels straddling it, weighted by how close the line passes to
// each. That fractional "coverage" is what gives antialiasing —
// here it's quantized into shadeChar()'s ASCII ramp since a terminal
// cell can't be partially lit.
void drawLineWu(Framebuffer& fb, int x0, int y0, int x1, int y1, char c) {
    auto ipart  = [](float x) { return std::floor(x); };
    auto fpart  = [](float x) { return x - std::floor(x); };
    auto rfpart = [&](float x) { return 1.0f - fpart(x); };
    auto plot   = [&](int x, int y, float brightness) {
        fb.set(x, y, shadeChar(c, brightness));
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

// ---------------------------------------------------------------
// Raw-mode keyboard input: lets us poll for a keypress once per frame
// without blocking on Enter. This is plumbing, not graphics theory —
// implemented for you.
//
// By default stdin is line-buffered (ICANON) and echoes what you type
// (ECHO) -- great for a shell, bad for a render loop, since std::cin
// would block waiting for you to hit Enter. Putting the terminal into
// raw mode disables both. Setting VMIN=0/VTIME=0 (still within
// termios, no fcntl needed) makes read() return immediately with 0
// bytes when nothing's been typed, instead of blocking -- that alone
// is enough for a polling read.
//
// Deliberately NOT using fcntl(..., O_NONBLOCK) here: that flag lives
// on the shared *open file description*, and for a real terminal,
// stdin/stdout/stderr are almost always dup()'d from the same open()
// call -- so making stdin non-blocking would silently make stdout
// non-blocking too, causing present()'s writes to randomly drop or
// truncate frames under load (which looks like a blank/corrupted
// screen with no obvious cause).
//
// RAII restores the original settings on scope exit, whenever main()
// exits, so a spacebar-triggered break still leaves the terminal
// usable afterward.
// ---------------------------------------------------------------
struct RawTerminalInput {
    termios original{};

    RawTerminalInput() {
        tcgetattr(STDIN_FILENO, &original);
        termios raw = original;
        raw.c_lflag &= ~(ICANON | ECHO);
        raw.c_cc[VMIN] = 0;  // read() returns immediately...
        raw.c_cc[VTIME] = 0; // ...even if 0 bytes are available
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }

    ~RawTerminalInput() {
        tcsetattr(STDIN_FILENO, TCSANOW, &original);
    }
};

enum class Key { None, Space, Up, Down, Left, Right };

// Non-blocking: returns Key::None if nothing is waiting. Arrow keys
// arrive as 3-byte escape sequences (ESC '[' A/B/C/D); everything
// else we care about is a single byte.
Key pollKey() {
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) <= 0) return Key::None;

    if (c == ' ') return Key::Space;

    if (c == '\x1b') {
        unsigned char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) <= 0) return Key::None;
        if (read(STDIN_FILENO, &seq[1], 1) <= 0) return Key::None;
        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return Key::Up;
                case 'B': return Key::Down;
                case 'C': return Key::Right;
                case 'D': return Key::Left;
            }
        }
    }

    return Key::None;
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
    struct winsize w;
    int screenWidth = 80;
    int screenHeight = 40;
    
    // Query the terminal size using standard output file descriptor
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        screenWidth = w.ws_col;
        screenHeight = w.ws_row;
    } else {
        std::cerr << "Failed to get terminal size." << std::endl;
    }
    
    Framebuffer fb(screenWidth, screenHeight);
    RawTerminalInput rawInput; // restores terminal settings on scope exit

    // Hide cursor for a cleaner animation; restore it on exit.
    std::fwrite("\x1b[?25l\x1b[2J", 1, 9, stdout);

    float angle = 0.0f;       // auto-spin around Y, advances every frame
    float angle_step = 0.03f;
    float arrowStep = 0.08f;  // radians added/removed per keypress
    bool spinning = false;    // let user toggle auto spin

    // Manual orientation from arrow keys, accumulated incrementally: each
    // keypress left-multiplies a small rotation (around a fixed
    // camera-relative axis) onto whatever's already here. That's the part
    // that keeps every individual tilt/spin camera-relative regardless of
    // what's already been applied -- rebuilding from independent pitch/yaw
    // angles every frame instead would nest one rotation inside the
    // other's local frame and make pitch look like roll once yaw is
    // nonzero.
    Mat4 orientation = Mat4::identity();

    // per frame matrices
    Mat4 model, view, proj;

    // camera position
    Vec3 camera_pos = Vec3(0,0, 4);
    Vec3 camera_target = Vec3(0,0,0);
    Vec3 camera_up = Vec3(0,1,0); // also used as the left/right yaw axis, so it stays camera-relative
    float camera_fovY = std::numbers::pi / 4;
    float nearZ = 1.0f;
    float farZ = 8.0f;

    // Camera's actual right axis, derived the same way lookAt() derives it
    // internally (up x forward). Used as the up/down pitch axis so it stays
    // camera-relative rather than assuming it happens to equal world X.
    Vec3 camera_forward = (camera_pos - camera_target).normalized();
    Vec3 camera_right = camera_up.cross(camera_forward).normalized();

    while (true) {
        Key key = pollKey();
        if (key == Key::Space) break; // spacebar exits cleanly

        switch (key) {
            case Key::Up:    orientation = Mat4::rotateAxis(camera_right, -arrowStep)  * orientation; break;
            case Key::Down:  orientation = Mat4::rotateAxis(camera_right, arrowStep) * orientation; break;
            case Key::Left:  orientation = Mat4::rotateAxis(camera_up, -arrowStep)    * orientation; break;
            case Key::Right: orientation = Mat4::rotateAxis(camera_up, arrowStep)     * orientation; break;
            default: break;
        }

        fb.clear();

//        fb.set(0, 0, '#');                              // top-left
//        fb.set(screenWidth-1, 0, '#');                  // top-right
//        fb.set(0, screenHeight-1, '#');                 // bottom-left
//        fb.set(screenWidth-1, screenHeight-1, '#');     // bottom-right

        // Model transform: continuous auto-spin (Y) applied first/
        // innermost, then the accumulated manual orientation from arrow
        // keys on top.
        model = orientation * Mat4::rotateY(angle);
        // look at origin from camera, facing upwards
        view = Mat4::lookAt(camera_pos, camera_target, camera_up);
        // project with our camera fov and the screen aspect ratio (uncorrected --
        // char-aspect correction happens below, in the viewport step)
        proj = Mat4::perspective(camera_fovY, (float)screenWidth / screenHeight, nearZ, farZ);

        Mat4 mvp = proj * view * model;

        // Transform each cube vertex through the full pipeline (model -> view
        // -> proj -> perspective divide -> viewport) into integer pixel
        // coordinates, once per frame.
        std::vector<std::pair<int, int>> screenPoints;
        screenPoints.reserve(cubeVertices.size());
        for (const Vec3& v : cubeVertices) {
            Vec4 clip = mvp * Vec4(v, 1.0f);

            // Perspective divide: clip space -> normalized device coords.
            float ndcX = clip.x / clip.w;
            float ndcY = clip.y / clip.w;

            // Viewport transform: NDC [-1,1] -> pixel coords, centered on
            // the framebuffer. y is flipped (NDC grows up, framebuffer rows
            // grow down) and compressed by 0.5 to correct for terminal
            // characters being roughly twice as tall as they are wide.
            int px = int(screenWidth / 2.0f + ndcX * (screenWidth / 2.0f));
            int py = int(screenHeight / 2.0f - ndcY * (screenHeight / 2.0f) * 0.5f);

            screenPoints.push_back({px, py});
        }

        // Draw each cube edge between its two transformed screen-space
        // endpoints.
        for (const auto& edge : cubeEdges) {
            const auto& [x0, y0] = screenPoints[edge.first];
            const auto& [x1, y1] = screenPoints[edge.second];
            drawLineBresenham(fb, x0, y0, x1, y1, '#');
        }

        fb.present();
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
//        angle += angle_step;
    }

    // Restore cursor.
    std::fwrite("\x1b[?25h", 1, 6, stdout);
    return 0;
}
