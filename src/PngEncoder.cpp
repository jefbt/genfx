#include "PngEncoder.h"
#include <wx/image.h>
#include <wx/mstream.h>
#include <algorithm>

bool EncodePNGFromBGRA(const uint8_t* bgra, int width, int height, std::vector<uint8_t>& outPng) {
    if (!bgra || width <= 0 || height <= 0) return false;

    static bool s_inited = false;
    if (!s_inited) {
        wxInitAllImageHandlers();
        s_inited = true;
    }

    // wxImage expects RGB data (3 bytes) and an optional separate alpha array
    wxImage img(width, height, /*clear=*/false);
    if (!img.IsOk()) return false;

    // Allocate alpha
    img.SetAlpha(new unsigned char[width * height]);

    unsigned char* rgb = img.GetData();
    unsigned char* alpha = img.GetAlpha();
    if (!rgb || !alpha) return false;

    // Convert BGRA -> RGB + Alpha
    const uint8_t* p = bgra;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            uint8_t B = *p++;
            uint8_t G = *p++;
            uint8_t R = *p++;
            uint8_t A = *p++;
            *rgb++ = R;
            *rgb++ = G;
            *rgb++ = B;
            *alpha++ = A;
        }
    }

    wxMemoryOutputStream memOut;
    if (!img.SaveFile(memOut, wxBITMAP_TYPE_PNG)) {
        return false;
    }

    size_t sz = memOut.GetSize();
    outPng.resize(sz);
    memOut.CopyTo(outPng.data(), sz);
    return true;
}
