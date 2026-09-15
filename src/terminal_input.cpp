#include "terminal_input.hpp"

#include <unistd.h>

// By default stdin is line-buffered (ICANON) and echoes what you type
// (ECHO) -- great for a shell, bad for a render loop, since std::cin
// would block waiting for you to hit Enter. Putting the terminal into
// raw mode disables both. Setting VMIN=0/VTIME=0 (still within termios,
// no fcntl needed) makes read() return immediately with 0 bytes when
// nothing's been typed, instead of blocking -- that alone is enough for
// a polling read.
//
// Deliberately NOT using fcntl(..., O_NONBLOCK) here: that flag lives on
// the shared *open file description*, and for a real terminal,
// stdin/stdout/stderr are almost always dup()'d from the same open()
// call -- so making stdin non-blocking would silently make stdout
// non-blocking too, causing present()'s writes to randomly drop or
// truncate frames under load (which looks like a blank/corrupted screen
// with no obvious cause).
//
// RAII restores the original settings on scope exit, whenever the
// caller's main() exits, so a spacebar-triggered break still leaves the
// terminal usable afterward.
RawTerminalInput::RawTerminalInput() {
    tcgetattr(STDIN_FILENO, &original);
    termios raw = original;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;  // read() returns immediately...
    raw.c_cc[VTIME] = 0; // ...even if 0 bytes are available
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

RawTerminalInput::~RawTerminalInput() {
    tcsetattr(STDIN_FILENO, TCSANOW, &original);
}

Key pollKey() {
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) <= 0) return Key::None;

    if (c == ' ') return Key::Space;
    if (c == 'z' || c == 'Z') return Key::ZoomIn;
    if (c == 'x' || c == 'X') return Key::ZoomOut;
    if (c == '[') return Key::DecreaseNear;
    if (c == ']') return Key::IncreaseNear;
    if (c == '-' || c == '_') return Key::DecreaseFar;
    if (c == '=' || c == '+') return Key::IncreaseFar;

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
