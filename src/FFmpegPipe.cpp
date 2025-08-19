#include "FFmpegPipe.h"
#include <sstream>
#include <iostream>
#ifdef _WIN32
  #include <io.h>
  #define popen _popen
  #define pclose _pclose
#else
  #include <unistd.h>
#endif

bool FFmpegPipe::Open(const std::string& outPath, int w, int h, int fps) {
    if (m_pipe) return false;

    // Command builds a VP9 WebM (libvpx-vp9) with alpha channel (yuva420p)
    // Reads raw BGRA frames from stdin
    std::ostringstream cmd;
    cmd << "ffmpeg -hide_banner -loglevel error -y "
        << "-f rawvideo -pix_fmt bgra "
        << "-s " << w << "x" << h << " "
        << "-r " << fps << " -i - "
        << "-an -c:v libvpx-vp9 -pix_fmt yuva420p "
        << "-b:v 0 -crf 28 -deadline good -row-mt 1 -tile-columns 1 -tile-rows 1 -auto-alt-ref 0 "
        << '"' << outPath << '"';

    m_cmd = cmd.str();
    m_pipe = popen(m_cmd.c_str(), "wb");
    return m_pipe != nullptr;
}

bool FFmpegPipe::WriteFrame(const void* data, size_t bytes) {
    if (!m_pipe) return false;
    size_t wrote = fwrite(data, 1, bytes, m_pipe);
    return wrote == bytes;
}

void FFmpegPipe::Close() {
    if (m_pipe) {
        fflush(m_pipe);
        pclose(m_pipe);
        m_pipe = nullptr;
    }
}
