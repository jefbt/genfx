#include "FFmpegPipe.h"
#include <sstream>
#include <iostream>
#ifdef _WIN32
  #include <Windows.h>
  #include <io.h>
  #include <fcntl.h>
#else
  #include <unistd.h>
#endif

bool FFmpegPipe::Open(const std::string& outPath, int w, int h, int fps, FFCodec codec) {
    if (m_pipe) return false;

    std::ostringstream cmd;
    cmd << "ffmpeg -hide_banner -loglevel info -report -y "
        << "-f image2pipe -c:v png "
        << "-r " << fps << " -i - ";

    if (codec == FFCodec::VP8_DualStream) {
        // VP8 dual-stream WebM alpha: color in v:0 (yuv420p), alpha in v:1 (gray)
        cmd << "-filter_complex "
            << "\"[0:v]split=2[c][a];[c]format=yuv420p[color];[a]alphaextract,format=gray[alpha]\" "
            << "-map [color] -map [alpha] "
            << "-c:v:0 libvpx -pix_fmt:v:0 yuv420p -b:v:0 0 -crf:v:0 22 -g 60 -deadline good -cpu-used 4 -auto-alt-ref 0 "
            << "-c:v:1 libvpx -pix_fmt:v:1 yuv420p -b:v:1 0 -crf:v:1 22 -g 60 -deadline good -cpu-used 4 -auto-alt-ref 0 "
            << "-metadata:s:v:0 alpha_mode=1 -metadata:s:v:1 alpha_mode=1 ";
    } else { // VP9 single-stream with yuva420p
        // Ensure alpha is preserved end-to-end:
        // 1) Force decoder output to RGBA (keeps alpha from PNGs)
        // 2) Convert to yuva420p for libvpx-vp9 alpha encoding
        // 3) Enable alt-ref and lag which vp9 alpha relies upon
        cmd << "-an -vf format=rgba,format=yuva420p "
            << "-c:v libvpx-vp9 -pix_fmt yuva420p "
            << "-b:v 0 -crf 22 -g 60 -deadline good -cpu-used 4 -auto-alt-ref 1 -lag-in-frames 25 "
            << "-metadata:s:v:0 alpha_mode=1 ";
    }

    cmd << '"' << outPath << '"';

    m_cmd = cmd.str();
    // DEBUG: Print command for troubleshooting
    std::cout << "FFmpeg command: " << m_cmd << std::endl;
#ifdef _WIN32
    // Create an anonymous pipe for child's STDIN
    SECURITY_ATTRIBUTES sa{}; sa.nLength = sizeof(sa); sa.bInheritHandle = TRUE; sa.lpSecurityDescriptor = nullptr;
    HANDLE hChildStdinRd = nullptr, hChildStdinWr = nullptr;
    if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &sa, 0)) {
        return false;
    }
    // Ensure the write handle is not inherited by the child
    if (!SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hChildStdinRd); CloseHandle(hChildStdinWr); return false;
    }

    // Open NUL for child's stdout/stderr to keep window and outputs hidden
    HANDLE hNull = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hNull == INVALID_HANDLE_VALUE) {
        CloseHandle(hChildStdinRd); CloseHandle(hChildStdinWr); return false;
    }

    STARTUPINFOA si{}; si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = hChildStdinRd;
    si.hStdOutput = hNull;
    si.hStdError = hNull;

    PROCESS_INFORMATION pi{};
    // CreateProcess requires a mutable command line buffer
    std::string cmdline = m_cmd;
    std::vector<char> cl(cmdline.begin(), cmdline.end());
    cl.push_back('\0');

    BOOL ok = CreateProcessA(
        nullptr,              // application name (search in PATH)
        cl.data(),            // command line
        nullptr,              // process security
        nullptr,              // thread security
        TRUE,                 // inherit handles (stdin/out/err)
        CREATE_NO_WINDOW,     // creation flags
        nullptr,              // environment
        nullptr,              // current directory
        &si,                  // startup info
        &pi                   // process info
    );

    // Parent no longer needs these
    CloseHandle(hChildStdinRd);
    CloseHandle(hNull);

    if (!ok) {
        CloseHandle(hChildStdinWr);
        return false;
    }

    // Wrap the write end into a FILE* for fwrite-based streaming
    int fd = _open_osfhandle((intptr_t)hChildStdinWr, _O_WRONLY | _O_BINARY);
    if (fd == -1) {
        // On failure, terminate process and cleanup
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        CloseHandle(hChildStdinWr);
        return false;
    }
    m_pipe = _fdopen(fd, "wb");
    if (!m_pipe) {
        TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        _close(fd); // closes underlying handle too
        return false;
    }

    // Save handles for clean shutdown
    m_hProcess = pi.hProcess;
    m_hThread = pi.hThread;
    m_childStdinWr = (void*)hChildStdinWr;
    m_childStdinRd = nullptr; // already closed on parent side
    return true;
#else
    m_pipe = popen(m_cmd.c_str(), "wb");
    return m_pipe != nullptr;
#endif
}

bool FFmpegPipe::WriteFrame(const void* data, size_t bytes) {
    if (!m_pipe) return false;
    size_t wrote = fwrite(data, 1, bytes, m_pipe);
    return wrote == bytes;
}

void FFmpegPipe::Close() {
    if (m_pipe) {
        fflush(m_pipe);
        // Close according to how it was opened on each platform
#ifdef _WIN32
        // Closing the FILE* will close the underlying pipe handle
        fclose(m_pipe);
#else
        pclose(m_pipe);
#endif
        m_pipe = nullptr;
    }
#ifdef _WIN32
    if (m_hThread) { CloseHandle((HANDLE)m_hThread); m_hThread = nullptr; }
    if (m_hProcess) {
        // Ensure process is finished; ffmpeg will exit when stdin closes
        WaitForSingleObject((HANDLE)m_hProcess, INFINITE);
        CloseHandle((HANDLE)m_hProcess);
        m_hProcess = nullptr;
    }
    m_childStdinWr = nullptr;
    m_childStdinRd = nullptr;
#else
    // nothing extra on POSIX; popen/pclose manage the child
#endif
}
