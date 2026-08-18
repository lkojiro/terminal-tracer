#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdio>
#include <utility>
#include <numbers>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#include <array>

#include "vec3.hpp"
#include "mat4.hpp"
#include "render.hpp"

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

// Each of the cube's 6 quad faces, split into 2 triangles (12 total).
// Wound counter-clockwise as viewed from *outside* the cube, so
// cross(b-a, c-a) on each triangle points outward, away from the
// cube's center -- that's what faceNormal()/back-face culling and
// lighting depend on. (Verified per-face against the expected outward
// axis, e.g. the back face at z=-1 should produce a normal of
// roughly (0,0,-1).)
static const std::vector<std::array<int, 3>> cubeTriangles = {
    {0,2,1}, {0,3,2}, // back   (z = -1)
    {4,5,6}, {4,6,7}, // front  (z = +1)
    {0,7,3}, {0,4,7}, // left   (x = -1)
    {1,2,6}, {1,6,5}, // right  (x = +1)
    {0,1,5}, {0,5,4}, // bottom (y = -1)
    {3,6,2}, {3,7,6}, // top    (y = +1)
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

    float angle_step = 0.03f; // radians of auto-spin applied per frame
    float arrowStep = 0.08f;  // radians added/removed per keypress
    bool spinning = true;

    // Manual orientation from arrow keys, accumulated incrementally: each
    // keypress left-multiplies a small rotation (around a fixed
    // camera-relative axis) onto whatever's already here. That's the part
    // that keeps every individual tilt/spin camera-relative regardless of
    // what's already been applied -- rebuilding from independent pitch/yaw
    // angles every frame instead would nest one rotation inside the
    // other's local frame and make pitch look like roll once yaw is
    // nonzero.
    Mat4 orientation = Mat4::identity();

    // Camera: eye position, look-at target, up vector, and projection
    // params. right()/forward() are derived on demand (see Camera above),
    // so they stay accurate even if pos/target ever change at runtime.
    Camera camera{
        Vec3(0, 0, 4),         // pos
        Vec3(0, 0, 0),         // target
        Vec3(0, 1, 0),         // up -- also the left/right yaw axis
        std::numbers::pi / 4,  // fovY
        1.0f, 8.0f,            // nearZ, farZ
    };

    // Idle-timeout auto-spin: any keystroke resets the clock and stops
    // auto-spin; once 3 seconds pass with no input, auto-spin kicks back in.
    const auto idleTimeout = std::chrono::seconds(2);
    auto lastInputTime = std::chrono::steady_clock::now();

    while (true) {
        Key key = pollKey();
        if (key == Key::Space) break; // spacebar exits cleanly

        if (key != Key::None) {
            lastInputTime = std::chrono::steady_clock::now();
            spinning = false;
        }

        switch (key) {
            case Key::Up:    orientation = Mat4::rotateAxis(camera.right(), -arrowStep) * orientation; break;
            case Key::Down:  orientation = Mat4::rotateAxis(camera.right(), arrowStep)  * orientation; break;
            case Key::Left:  orientation = Mat4::rotateAxis(camera.up, -arrowStep)      * orientation; break;
            case Key::Right: orientation = Mat4::rotateAxis(camera.up, arrowStep)       * orientation; break;
            default: break;
        }

        if (std::chrono::steady_clock::now() - lastInputTime >= idleTimeout) {
            spinning = true;
        }

        fb.clear();

//        fb.set(0, 0, '#');                              // top-left
//        fb.set(screenWidth-1, 0, '#');                  // top-right
//        fb.set(0, screenHeight-1, '#');                 // bottom-left
//        fb.set(screenWidth-1, screenHeight-1, '#');     // bottom-right

        render(fb, cubeVertices, cubeEdges, cubeTriangles, orientation, camera);

        fb.present();
        std::this_thread::sleep_for(std::chrono::milliseconds(33));

        // Auto-spin, applied the same way as the arrow keys: left-
        // multiplied onto `orientation` (outermost, in the camera's
        // current frame), not composed underneath it as a separate
        // term. That's what keeps it spinning around the camera's
        // actual up axis regardless of any accumulated pitch, instead
        // of getting dragged along a skewed axis.
        if (spinning) {
            orientation = Mat4::rotateAxis(camera.up, angle_step) * orientation;
        }
    }

    // Restore cursor.
    std::fwrite("\x1b[?25h", 1, 6, stdout);
    return 0;
}
