#pragma once
#include <string>
#include <cstdio>
#include <vector>

class FFmpegPipe {
public:
    FFmpegPipe() = default;
    ~FFmpegPipe() { Close(); }

    // Open ffmpeg for writing raw BGRA frames. Returns false on failure.
    bool Open(const std::string& outPath, int w, int h, int fps);
    bool WriteFrame(const void* data, size_t bytes);
    void Close();

private:
    std::string m_cmd;
    std::FILE* m_pipe{nullptr};
};
