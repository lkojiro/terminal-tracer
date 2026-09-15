#pragma once
#include "render.hpp"
#include "video_source.hpp"

// Draws one decoded video frame into `fb`, reusing render.hpp's ASCII
// density ramp (shadeChar()) the same way the mesh path's MSAA resolve
// does -- just fed from a source pixel's luminance instead of a
// triangle's coverage/lighting.
//
// `frame` is expected to be fb.width x (fb.height*2) RGB24 -- i.e.
// VideoSource constructed with sampleWidth=fb.width,
// sampleHeight=fb.height*2. This box-filters each vertical pair of
// source rows into one terminal row, the video-frame equivalent of
// resolveMSAA()'s sample averaging, and for the same reason: correcting
// for terminal characters being roughly twice as tall as they are wide.
void drawVideoFrame(Framebuffer& fb, const VideoFrame& frame);
