#pragma once
#include <chrono>
#include <string>
#include <sys/types.h>

// ---------------------------------------------------------------
// Plays a YouTube URL's audio track in the background and exposes how
// far into it playback has gotten, for the render loop to sync video
// frames against.
//
// Rather than decoding PCM into this process and writing it to a
// platform audio API ourselves, this chains three child processes the
// way a shell pipeline would: `yt-dlp -f <audio-only selector> -o -
// <url>` streams the selected format to its stdout, piped into `ffmpeg
// -i pipe:0 ... pipe:1` which decodes it to raw PCM, piped in turn
// (kernel-buffered, never touching our memory) into `ffplay`'s stdin,
// which does the actual playback. yt-dlp does the fetching rather than
// ffmpeg being handed a direct CDN URL -- see VideoSource's class
// comment for why (googlevideo.com throttles a single continuous GET
// hard after an initial burst; yt-dlp's own downloader avoids it).
// There's no dedicated playback thread in this process as a result --
// the OS processes carry the real-time work, and elapsedSeconds() is
// just wall clock time since they were launched.
//
// That trades clock precision for simplicity: this doesn't know
// ffplay's actual output-buffer fill level, so it can drift from true
// audio position by ffplay's startup/buffering latency (typically well
// under 100ms). Good enough to keep ASCII video frames from visibly
// drifting out of sync over a multi-minute clip; not a substitute for a
// real audio-clock API if frame-accurate lip sync ever matters here.
// ---------------------------------------------------------------
class AudioPlayer {
public:
    explicit AudioPlayer(const std::string& youtubeUrl);
    ~AudioPlayer();

    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    // Seconds of wall-clock time since playback was launched. Used as
    // the render loop's master clock (see video_player.cpp): video
    // frames are paced against this instead of their own free-running
    // timer, so the two stay in sync even if the render loop
    // momentarily falls behind.
    double elapsedSeconds() const;

private:
    pid_t ytdlpPid_ = -1;
    pid_t ffmpegPid_ = -1;
    pid_t ffplayPid_ = -1;
    std::chrono::steady_clock::time_point startTime_;
};
