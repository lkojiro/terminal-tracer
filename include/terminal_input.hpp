#pragma once
#include <termios.h>

// ---------------------------------------------------------------
// Raw-mode keyboard input: lets us poll for a keypress once per frame
// without blocking on Enter. Shared by both the mesh viewer's render
// loop (main.cpp) and the video player's (video_player.cpp) -- neither
// wants std::cin blocking the frame on Enter, and both want the same
// spacebar-quits convention. See RawTerminalInput's constructor
// (terminal_input.cpp) for why raw mode, and why not fcntl(O_NONBLOCK).
// ---------------------------------------------------------------
struct RawTerminalInput {
    termios original{};

    RawTerminalInput();
    ~RawTerminalInput();
};

enum class Key {
    None, Space, Up, Down, Left, Right, ZoomIn, ZoomOut,
    DecreaseNear, IncreaseNear, DecreaseFar, IncreaseFar,
};

// Non-blocking: returns Key::None if nothing is waiting. Arrow keys
// arrive as 3-byte escape sequences (ESC '[' A/B/C/D); everything else
// we care about is a single byte.
Key pollKey();
