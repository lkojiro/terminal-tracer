#include "video_player.hpp"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <optional>
#include <thread>

#include <sys/ioctl.h>
#include <unistd.h>

#include "ascii_video.hpp"
#include "audio_player.hpp"
#include "render.hpp"
#include "terminal_input.hpp"
#include "video_source.hpp"

namespace {

constexpr double kTargetFps = 24.0;

// SIGINT's (Ctrl+C) default action terminates the process immediately,
// without unwinding the stack -- which means VideoSource/AudioPlayer's
// destructors, and the ffmpeg/ffplay children they'd kill, never run.
// Left alone, that orphans those subprocesses: video decode just stops
// getting consumed, but audio playback (an independent OS process) has
// no reason to stop and keeps going indefinitely, unkillable from
// within this program once it's gone. Catching the signal and only
// setting a flag (the one thing async-signal-safe to do in a handler)
// lets the render loop notice it and return normally instead, so the
// same RAII cleanup a spacebar quit gets also runs here.
volatile sig_atomic_t g_quitRequested = 0;

void handleQuitSignal(int) { g_quitRequested = 1; }

bool shouldQuit() {
    return g_quitRequested || pollKey() == Key::Space;
}

// How far *behind* the audio clock a decoded frame is allowed to fall
// before we give up on presenting it and skip straight to the next one,
// rather than showing something stale. Wider than one frame interval so
// ordinary scheduling jitter doesn't cause needless drops.
constexpr double kDropThresholdSeconds = 0.1;

// Longest a single sleep_for() call is allowed to run before checking
// for a keypress again. "Frame's early, wait for the audio clock" waits
// are normally under one frame interval, but can be much longer right
// at stream start (decode races ahead while ffmpeg/ffplay are still
// spinning up) -- slicing the wait keeps a spacebar quit responsive
// through all of it instead of only between frames.
constexpr auto kPollInterval = std::chrono::milliseconds(15);

// Sleeps up to `duration`, but returns early (true) the moment spacebar
// is pressed, by polling in kPollInterval-sized slices rather than one
// long sleep_for(). Returns false if the full duration elapsed with no
// quit key seen.
bool sleepUnlessQuit(std::chrono::duration<double> duration) {
    auto remaining = duration;
    while (remaining.count() > 0) {
        auto step = std::min(remaining, std::chrono::duration<double>(kPollInterval));
        std::this_thread::sleep_for(step);
        if (shouldQuit()) return true;
        remaining -= step;
    }
    return false;
}

} // namespace

int runVideoPlayer(const std::string& youtubeUrl) {
    // Installed up front so it's armed for the whole function, including
    // while VideoSource/AudioPlayer's child processes exist below --
    // see g_quitRequested's comment for why this matters.
    std::signal(SIGINT, handleQuitSignal);
    std::signal(SIGTERM, handleQuitSignal);

    struct winsize w;
    int screenWidth = 80, screenHeight = 40;
    // ioctl can report success with a degenerate 0x0 size (some
    // multiplexer/SSH edge cases) rather than failing outright -- guard
    // for that too, not just a nonzero return, or a 0x0 Framebuffer
    // would silently render nothing at all.
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0 && w.ws_row > 0) {
        screenWidth = w.ws_col;
        screenHeight = w.ws_row;
    } else {
        std::cerr << "Failed to get terminal size.\n";
    }

    // ffmpeg/ffplay's own stderr goes to these files rather than the
    // terminal (see VideoSource/AudioPlayer) -- printed now, before raw
    // mode/the screen clear below, so it survives in scrollback instead
    // of getting overwritten by the ASCII redraw almost immediately.
    std::cerr << "(decode/playback logs: /tmp/terminal_tracer_video_ffmpeg.log, "
                 "/tmp/terminal_tracer_audio_ffmpeg.log, /tmp/terminal_tracer_ffplay.log)\n";

    Framebuffer fb(screenWidth, screenHeight);
    RawTerminalInput rawInput; // stdin only -- safe to enable before there's anything to show yet

    // Sample video at double the terminal's row count -- drawVideoFrame()
    // box-filters row pairs back down to correct for terminal characters
    // being roughly twice as tall as they are wide (see its comment).
    VideoSource video(youtubeUrl, screenWidth, screenHeight * 2, kTargetFps);

    // AudioPlayer isn't started yet -- see below. std::optional (rather
    // than a plain member) since it has to be constructed *after* the
    // wait for the first video frame, and AudioPlayer has no move/copy
    // constructor to assign into an already-existing one with.
    std::optional<AudioPlayer> audio;

    VideoFrame frame;

    // Deliberately NOT hiding the cursor / clearing the screen -- or
    // starting audio -- yet: resolving+opening the actual video stream
    // (as opposed to the yt-dlp URL lookup above) can itself take a
    // while, and doing either first would cause problems. Clearing the
    // screen first would leave the terminal sitting on a blank cleared
    // screen with nothing to say why -- indistinguishable from a genuine
    // hang. Starting audio first would be worse: audio hardware paces
    // itself in real time regardless of buffering, so by the time video
    // finally connects, the audio track would already have played
    // however many seconds that took -- a real content-level desync
    // clockOffset (below) can't retroactively fix, since it only
    // compensates for *when* things started, not for audio content that
    // has already been heard. Print a visible, updating status instead
    // until the first frame actually arrives, so a slow connection and
    // a stuck one don't look identical -- and only then start audio, as
    // close as possible to when video is about to actually appear.
    auto connectStart = std::chrono::steady_clock::now();
    auto lastStatusPrint = connectStart;
    while (true) {
        if (shouldQuit()) return 0; // fine to return directly -- cursor was never hidden, nothing to restore
        PopStatus status = video.nextFrameFor(frame, kPollInterval);
        if (status == PopStatus::Item) break;
        if (status == PopStatus::Closed) {
            std::cerr << "\nStream ended before any video frame was decoded -- "
                         "check /tmp/terminal_tracer_video_ffmpeg.log.\n";
            return 1;
        }
        auto now = std::chrono::steady_clock::now();
        if (now - lastStatusPrint >= std::chrono::milliseconds(500)) {
            double elapsed = std::chrono::duration<double>(now - connectStart).count();
            std::cerr << "\rConnecting to video stream... " << (int)elapsed << "s"
                       << (elapsed > 10.0 ? " (unusually long -- check the log above if this doesn't stop)" : "")
                       << "    " << std::flush;
            lastStatusPrint = now;
        }
    }
    std::cerr << "\n";

    // Start audio now -- video has its first frame ready to show, so
    // this is as close as it gets to both starting from the same
    // content position. AudioPlayer's own construction (forking off
    // ffmpeg/ffplay) is fast; the remaining gap is just its own
    // decode/buffering startup latency, which clockOffset below still
    // absorbs the same way it always did, just over a much smaller gap
    // now than the full video-connect delay.
    audio.emplace(youtubeUrl);

    // Hide cursor for a cleaner animation; restore it on exit.
    std::fwrite("\x1b[?25l\x1b[2J", 1, 9, stdout);

    bool haveFrame = true; // the first frame, already in hand from the wait above

    // VideoSource and AudioPlayer are two independent subprocess
    // pipelines (separate network fetches, separate ffmpeg startup) --
    // there's no reason their real-world startup latencies match, so
    // frame.pts == 0 does NOT land at audio->elapsedSeconds() == 0.
    // Comparing them directly would misread that mismatch as "video
    // already behind" from frame one and never recover (a fixed offset
    // rather than something that closes over time), dropping every
    // single frame for the rest of playback. Anchoring clockOffset to
    // whatever the audio clock reads when the *first* video frame
    // actually arrives -- rather than assuming both started at t=0
    // together -- removes that fixed bias; every later comparison goes
    // through this offset.
    bool haveClockOffset = false;
    double clockOffset = 0.0;

    while (true) {
        if (shouldQuit()) break; // spacebar, Ctrl+C, or a SIGTERM all exit cleanly

        if (!haveFrame) {
            // Timed rather than a plain blocking nextFrame(): if decode
            // has stalled (a slow/stuck network read), this still comes
            // back every kPollInterval so the shouldQuit() check above
            // keeps running instead of leaving the loop stuck
            // unresponsive until a frame -- or the stream ending --
            // eventually unblocks it on its own.
            PopStatus status = video.nextFrameFor(frame, kPollInterval);
            if (status == PopStatus::Timeout) continue;
            if (status == PopStatus::Closed) break; // stream ended
            haveFrame = true;
        }

        // Audio is the sync master (see AudioPlayer's class comment):
        // it can't be paced arbitrarily once ffplay is consuming it, so
        // video frames are the flexible side, paced to match instead of
        // free-running on their own timer.
        if (!haveClockOffset) {
            clockOffset = audio->elapsedSeconds() - frame.pts;
            haveClockOffset = true;
        }
        double clock = audio->elapsedSeconds() - clockOffset;
        if (frame.pts > clock) {
            // Ahead of the audio clock: wait rather than draw early --
            // sliced so a spacebar quit during the wait takes effect
            // right away instead of only once it elapses.
            if (sleepUnlessQuit(std::chrono::duration<double>(frame.pts - clock))) break;
        } else if (clock - frame.pts > kDropThresholdSeconds) {
            // Fell far enough behind that presenting this frame would
            // just show something stale -- drop it and check the next
            // one immediately instead of letting the backlog grow.
            haveFrame = false;
            continue;
        }

        drawVideoFrame(fb, frame);
        fb.present();
        haveFrame = false;
    }

    // Restore cursor.
    std::fwrite("\x1b[?25h", 1, 6, stdout);
    return 0;
}
