#include "subprocess.hpp"

#include <cerrno>
#include <csignal>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <sys/wait.h>

pid_t spawnProcess(const std::vector<std::string>& argv, int stdinFd, int stdoutFd, int stderrFd) {
    pid_t pid = fork();
    if (pid != 0) return pid; // parent (or fork() failed: pid == -1)

    // Child from here on -- nothing below this point runs in the parent.
    if (stdinFd >= 0) dup2(stdinFd, STDIN_FILENO);
    if (stdoutFd >= 0) dup2(stdoutFd, STDOUT_FILENO);
    if (stderrFd >= 0) dup2(stderrFd, STDERR_FILENO);

    // execvp wants a raw char*[], null-terminated, not a
    // vector<string> -- build one pointing into the strings we already
    // own rather than copying them. execvp never modifies argv despite
    // the non-const signature (POSIX guarantees this; it's a historical
    // wart), so aliasing the strings' internal buffers is safe.
    std::vector<char*> cargv;
    cargv.reserve(argv.size() + 1);
    for (const std::string& s : argv) cargv.push_back(const_cast<char*>(s.c_str()));
    cargv.push_back(nullptr);

    execvp(cargv[0], cargv.data());
    // Only reached if execvp failed (e.g. the program isn't on PATH).
    _exit(127);
}

void waitForExit(pid_t pid) {
    if (pid <= 0) return;
    int status;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
}

void terminateAndWait(pid_t pid) {
    if (pid <= 0) return;
    kill(pid, SIGTERM);

    // SIGTERM is a request, not a guarantee -- ffmpeg/ffplay install
    // their own handler for it and try to wind down gracefully rather
    // than dying on the spot, which can take a while (or never happen
    // at all) if they're stuck in a slow/stalled network read at the
    // moment it arrives. A plain blocking waitpid() here would inherit
    // that same unbounded wait, which is exactly what let a killed
    // rasterizer process leave orphaned ffmpeg/ffplay children (still
    // playing audio) behind. Poll for a bounded grace period instead,
    // and if the child hasn't exited by the end of it, escalate to
    // SIGKILL -- unlike SIGTERM this can't be caught, blocked, or
    // deferred, so the final waitpid() is guaranteed to return quickly.
    constexpr auto kGracePeriod = std::chrono::milliseconds(500);
    constexpr auto kPollInterval = std::chrono::milliseconds(20);
    auto deadline = std::chrono::steady_clock::now() + kGracePeriod;
    while (std::chrono::steady_clock::now() < deadline) {
        int status;
        pid_t result = waitpid(pid, &status, WNOHANG);
        if (result == pid) return; // exited (and now reaped) on its own
        if (result < 0 && errno != EINTR) return; // e.g. ECHILD: already reaped elsewhere
        std::this_thread::sleep_for(kPollInterval);
    }

    kill(pid, SIGKILL);
    waitForExit(pid);
}
