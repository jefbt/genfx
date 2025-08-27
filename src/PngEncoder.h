#pragma once
#include <vector>
#include <cstdint>

// Encode a BGRA buffer (width*height*4) into a PNG byte vector.
// Returns true on success. On success, 'outPng' is filled with the PNG file bytes.
bool EncodePNGFromBGRA(const uint8_t* bgra, int width, int height, std::vector<uint8_t>& outPng);
