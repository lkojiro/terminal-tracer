#pragma once
#include <string>

// Runs the YouTube-to-ASCII video player: resolves `youtubeUrl` to a
// direct stream (via yt-dlp), then decodes and renders its video track
// as ASCII frames (VideoSource + drawVideoFrame(), reusing render.hpp's
// Framebuffer/shadeChar) in sync with its audio track played back by
// AudioPlayer. Blocks until the clip ends or the user quits (spacebar).
// Returns a process exit code (0 on normal completion, nonzero if the
// URL couldn't be resolved to a playable stream).
int runVideoPlayer(const std::string& youtubeUrl);
