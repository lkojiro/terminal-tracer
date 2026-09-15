#pragma once
#include <string>
#include <vector>
#include <sys/types.h>

// ---------------------------------------------------------------
// Minimal POSIX process spawning: runs an external program (yt-dlp,
// ffmpeg, ffplay) as a child process, optionally wiring its stdin/stdout
// to pipe file descriptors instead of inheriting ours. This is the
// building block both VideoSource (spawns ffmpeg, reads its stdout) and
// AudioPlayer (chains ffmpeg's stdout into ffplay's stdin) are built on.
//
// Uses fork()+execvp() with an explicit argv, not popen()/system() --
// the YouTube URL a user pastes in becomes part of that argv, and
// popen()/system() would hand the whole command line to `sh -c`,
// letting shell metacharacters in a crafted "URL" run arbitrary
// commands. execvp() with an argv array never invokes a shell, so
// there's nothing for such characters to be interpreted by.
// ---------------------------------------------------------------

// Spawns argv[0] with the given arguments. If stdinFd/stdoutFd/stderrFd
// are >= 0, the child's stdin/stdout/stderr are dup2'd from them; -1
// means inherit ours unchanged. Never returns in the child (execs, or
// _exit(127) if the program couldn't be found/run); returns the child's
// pid in the parent, or -1 if fork() itself failed.
//
// Does NOT close stdinFd/stdoutFd/stderrFd for you (in either the parent
// or, past the dup2, the child) -- the caller opened the pipe/file and
// knows which ends are still needed where; close them once every
// process that needs that fd has it, so EOF actually propagates when a
// child exits.
pid_t spawnProcess(const std::vector<std::string>& argv, int stdinFd = -1, int stdoutFd = -1, int stderrFd = -1);

// Sends SIGTERM (if the process is still running) and blocks until it
// exits, reaping it so it doesn't linger as a zombie. Safe to call on a
// pid that has already exited, or on -1 (a spawnProcess() that failed).
void terminateAndWait(pid_t pid);

// Blocks until the process exits (no signal sent first), reaping it.
// Safe to call on a pid that has already exited, or on -1.
void waitForExit(pid_t pid);
