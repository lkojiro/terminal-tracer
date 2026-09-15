#include "video_source.hpp"
#include "subprocess.hpp"

#include <cerrno>
#include <iostream>
#include <fcntl.h>
#include <unistd.h>

namespace {

// ffmpeg's stderr would otherwise land straight in the terminal
// interleaved with the raw ASCII redraw, which overwrites/scrolls past
// it almost immediately -- if decode is ever failing, this is the only
// way to actually read why. Truncated fresh each run.
int openLogFile(const char* path) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) std::cerr << "warning: couldn't open " << path << " for ffmpeg's stderr\n";
    return fd;
}

// Reads exactly buf.size() bytes from fd, retrying on short reads (a
// pipe can hand back fewer bytes than requested even mid-stream, not
// just at EOF). Returns false on EOF/error before that many bytes
// arrived -- the normal way this signals "the stream just ended".
bool readExact(int fd, std::vector<uint8_t>& buf) {
    size_t total = 0;
    while (total < buf.size()) {
        ssize_t n = read(fd, buf.data() + total, buf.size() - total);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false; // EOF mid-frame -- stream ended
        total += (size_t)n;
    }
    return true;
}

} // namespace

VideoSource::VideoSource(const std::string& youtubeUrl, int sampleWidth, int sampleHeight, double fps)
    : sampleWidth_(sampleWidth), sampleHeight_(sampleHeight), fps_(fps) {
    // Stage 1: yt-dlp streams the video-only track to its stdout (see
    // this class's comment for why yt-dlp does the actual fetching
    // rather than ffmpeg being handed a direct CDN URL). "bv*" alone
    // (no "+ba") always resolves to a single format, never a merge, so
    // it's safe to stream straight to stdout via -o -. "/b" is a last
    // resort for the rare video with no video-only format at all --
    // ffmpeg's -an below still strips its audio track either way.
    int ytdlpPipe[2];
    if (pipe(ytdlpPipe) != 0) {
        std::cerr << "VideoSource: pipe() failed\n";
        queue_.close();
        return;
    }
    std::vector<std::string> ytdlpArgv = {
        "yt-dlp", "-f", "bv*[height<=240]/bv*/b", "-o", "-", youtubeUrl,
    };
    int ytdlpLogFd = openLogFile("/tmp/terminal_tracer_video_ytdlp.log");
    ytdlpPid_ = spawnProcess(ytdlpArgv, -1, ytdlpPipe[1], ytdlpLogFd);
    if (ytdlpLogFd >= 0) close(ytdlpLogFd);

    // Stage 2: ffmpeg decodes that stream -- fed via pipe:0, not a URL,
    // so no HTTP-specific options (reconnect/timeout) are needed here;
    // yt-dlp already did the actual fetching and its own retry handling.
    int ffmpegPipe[2];
    if (pipe(ffmpegPipe) != 0) {
        std::cerr << "VideoSource: pipe() failed\n";
        close(ytdlpPipe[0]);
        close(ytdlpPipe[1]);
        terminateAndWait(ytdlpPid_);
        queue_.close();
        return;
    }
    std::vector<std::string> ffmpegArgv = {
        "ffmpeg", "-loglevel", "warning", // stderr goes to a log file, not the terminal -- see openLogFile()
        "-i", "pipe:0",
        "-an", // this pipe is video-only; AudioPlayer decodes audio separately
        "-f", "rawvideo", "-pix_fmt", "rgb24",
        "-vf", "scale=" + std::to_string(sampleWidth) + ":" + std::to_string(sampleHeight),
        "-r", std::to_string(fps),
        "pipe:1",
    };
    int logFd = openLogFile("/tmp/terminal_tracer_video_ffmpeg.log");
    ffmpegPid_ = spawnProcess(ffmpegArgv, ytdlpPipe[0], ffmpegPipe[1], logFd);

    // Close every fd that was dup2'd into a child -- our own copies
    // would otherwise keep those pipes alive past that child exiting,
    // so EOF would never propagate down the chain.
    close(ytdlpPipe[0]);
    close(ytdlpPipe[1]);
    close(ffmpegPipe[1]);
    if (logFd >= 0) close(logFd);
    pipeReadFd_ = ffmpegPipe[0];

    if (ytdlpPid_ < 0 || ffmpegPid_ < 0) {
        std::cerr << "VideoSource: fork() failed\n";
        close(pipeReadFd_);
        pipeReadFd_ = -1;
        queue_.close();
        return;
    }

    decodeThread_ = std::thread(&VideoSource::decodeLoop, this);
}

VideoSource::~VideoSource() {
    // Order matters: close the queue first so a decode thread blocked
    // on a full push() wakes up and re-checks (rather than pushing) the
    // moment it's closed; kill yt-dlp next -- the front of the pipeline
    // -- so ffmpeg (fed from its stdout) sees EOF and winds down on its
    // own rather than needing a second kill signal; then ffmpeg itself,
    // in case it's stuck on something unrelated to its input closing;
    // only then is it safe to join (the thread is now guaranteed to be
    // unblockable) and finally close our own end of the pipe.
    queue_.close();
    terminateAndWait(ytdlpPid_);
    terminateAndWait(ffmpegPid_);
    if (decodeThread_.joinable()) decodeThread_.join();
    if (pipeReadFd_ >= 0) close(pipeReadFd_);
}

void VideoSource::decodeLoop() {
    if (pipeReadFd_ < 0) return; // construction failed; nothing to decode

    const size_t frameBytes = (size_t)sampleWidth_ * (size_t)sampleHeight_ * 3;
    long frameIndex = 0;
    while (true) {
        VideoFrame frame;
        frame.rgb.resize(frameBytes);
        if (!readExact(pipeReadFd_, frame.rgb)) break; // EOF: stream ended
        frame.pts = (double)frameIndex / fps_;
        frameIndex++;
        queue_.push(std::move(frame));
    }
    queue_.close();
}

bool VideoSource::nextFrame(VideoFrame& out) {
    return queue_.pop(out);
}
