#include "audio_player.hpp"
#include "subprocess.hpp"

#include <iostream>
#include <fcntl.h>
#include <unistd.h>

namespace {

// ffmpeg's/ffplay's stderr would otherwise land straight in the
// terminal interleaved with the raw ASCII redraw, which
// overwrites/scrolls past it almost immediately -- if playback is ever
// failing, this is the only way to actually read why. Truncated fresh
// each run.
int openLogFile(const char* path) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) std::cerr << "warning: couldn't open " << path << " for logging\n";
    return fd;
}

} // namespace

AudioPlayer::AudioPlayer(const std::string& youtubeUrl) {
    // Stage 1: yt-dlp streams the audio-only track to its stdout -- see
    // VideoSource's class comment for why this goes through yt-dlp
    // rather than resolving a URL and letting ffmpeg fetch it directly.
    // "ba" alone (no "+bv") always resolves to a single format, never a
    // merge, so it's safe to stream straight to stdout via -o -.
    int ytdlpPipe[2];
    if (pipe(ytdlpPipe) != 0) {
        std::cerr << "AudioPlayer: pipe() failed\n";
        startTime_ = std::chrono::steady_clock::now();
        return;
    }
    std::vector<std::string> ytdlpArgv = {"yt-dlp", "-f", "ba/b", "-o", "-", youtubeUrl};
    int ytdlpLogFd = openLogFile("/tmp/terminal_tracer_audio_ytdlp.log");
    ytdlpPid_ = spawnProcess(ytdlpArgv, -1, ytdlpPipe[1], ytdlpLogFd);
    if (ytdlpLogFd >= 0) close(ytdlpLogFd);

    // Stage 2: ffmpeg decodes that stream to raw PCM -- fed via pipe:0,
    // not a URL, so no HTTP-specific options are needed here; yt-dlp
    // already did the actual fetching.
    int ffmpegPipe[2];
    if (pipe(ffmpegPipe) != 0) {
        std::cerr << "AudioPlayer: pipe() failed\n";
        close(ytdlpPipe[0]);
        close(ytdlpPipe[1]);
        terminateAndWait(ytdlpPid_);
        startTime_ = std::chrono::steady_clock::now();
        return;
    }
    std::vector<std::string> decodeArgv = {
        "ffmpeg", "-loglevel", "warning", // stderr goes to a log file, not the terminal -- see openLogFile()
        "-i", "pipe:0",
        "-vn", // audio only -- VideoSource decodes video separately
        "-f", "s16le", "-ar", "48000", "-ac", "2",
        "pipe:1",
    };
    // Note: -ar/-ac (used above for ffmpeg) are ffmpeg-transcode-only
    // shorthands; ffplay has no such translation layer and applies
    // unrecognized options straight to the raw-PCM demuxer's own
    // AVOptions, which are actually named sample_rate/ch_layout (see
    // `ffmpeg -h demuxer=s16le`) -- passing -ar/-ac here instead fails
    // with "Option not found" and ffplay never starts, so audio (and
    // the frame sync clock riding on it) silently never plays.
    // Stage 3: ffplay plays the raw PCM.
    std::vector<std::string> playArgv = {
        "ffplay", "-loglevel", "error", "-nodisp", "-autoexit",
        "-f", "s16le", "-sample_rate", "48000", "-ch_layout", "stereo", "-",
    };

    int ffmpegLogFd = openLogFile("/tmp/terminal_tracer_audio_ffmpeg.log");
    int ffplayLogFd = openLogFile("/tmp/terminal_tracer_ffplay.log");
    ffmpegPid_ = spawnProcess(decodeArgv, ytdlpPipe[0], ffmpegPipe[1], ffmpegLogFd);
    ffplayPid_ = spawnProcess(playArgv, ffmpegPipe[0], -1, ffplayLogFd);

    // Close every fd that was dup2'd into a child -- our own copies
    // would otherwise keep those pipes alive past that child exiting,
    // so EOF would never propagate down the chain.
    close(ytdlpPipe[0]);
    close(ytdlpPipe[1]);
    close(ffmpegPipe[0]);
    close(ffmpegPipe[1]);
    if (ffmpegLogFd >= 0) close(ffmpegLogFd);
    if (ffplayLogFd >= 0) close(ffplayLogFd);

    startTime_ = std::chrono::steady_clock::now();
}

AudioPlayer::~AudioPlayer() {
    // ffplay first (end of the pipeline): killing an upstream stage out
    // from under it would just make it exit anyway once its stdin
    // closes, but doing it in this order avoids a moment where an
    // upstream stage is dead and a downstream one is left blocked
    // reading from a now-empty pipe before finally noticing EOF. Then
    // ffmpeg, then yt-dlp (the front of the pipeline).
    terminateAndWait(ffplayPid_);
    terminateAndWait(ffmpegPid_);
    terminateAndWait(ytdlpPid_);
}

double AudioPlayer::elapsedSeconds() const {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime_).count();
}
