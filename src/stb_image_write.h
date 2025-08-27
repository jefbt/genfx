// stb_image_write - v1.16 - public domain - http://nothings.org/stb
// This is a local vendored copy for genfx. See upstream for the latest.
// Original repository: https://github.com/nothings/stb
//
// Only the PNG encoder is needed here.

#ifndef STB_IMAGE_WRITE_H
#define STB_IMAGE_WRITE_H

#ifdef __cplusplus
extern "C" {
#endif

// Define STBIWDEF to 'static' to avoid symbol export in a single TU build
#ifndef STBIWDEF
#define STBIWDEF extern
#endif

typedef void stbi_write_func(void *context, void *data, int size);

STBIWDEF int stbi_write_png_to_mem(const unsigned char *pixels, int stride_bytes, int x, int y, int n, int *out_len);

#ifdef __cplusplus
}
#endif

#endif // STB_IMAGE_WRITE_H
