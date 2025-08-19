#include "Renderer.h"
#include <cstring>
#include <algorithm>
#include <random>
// MSVC may not define M_PI unless _USE_MATH_DEFINES is set before <cmath>.
// Use our own constant to avoid that dependency.
static constexpr float PI = 3.14159265358979323846f;

// ---------------------- Effects Implementations ----------------------

static inline void putPixelBGRA(std::vector<uint8_t>& buf, int w, int h, int x, int y,
                                uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if ((unsigned)x >= (unsigned)w || (unsigned)y >= (unsigned)h) return;
    size_t idx = (size_t(y) * w + x) * 4;
    // Alpha blend over existing (premultiplied not assumed). Simple src-over.
    uint8_t dstB = buf[idx+0];
    uint8_t dstG = buf[idx+1];
    uint8_t dstR = buf[idx+2];
    uint8_t dstA = buf[idx+3];

    float sa = a / 255.0f;
    float da = dstA / 255.0f;
    float outA = sa + da * (1.0f - sa);
    if (outA <= 0.0001f) {
        buf[idx+0]=buf[idx+1]=buf[idx+2]=0; buf[idx+3]=0; return;
    }
    auto blend = [&](uint8_t sc, uint8_t dc) -> uint8_t {
        float sr = sc / 255.0f;
        float dr = dc / 255.0f;
        float orv = (sr*sa + dr*da*(1.0f-sa)) / outA;
        return (uint8_t)std::clamp(int(orv*255.0f + 0.5f), 0, 255);
    };
    buf[idx+0] = blend(b, dstB);
    buf[idx+1] = blend(g, dstG);
    buf[idx+2] = blend(r, dstR);
    buf[idx+3] = (uint8_t)std::clamp(int(outA*255.0f + 0.5f), 0, 255);
}

static inline void fillCircleBGRA(std::vector<uint8_t>& buf, int w, int h, float cx, float cy, float radius,
                                  uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int minx = std::max(0, (int)std::floor(cx - radius));
    int maxx = std::min(w-1, (int)std::ceil(cx + radius));
    int miny = std::max(0, (int)std::floor(cy - radius));
    int maxy = std::min(h-1, (int)std::ceil(cy + radius));
    float r2 = radius*radius;
    for (int y=miny; y<=maxy; ++y) {
        for (int x=minx; x<=maxx; ++x) {
            float dx = x + 0.5f - cx;
            float dy = y + 0.5f - cy;
            if (dx*dx + dy*dy <= r2) {
                putPixelBGRA(buf, w, h, x, y, r, g, b, a);
            }
        }
    }
}

// Base Effect class helpers
class EffectBlackNoise : public Effect {
public:
    void setup(const EffectContext& ctx) override {
        m_gridSize = std::max(2, ctx.width / 80); // particle size
        int cols = (ctx.width + m_gridSize-1) / m_gridSize;
        int rows = (ctx.height + m_gridSize-1) / m_gridSize;
        m_cells.resize(size_t(cols*rows));
        RNG rng(12345u);
        int tf = ctx.totalFrames();
        for (auto& c : m_cells) {
            c.nextChange = rng.randint(10, 60);
            c.phase = rng.randint(0, tf-1);
            c.value = (rng.uniform01() > 0.5) ? 255 : 0;
        }
    }
    void drawBGRA(std::vector<uint8_t>& buf, int frame, const EffectContext& ctx) override {
        // For seamless loop, derive state deterministically from frame modulo totalFrames
        int tf = ctx.totalFrames();
        int fInt = int(frame * ctx.speed) % std::max(1, tf);
        int cols = (ctx.width + m_gridSize-1) / m_gridSize;
        int rows = (ctx.height + m_gridSize-1) / m_gridSize;
        for (int r=0; r<rows; ++r) {
            for (int c=0; c<cols; ++c) {
                size_t idx = size_t(r*cols + c);
                auto& cell = m_cells[idx];
                int f = (fInt + cell.phase) % tf;
                // Toggle value every cell.nextChange frames, deterministic
                int toggles = f / std::max(1, cell.nextChange);
                uint8_t v = (toggles % 2 == 0) ? cell.value : (cell.value?0:255);
                if (v > 0) {
                    int x = c * m_gridSize;
                    int y = r * m_gridSize;
                    for (int yy=0; yy<m_gridSize && y+yy<ctx.height; ++yy) {
                        for (int xx=0; xx<m_gridSize && x+xx<ctx.width; ++xx) {
                            putPixelBGRA(buf, ctx.width, ctx.height, x+xx, y+yy, v, v, v, v);
                        }
                    }
                }
            }
        }
        m_frame = (m_frame+1) % tf;
    }
private:
    struct Cell { int nextChange{30}; int phase{0}; uint8_t value{0}; };
    std::vector<Cell> m_cells;
    int m_gridSize{8};
    int m_frame{0};
};

class EffectGoldenLights : public Effect {
public:
    void setup(const EffectContext& ctx) override {
        m_particles.clear();
        RNG rng(98765u);
        int totalFrames = ctx.totalFrames();
        int count = std::max(10, ctx.width * ctx.height / 8000 * ctx.density / 50);
        m_particles.reserve(count);
        for (int i=0;i<count;++i) {
            int cycles = (rng.randint(1,2));
            Particle p;
            p.baseX = rng.uniform01() * ctx.width;
            p.baseY = rng.uniform01() * ctx.height;
            p.vx = ((rng.uniform01() - 0.5) * ctx.width / 2.0) / totalFrames;
            p.vy = ((rng.uniform01() - 0.5) * ctx.height / 2.0) / totalFrames;
            p.ampX = rng.uniform01() * 20.0 + 10.0;
            p.ampY = rng.uniform01() * 20.0 + 10.0;
            p.freqX = (PI * 2.0f * cycles) / totalFrames;
            p.freqY = (PI * 2.0f * cycles) / totalFrames;
            p.phaseX = rng.uniform01() * PI * 2.0f;
            p.phaseY = rng.uniform01() * PI * 2.0f;
            p.radius = rng.uniform01() * (ctx.width / 400.0f) + 1.0f;
            // golden colors palette
            struct Col{uint8_t r,g,b;};
            static Col pal[] = {{255,196,0},{255,214,10},{255,170,51},{255,236,179}};
            auto c = pal[rng.randint(0,3)];
            p.r=c.r; p.g=c.g; p.b=c.b;
            p.opacity = rng.uniform01() * 0.5 + 0.2;
            m_particles.push_back(p);
        }
        m_frame = 0;
    }
    void drawBGRA(std::vector<uint8_t>& buf, int frame, const EffectContext& ctx) override {
        float ff = frame * ctx.speed;
        for (auto& p : m_particles) {
            p.baseX += p.vx; p.baseY += p.vy;
            float ox = std::sin(ff * p.freqX + p.phaseX) * p.ampX;
            float oy = std::cos(ff * p.freqY + p.phaseY) * p.ampY;
            float x = std::fmod(p.baseX + ox, (float)ctx.width);
            float y = std::fmod(p.baseY + oy, (float)ctx.height);
            if (x < 0) x += ctx.width; if (y < 0) y += ctx.height;
            uint8_t a = (uint8_t)std::clamp(int(p.opacity * 255), 0, 255);
            fillCircleBGRA(buf, ctx.width, ctx.height, x, y, p.radius, p.r, p.g, p.b, a);
        }
        m_frame = (m_frame+1) % ctx.totalFrames();
    }
private:
    struct Particle {
        float baseX, baseY, vx, vy, ampX, ampY, freqX, freqY, phaseX, phaseY, radius, opacity;
        uint8_t r,g,b;
    };
    std::vector<Particle> m_particles;
    int m_frame{0};
};

class EffectRain : public Effect {
public:
    void setup(const EffectContext& ctx) override {
        RNG rng(24680u);
        int count = std::max(50, ctx.width * ctx.density / 4 / 10);
        m_drops.clear(); m_drops.reserve(count);
        float duration = (float)ctx.duration;
        for (int i=0;i<count;++i) {
            Drop d;
            d.x = rng.uniform01() * ctx.width;
            d.y = rng.uniform01() * ctx.height;
            d.length = rng.uniform01() * (ctx.height / 30.0f) + (ctx.height / 60.0f);
            float speed = ((rng.randint(1,2)) * ctx.height / duration) / ctx.fps;
            d.vy = speed;
            m_drops.push_back(d);
        }
    }
    void drawBGRA(std::vector<uint8_t>& buf, int, const EffectContext& ctx) override {
        // Simple line drawing using vertical segments
        for (auto& d : m_drops) {
            d.y += d.vy * ctx.speed;
            float y = std::fmod(d.y, (float)ctx.height);
            if (y < 0) y += ctx.height;
            int x = (int)std::round(d.x);
            int y0 = (int)std::round(y);
            int y1 = (int)std::round(y + d.length);
            for (int yy=y0; yy<=y1; ++yy) {
                putPixelBGRA(buf, ctx.width, ctx.height, x, yy % ctx.height, 174,194,224,128);
            }
        }
    }
private:
    struct Drop { float x,y,vy,length; };
    std::vector<Drop> m_drops;
};

class EffectSnow : public Effect {
public:
    void setup(const EffectContext& ctx) override {
        RNG rng(13579u);
        int count = std::max(50, ctx.width * ctx.height / 2400 * ctx.density / 50);
        m_particles.clear(); m_particles.reserve(count);
        float duration = (float)ctx.duration;
        for (int i=0;i<count;++i) {
            P p;
            p.x = rng.uniform01() * ctx.width; p.y = rng.uniform01() * ctx.height;
            p.vx = ((rng.uniform01() - 0.5f) * ctx.width / 4.0f) / (duration * ctx.fps);
            p.vy = ((rng.randint(1,2)) * ctx.height / duration) / (float)ctx.fps;
            p.radius = rng.uniform01() * 2.5f + 1.0f;
            p.opacity = rng.uniform01() * 0.5f + 0.3f;
            m_particles.push_back(p);
        }
    }
    void drawBGRA(std::vector<uint8_t>& buf, int, const EffectContext& ctx) override {
        for (auto& p : m_particles) {
            p.x += p.vx * ctx.speed; p.y += p.vy * ctx.speed;
            float x = std::fmod(p.x, (float)ctx.width); if (x<0) x+=ctx.width;
            float y = std::fmod(p.y, (float)ctx.height); if (y<0) y+=ctx.height;
            uint8_t a = (uint8_t)std::clamp(int(p.opacity * 255), 0, 255);
            fillCircleBGRA(buf, ctx.width, ctx.height, x, y, p.radius, 255,255,255, a);
        }
    }
private:
    struct P { float x,y,vx,vy,radius,opacity; };
    std::vector<P> m_particles;
};

class EffectFireflies : public Effect {
public:
    void setup(const EffectContext& ctx) override {
        RNG rng(112233u);
        int count = std::max(20, ctx.width * ctx.height / 16000 * ctx.density / 50);
        m_particles.clear(); m_particles.reserve(count);
        float duration = (float)ctx.duration;
        for (int i=0;i<count;++i) {
            P p;
            p.x = rng.uniform01() * ctx.width; p.y = rng.uniform01() * ctx.height;
            p.vx = ((rng.uniform01() - 0.5f) * ctx.width / 2.0f) / (duration * ctx.fps);
            p.vy = ((rng.uniform01() - 0.5f) * ctx.height / 2.0f) / (duration * ctx.fps);
            p.radius = rng.uniform01() * 2.0f + 1.0f;
            p.blinkOffset = rng.uniform01() * PI * 2.0f;
            int cycles = (rng.randint(1,2));
            p.blinkSpeed = (PI * 2.0f * cycles) / (duration * ctx.fps);
            m_particles.push_back(p);
        }
        m_frame = 0;
    }
    void drawBGRA(std::vector<uint8_t>& buf, int frame, const EffectContext& ctx) override {
        float ff = frame * ctx.speed;
        for (auto& p : m_particles) {
            p.x += p.vx * ctx.speed; p.y += p.vy * ctx.speed;
            float x = std::fmod(p.x, (float)ctx.width); if (x<0) x+=ctx.width;
            float y = std::fmod(p.y, (float)ctx.height); if (y<0) y+=ctx.height;
            float opacity = 0.5f + std::sin(p.blinkOffset + ff * p.blinkSpeed) * 0.5f;
            uint8_t a = (uint8_t)std::clamp(int(opacity * 255), 0, 255);
            fillCircleBGRA(buf, ctx.width, ctx.height, x, y, p.radius, 223,255,100, a);
        }
        m_frame = (m_frame+1) % ctx.totalFrames();
    }
private:
    struct P { float x,y,vx,vy,radius,blinkOffset,blinkSpeed; };
    std::vector<P> m_particles;
    int m_frame{0};
};

// ---------------------- Renderer ----------------------

Renderer::Renderer(int w, int h) {
    m_ctx.width = w; m_ctx.height = h; m_ctx.duration = 12; m_ctx.fps = 30; m_ctx.density = 50;
    m_bgra.resize(size_t(w*h*4), 0);
}

void Renderer::SetEffect(const std::string& name) { m_effectName = name; }
void Renderer::SetDuration(int sec) { m_ctx.duration = std::clamp(sec, 10, 20); }
void Renderer::SetFPS(int fps) { m_ctx.fps = std::clamp(fps, 1, 120); }
void Renderer::SetDensity(int density) { m_ctx.density = std::clamp(density, 1, 100); }
void Renderer::SetSpeed(float s) { m_ctx.speed = std::clamp(s, 0.1f, 5.0f); }

void Renderer::Setup() {
    if (m_effectName == "black-noise") m_effect = std::make_unique<EffectBlackNoise>();
    else if (m_effectName == "golden-lights") m_effect = std::make_unique<EffectGoldenLights>();
    else if (m_effectName == "rain") m_effect = std::make_unique<EffectRain>();
    else if (m_effectName == "snow") m_effect = std::make_unique<EffectSnow>();
    else if (m_effectName == "fireflies") m_effect = std::make_unique<EffectFireflies>();
    else m_effect = std::make_unique<EffectGoldenLights>();

    m_bgra.assign(m_bgra.size(), 0);
    m_frame = 0;
    m_effect->setup(m_ctx);
}

void Renderer::ClearBuffer() {
    std::fill(m_bgra.begin(), m_bgra.end(), 0);
}

void Renderer::RenderNextFrame() {
    if (!m_effect) return;
    ClearBuffer();
    m_effect->drawBGRA(m_bgra, m_frame, m_ctx);
    // advance time by 1 frame (frame index used with speed inside effects)
    m_frame = (m_frame + 1) % m_ctx.totalFrames();
}
