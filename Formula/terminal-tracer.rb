class TerminalTracer < Formula
  desc "Software 3D rasterizer for the terminal, plus YouTube-to-ASCII video playback"
  homepage "https://github.com/lkojiro/terminal-tracer"
  url "https://github.com/lkojiro/terminal-tracer.git",
      tag:      "v0.1.0",
      revision: "REPLACE_WITH_COMMIT_SHA_AT_TAG_TIME"
  version "0.1.0"
  license "MIT"
  head "https://github.com/lkojiro/terminal-tracer.git", branch: "main"

  depends_on "cmake" => :build
  # Runtime deps for --video mode. The mesh viewer (default, no --video)
  # doesn't need either, but Homebrew formulae can't express "optional at
  # runtime" cleanly, so they're plain dependencies here.
  depends_on "ffmpeg"
  depends_on "yt-dlp"

  def install
    system "cmake", "-B", "build", *std_cmake_args
    system "cmake", "--build", "build"
    bin.install "build/rasterizer" => "terminal-tracer"
    # Sample meshes for the default (non-video) mode -- keeps
    # `terminal-tracer teapot.obj` working the same as the repo layout.
    pkgshare.install "teapot.obj", "ChessKing.obj"
  end

  def caveats
    <<~EOS
      Sample meshes were installed to:
        #{opt_pkgshare}/teapot.obj
        #{opt_pkgshare}/ChessKing.obj

      Usage:
        terminal-tracer                          # spinning cube
        terminal-tracer #{opt_pkgshare}/teapot.obj
        terminal-tracer --video "https://www.youtube.com/watch?v=..."
    EOS
  end

  test do
    # No real terminal/network in the Homebrew test sandbox, so this
    # just confirms the binary exists and links correctly rather than
    # exercising either render mode.
    assert_predicate bin/"terminal-tracer", :exist?
  end
end
