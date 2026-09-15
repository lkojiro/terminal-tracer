#pragma once
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>
#include <sys/types.h>

#include "frame_queue.hpp"

// One decoded video frame: packed RGB24 (3 bytes/pixel, row-major, no
// padding) at VideoSource's fixed sampleWidth x sampleHeight, plus the
// timestamp (seconds from stream start) it should be presented at.
struct VideoFrame {
    std::vector<uint8_t> rgb;
    double pts;
};

// ---------------------------------------------------------------
// Streams a YouTube (or any yt-dlp-supported) URL's video track and
// decodes it to raw RGB24 frames, read off a background thread into a
// small bounded queue -- nextFrame()/nextFrameFor() are the consumer
// side, called from the render thread.
//
// Chains two child processes the way a shell pipeline would:
// `yt-dlp -f <video-only selector> -o - <url>` streams the selected
// format straight to its stdout, piped directly into `ffmpeg -i pipe:0
// ...` for decoding. Deliberately NOT resolving a direct CDN URL
// ourselves and handing it to ffmpeg's own HTTP fetch (an earlier
// version of this did exactly that): googlevideo.com throttles a
// single continuous GET hard after an initial burst -- confirmed
// directly, curl and a raw `ffmpeg -i <url>` both stall at a few KB/s
// after about 48KB regardless of User-Agent/Referer, while `yt-dlp`'s
// own downloader reliably gets full speed on the identical content (a
// 722KB clip: 2.4s via yt-dlp vs. projecting 100+s at the throttled
// rate). Delegating the actual fetch to yt-dlp sidesteps that
// entirely instead of reimplementing whatever request pattern it uses
// to avoid it.
//
// Callers wanting terminal-correct aspect ratio should request
// sampleHeight = 2x the terminal's row count and box-filter row pairs
// back down when drawing (see ascii_video.hpp's drawVideoFrame(), which
// does exactly this) -- terminal characters are roughly twice as tall
// as they are wide, the same correction render()'s viewport transform
// applies for the mesh path.
// ---------------------------------------------------------------
class VideoSource {
public:
    VideoSource(const std::string& youtubeUrl, int sampleWidth, int sampleHeight, double fps);
    ~VideoSource();

    VideoSource(const VideoSource&) = delete;
    VideoSource& operator=(const VideoSource&) = delete;

    // Blocks until a frame is available. Returns false once the stream
    // has ended and every already-decoded frame has been consumed.
    bool nextFrame(VideoFrame& out);

    // Like nextFrame(), but gives up after `timeout` instead of blocking
    // indefinitely (see PopStatus) -- lets a caller keep checking for a
    // quit request even while decode has stalled, rather than being
    // stuck unresponsive inside a plain blocking wait.
    template <typename Rep, typename Period>
    PopStatus nextFrameFor(VideoFrame& out, std::chrono::duration<Rep, Period> timeout) {
        return queue_.pop_for(out, timeout);
    }

    int sampleWidth() const { return sampleWidth_; }
    int sampleHeight() const { return sampleHeight_; }

private:
    void decodeLoop();

    int sampleWidth_, sampleHeight_;
    double fps_;
    pid_t ytdlpPid_ = -1;
    pid_t ffmpegPid_ = -1;
    int pipeReadFd_ = -1;
    BoundedQueue<VideoFrame> queue_{4}; // a handful of frames of slack, not the whole video
    std::thread decodeThread_;
};
