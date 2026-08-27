# Terminal Tracer

A software 3D rasterizer that renders directly to an ANSI terminal, using
ASCII/Unicode-density characters as pixels. No GPU, no graphics library —
just matrix math, a z-buffer, and `stdout`.

It loads a mesh (a built-in cube, or any `.obj` file given on the command
line), spins it, and lets you fly the camera around it with the keyboard,
all inside a terminal window.

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

## Building & running

```sh
cmake -B build
cmake --build build

./build/rasterizer                # built-in cube
./build/rasterizer path/to/model.obj

ctest --test-dir build --output-on-failure   # run the test suite
```

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
include/vec3.hpp        Vec3/Vec4
include/mat4.hpp         4x4 matrix + transform/projection builders
include/render.hpp       Framebuffer, rasterization, clipping, camera, render()
include/obj_loader.hpp   .obj parsing
src/                     implementations
tests/                   unit tests (math, render, obj loader)
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
