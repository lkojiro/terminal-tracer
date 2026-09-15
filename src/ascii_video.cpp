#include "ascii_video.hpp"

void drawVideoFrame(Framebuffer& fb, const VideoFrame& frame) {
    const int w = fb.width;
    const int h = fb.height;
    const uint8_t* px = frame.rgb.data();

    // Rec. 601 luma weights -- perceptual brightness, not a flat RGB
    // average, so e.g. a saturated blue channel doesn't read as
    // brighter than it actually looks.
    auto luminance = [&](int sx, int sy) {
        const uint8_t* p = px + ((size_t)sy * w + (size_t)sx) * 3;
        return (0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2]) / 255.0f;
    };

    for (int y = 0; y < h; y++) {
        int srcY0 = y * 2;
        int srcY1 = srcY0 + 1;
        for (int x = 0; x < w; x++) {
            float intensity = 0.5f * (luminance(x, srcY0) + luminance(x, srcY1));
            fb.set(x, y, shadeChar(intensity));
        }
    }
}
