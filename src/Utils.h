#pragma once
#include <string>
#include <algorithm>
#include <cctype>
#include <random>
#include <vector>

inline std::string ToKebabCase(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    bool lastDash = false;
    for (char ch : in) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            lastDash = false;
        } else {
            if (!lastDash && !out.empty()) { out.push_back('-'); lastDash = true; }
        }
    }
    if (!out.empty() && out.back()=='-') out.pop_back();
    return out;
}

// Simple seeded RNG wrapper for determinism across preview/export
class RNG {
public:
    explicit RNG(uint32_t seed = 123456789u) : eng(seed) {}
    void reseed(uint32_t seed) { eng.seed(seed); }
    double uniform01() { return dist(eng); }
    int randint(int a, int b) { // inclusive a..b
        std::uniform_int_distribution<int> d(a,b);
        return d(eng);
    }
private:
    std::mt19937 eng;
    std::uniform_real_distribution<double> dist{0.0,1.0};
};

struct SizeI { int w{0}; int h{0}; };
