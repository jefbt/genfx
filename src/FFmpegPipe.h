#pragma once
#include <string>
#include <cstdio>
#include <vector>

enum class FFCodec { VP8_DualStream, VP9_SingleStream };

class FFmpegPipe {
public:
    FFmpegPipe() = default;
    ~FFmpegPipe() { Close(); }

    // Open ffmpeg for writing raw BGRA frames. Returns false on failure.
    bool Open(const std::string& outPath, int w, int h, int fps, FFCodec codec);
    bool WriteFrame(const void* data, size_t bytes);
    void Close();

private:
    std::string m_cmd;
    std::FILE* m_pipe{nullptr};
#ifdef _WIN32
    // Windows-specific: keep process info to ensure clean shutdown
    void* m_hProcess{nullptr}; // HANDLE
    void* m_hThread{nullptr};  // HANDLE
    void* m_childStdinWr{nullptr}; // HANDLE we write to (wrapped by m_pipe)
    void* m_childStdinRd{nullptr}; // HANDLE passed to child as STDIN
#endif
};
