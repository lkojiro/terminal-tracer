# Terminal Tracer

A software 3D rasterizer that renders directly to an ANSI terminal, using
ASCII/Unicode-density characters as pixels. No GPU, no graphics library —
just matrix math, a z-buffer, and `stdout`.

It loads a mesh (a built-in cube, or any `.obj` file given on the command
line), spins it, and lets you fly the camera around it with the keyboard,
all inside a terminal window. It can also turn any YouTube video into
ASCII, audio and all — see [Video player](#video-player) below.

## Demo

<!--
  TODO: replace this with a real recording. For the video player
  specifically, a screen capture that preserves audio is worth the
  extra step over a silent GIF:

  1. QuickTime Player -> File -> New Screen Recording -> the gear icon
     next to Record has a "Record System Audio" option -- turn it on so
     the video player's audio makes it into the recording. Capture the
     terminal window for ~10-15s of a running `--video` session.
  2. Drag the resulting .mov into a GitHub PR description, issue, or
     Discussion comment box (github.com, not a local editor) -- GitHub
     uploads it and gives you a
     `https://github.com/user-attachments/assets/...` URL.
  3. Paste that URL directly into this README on its own line -- GitHub
     renders it as an inline, playable video (with audio) right in the
     page, no extra markdown syntax needed.

  For a lighter, silent, always-visible-without-clicking preview (good
  as a second image right below the video, or as a fallback), record a
  GIF with vhs (https://github.com/charmbracelet/vhs -- scriptable, so
  it's easy to regenerate later):

      brew install vhs
      cat > demo.tape <<'EOF'
      Output demo.gif
      Set FontSize 14
      Set Width 1000
      Set Height 600
      Type "./build/rasterizer --video \"https://www.youtube.com/watch?v=...\""
      Enter
      Sleep 10s
      Space
      EOF
      vhs demo.tape

  Then embed it with: ![demo](demo.gif)
-->

## Installation

```sh
brew tap lkojiro/terminal-tracer https://github.com/lkojiro/terminal-tracer
brew install terminal-tracer

terminal-tracer                          # spinning cube
terminal-tracer --video "https://www.youtube.com/watch?v=..."
```

This pulls in `ffmpeg` and `yt-dlp` automatically (needed for `--video`
mode). No Homebrew, or want to build from source instead? See
[Building & running](#building--running).

## Features

- **Full transform pipeline**: model → view → projection, built from
  scratch on a column-major `Mat4` (translate/scale/rotate, arbitrary-axis
  rotation via Rodrigues' formula, `lookAt`, perspective projection).
- **Perspective-correct triangle rasterization** via Pineda's edge-function
  algorithm, with the top-left fill rule so shared edges aren't
  double-drawn or gapped.
- **Z-buffered solid shading**: back-face culling + Lambertian lighting per
  triangle, with per-pixel depth testing so nearer geometry correctly
  occludes farther geometry.
- **Near/far clip-space triangle clipping** (Sutherland-Hodgman), so
  geometry crossing the camera or stretching past the far plane clips
  cleanly instead of producing garbage.
- **4x MSAA antialiasing**: each pixel is supersampled and resolved into
  one of ~70 ASCII/Unicode density-ramp characters, giving smooth-looking
  edges despite the coarse terminal "pixel" grid.
- **Line-drawing algorithms** (Bresenham and Xiaolin Wu's antialiased
  variant) for wireframe rendering, alongside the solid-fill path.
- **Wavefront `.obj` loading**: parses `v`/`f` lines (including `v/vt/vn`
  references, n-gon fan-triangulation, and negative/relative indices),
  then recenters and rescales the mesh to fit the camera framing.
- **Interactive controls**: arrow keys orbit the camera, `z`/`x` zoom,
  `[`/`]` and `-`/`=` adjust the near/far clip planes, spacebar quits, and
  the model auto-spins after a couple of seconds of no input.
- **Zero-dependency unit test suite** (custom `test_framework.hpp`)
  covering the math library, rasterizer, and `.obj` loader.
- **YouTube-to-ASCII video player**: paste a URL and it's decoded frame by
  frame into the same ASCII density ramp the mesh renderer uses, with audio
  played back in sync on a separate thread. See "Video player" below.

## Building & running

```sh
cmake -B build
cmake --build build

./build/rasterizer                # built-in cube
./build/rasterizer path/to/model.obj

ctest --test-dir build --output-on-failure   # run the test suite
```

## Video player

```sh
./build/rasterizer --video "https://www.youtube.com/watch?v=..."
```

Requires `yt-dlp` and `ffmpeg`/`ffplay` on `PATH` at runtime (not linked in --
this mode just shells out to them, the same way a shell pipeline would).
For each of the video and audio tracks, `yt-dlp -f <selector> -o - <url>`
streams the selected format straight to its stdout, piped directly into an
`ffmpeg` process that decodes it -- `yt-dlp` does the actual downloading
rather than `ffmpeg` being handed a resolved CDN URL to fetch itself,
because `googlevideo.com` throttles a single continuous GET hard after an
initial burst (confirmed directly: a raw URL fetch stalls at a few KB/s,
while `yt-dlp`'s own downloader gets several MB/s on the same content).
The video-side `ffmpeg` produces raw RGB frames consumed by the ASCII
render loop (reusing `Framebuffer`/`shadeChar` from the mesh path); the
audio-side `ffmpeg` decodes to raw PCM piped into `ffplay`, which plays it
in the background. The render loop paces video frames against elapsed
wall-clock time since audio playback started, dropping frames rather than
piling up a backlog if it falls behind. Space quits, same as the mesh
viewer.

## Controls

| Key | Action |
|---|---|
| Arrow keys | Orbit the camera |
| `z` / `x` | Zoom in / out |
| `[` / `]` | Decrease / increase near clip plane |
| `-` / `=` | Decrease / increase far clip plane |
| Space | Quit |

## Layout

```
include/vec3.hpp          Vec3/Vec4
include/mat4.hpp          4x4 matrix + transform/projection builders
include/render.hpp        Framebuffer, rasterization, clipping, camera, render()
include/obj_loader.hpp    .obj parsing
include/terminal_input.hpp  raw-mode keyboard polling (shared by both modes)
include/video_player.hpp  YouTube-to-ASCII video player entry point
include/video_source.hpp  yt-dlp video streaming + ffmpeg frame decoding
include/audio_player.hpp  ffmpeg|ffplay background audio playback + clock
include/ascii_video.hpp   RGB video frame -> Framebuffer, via shadeChar()
include/frame_queue.hpp   generic bounded producer/consumer queue
include/subprocess.hpp    fork/exec process spawning helper
src/                       implementations
tests/                     unit tests (math, render, obj loader)
```

## Sample models

`teapot.obj` and `ChessKing.obj` are included as sample meshes to load
(`./build/rasterizer teapot.obj`). Sourced from the OpenGL repository.

## History

Built up incrementally, commit by commit: vector/matrix math and camera
(`lookAt`, `rotateY`) with tests → a basic rotating-cube ASCII demo →
`Camera`/`render()` extracted out of `main()` → top-left-rule triangle
fill and solid rendering → 4x MSAA antialiasing → `.obj` loading and zoom
controls → near/far clip-space triangle clipping and full interactive
controls.
