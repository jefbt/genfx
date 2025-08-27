#include "Renderer.h"
#include <cstring>
#include <algorithm>
#include <random>
#include <cmath>
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

// Simple filled rectangle helper (axis-aligned)
static inline void fillRectBGRA(std::vector<uint8_t>& buf, int w, int h, int x, int y, int rw, int rh,
                                uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    int x0 = std::max(0, x);
    int y0 = std::max(0, y);
    int x1 = std::min(w, x + rw);
    int y1 = std::min(h, y + rh);
    for (int yy = y0; yy < y1; ++yy) {
        for (int xx = x0; xx < x1; ++xx) {
            putPixelBGRA(buf, w, h, xx, yy, r, g, b, a);
        }
    }
}

// Draw a thin line by sampling t along the segment. thickness in pixels (1 or 2 recommended)
static inline void drawLineBGRA(std::vector<uint8_t>& buf, int w, int h,
                                float x0, float y0, float x1, float y1,
                                uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                int thickness = 1) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = std::sqrt(dx*dx + dy*dy);
    int steps = (int)std::max(1.0f, len);
    for (int i = 0; i <= steps; ++i) {
        float t = (steps==0)?0.0f: (float)i / (float)steps;
        float x = x0 + dx * t;
        float y = y0 + dy * t;
        int ix = (int)std::round(x);
        int iy = (int)std::round(y);
        for (int oy = -thickness/2; oy <= thickness/2; ++oy) {
            for (int ox = -thickness/2; ox <= thickness/2; ++ox) {
                putPixelBGRA(buf, w, h, ix + ox, iy + oy, r, g, b, a);
            }
        }
    }
}

// Quadratic Bezier curve drawing using line segments
static inline void drawQuadBezierBGRA(std::vector<uint8_t>& buf, int w, int h,
                                      float x0, float y0, float cx, float cy, float x1, float y1,
                                      uint8_t r, uint8_t g, uint8_t b, uint8_t a,
                                      int thickness = 1, int segments = 32) {
    float px = x0, py = y0;
    for (int i=1; i<=segments; ++i) {
        float t = (float)i / (float)segments;
        float it = 1.0f - t;
        float x = it*it*x0 + 2*it*t*cx + t*t*x1;
        float y = it*it*y0 + 2*it*t*cy + t*t*y1;
        drawLineBGRA(buf, w, h, px, py, x, y, r, g, b, a, thickness);
        px = x; py = y;
    }
}

// Base Effect class helpers
// Film Dust and Scratches effect (replaces former Black Noise)
class EffectBlackNoise : public Effect {
public:
    void setup(const EffectContext& ctx) override {
        // Precompute vignette mask for performance
        m_w = ctx.width; m_h = ctx.height;
        m_vignette.resize(size_t(m_w * m_h), 0);
        // Max vignette alpha reduced for extra subtlety and scaled slightly with density
        // Previously ~[16..64]; now ~[8..32]
        int maxA = std::clamp(12 + (ctx.density * 8 / 100), 8, 32);
        float cx = (m_w - 1) * 0.5f;
        float cy = (m_h - 1) * 0.5f;
        float rx = cx;
        float ry = cy;
        for (int y=0; y<m_h; ++y) {
            for (int x=0; x<m_w; ++x) {
                float nx = (x - cx) / rx;
                float ny = (y - cy) / ry;
                float d = std::sqrt(nx*nx + ny*ny); // 0 at center, ~1 at corners
                float t = std::clamp((d - 0.6f) / 0.4f, 0.0f, 1.0f); // start darkening after 60% radius
                uint8_t a = (uint8_t)std::clamp(int(t * maxA), 0, 255);
                m_vignette[size_t(y)*m_w + x] = a;
            }
        }
        m_seedBase = 77771u; // deterministic base
    }

    void drawBGRA(std::vector<uint8_t>& buf, int frame, const EffectContext& ctx) override {
        int tf = std::max(1, ctx.totalFrames());
        int f = int(frame * ctx.speed) % tf;
        RNG rng(m_seedBase + (uint32_t)f);

        // Global flicker computation kept minimal; vignette no longer flips to brighten
        float flicker = float(rng.uniform01()*0.02 - 0.01); // smaller range
        (void)flicker;

        // Small jitter offset to all primitives
        int jx = rng.randint(-1, 1);
        int jy = rng.randint(-1, 1);

        // 1) Vignette (always slight darken; more subtle than before)
        for (int y=0; y<m_h; ++y) {
            for (int x=0; x<m_w; ++x) {
                uint8_t a = m_vignette[size_t(y)*m_w + x];
                if (a) {
                    // further attenuate vignette alpha for subtlety
                    uint8_t a2 = (uint8_t)std::clamp(int(a * 0.6f), 0, 255);
                    putPixelBGRA(buf, ctx.width, ctx.height, x, y, 0,0,0, a2);
                }
            }
        }

        // Helper lambdas for colors
        auto randGray = [&](int minV, int maxV){ return (uint8_t)rng.randint(minV, maxV); };

        // 2) Dust particles
        int dustMin = 20;
        int dustMax = 50;
        int dustCount = std::clamp((ctx.density * (dustMin + dustMax) / 200), dustMin/2, dustMax*2);
        for (int i=0; i<dustCount; ++i) {
            int px = rng.randint(0, ctx.width-1) + jx;
            int py = rng.randint(0, ctx.height-1) + jy;
            int rw = rng.randint((int)std::max(1.0f, ctx.sizeMin), (int)std::max(1.0f, ctx.sizeMax));
            int rh = rng.randint(1, std::max(1, rw)); // slightly irregular
            uint8_t v = randGray(0, 40); // black to dark gray
            uint8_t a = (uint8_t)rng.randint(178, 255); // 70%-100%
            // prefer small irregular rectangle
            fillRectBGRA(buf, ctx.width, ctx.height, px, py, rw, rh, v, v, v, a);
        }

        // 3) Scratches (thin lines/curves)
        int scratchCount = rng.randint(0, std::max(1, ctx.density/20 + 2)); // 0..~7
        for (int i=0; i<scratchCount; ++i) {
            float x0 = (float)rng.randint(0, ctx.width-1);
            float y0 = (float)rng.randint(0, ctx.height-1);
            float len = (float)rng.randint(10, 40);
            float angle = float(rng.uniform01() * 2.0 * PI);
            float x1 = x0 + std::cos(angle) * len;
            float y1 = y0 + std::sin(angle) * len;
            int thick = rng.randint(1, 2);
            uint8_t v = randGray(0, 30);
            uint8_t a = (uint8_t)rng.randint(130, 220);
            if (rng.uniform01() < 0.5) {
                // straight line
                drawLineBGRA(buf, ctx.width, ctx.height, x0 + jx, y0 + jy, x1 + jx, y1 + jy, v, v, v, a, thick);
            } else {
                // curved line via a control point near the middle
                float cxp = (x0 + x1) * 0.5f + (float)rng.randint(-10, 10);
                float cyp = (y0 + y1) * 0.5f + (float)rng.randint(-10, 10);
                drawQuadBezierBGRA(buf, ctx.width, ctx.height, x0 + jx, y0 + jy, cxp + jx, cyp + jy, x1 + jx, y1 + jy,
                                   v, v, v, a, thick, 24);
            }
        }

        // 4) Rare large debris: hairs and smudges
        double rare = rng.uniform01();
        if (rare < 0.05) {
            // Hair: long thin curve near edges
            float side = (rng.uniform01() < 0.5) ? 0.0f : (float)ctx.width;
            float x0 = side;
            float y0 = (float)rng.randint(0, ctx.height-1);
            float x1 = (float)rng.randint(0, ctx.width-1);
            float y1 = (float)rng.randint(0, ctx.height-1);
            float cxp = ((x0 + x1) * 0.5f) + (float)rng.randint(-30, 30);
            float cyp = ((y0 + y1) * 0.5f) + (float)rng.randint(-30, 30);
            uint8_t v = randGray(0, 25);
            uint8_t a = (uint8_t)rng.randint(110, 180);
            drawQuadBezierBGRA(buf, ctx.width, ctx.height, x0 + jx, y0 + jy, cxp + jx, cyp + jy, x1 + jx, y1 + jy,
                               v, v, v, a, 1, 40);
        } else if (rare < 0.08) {
            // Smudge: big faint circle
            float cxp = (float)rng.randint(0, ctx.width-1) + jx;
            float cyp = (float)rng.randint(0, ctx.height-1) + jy;
            float rad = (float)rng.randint(std::max(20, ctx.width/20), std::max(30, ctx.width/10));
            uint8_t v = randGray(20, 60);
            uint8_t a = (uint8_t)rng.randint(25, 50); // 10%-20%
            fillCircleBGRA(buf, ctx.width, ctx.height, cxp, cyp, rad, v, v, v, a);
        }

        // No internal state to keep for seamless loop
        (void)ctx;
    }

private:
    int m_w{0}, m_h{0};
    std::vector<uint8_t> m_vignette; // per-pixel alpha for vignette
    uint32_t m_seedBase{0};
};

// White Noise: rounded light artifacts, fewer scratches
class EffectWhiteNoise : public Effect {
public:
    void setup(const EffectContext& ctx) override {
        m_w = ctx.width; m_h = ctx.height;
        m_seedBase = 99123u;
    }
    void drawBGRA(std::vector<uint8_t>& buf, int frame, const EffectContext& ctx) override {
        int tf = std::max(1, ctx.totalFrames());
        int f = int(frame * ctx.speed) % tf;
        RNG rng(m_seedBase + (uint32_t)f);

        // Slight jitter for naturalism
        int jx = rng.randint(-1, 1);
        int jy = rng.randint(-1, 1);

        // Rounded dust particles: light gray to white, varying opacity
        int baseMin = 16, baseMax = 36; // fewer than black-noise
        int count = std::clamp((ctx.density * (baseMin + baseMax) / 240), baseMin/2, baseMax);
        float rmin = std::min(ctx.sizeMin, ctx.sizeMax);
        float rmax = std::max(ctx.sizeMin, ctx.sizeMax);
        rmin = std::max(1.0f, rmin);
        rmax = std::max(rmin, rmax);
        for (int i=0; i<count; ++i) {
            float cx = (float)rng.randint(0, ctx.width-1) + jx;
            float cy = (float)rng.randint(0, ctx.height-1) + jy;
            float rad = rmin + (float)rng.uniform01() * std::max(0.0f, rmax - rmin);
            uint8_t v = (uint8_t)rng.randint(200, 255); // light
            uint8_t a = (uint8_t)rng.randint(150, 230);  // 60%..90%
            fillCircleBGRA(buf, ctx.width, ctx.height, cx, cy, rad, v, v, v, a);
        }

        // Very few scratches: thinner and lighter
        int scratches = rng.randint(0, std::max(1, ctx.density/30)); // fewer overall
        for (int i=0; i<scratches; ++i) {
            float x0 = (float)rng.randint(0, ctx.width-1);
            float y0 = (float)rng.randint(0, ctx.height-1);
            float len = (float)rng.randint(8, 28);
            float angle = float(rng.uniform01() * 2.0 * PI);
            float x1 = x0 + std::cos(angle) * len;
            float y1 = y0 + std::sin(angle) * len;
            int thick = 1;
            uint8_t v = (uint8_t)rng.randint(200, 245);
            uint8_t a = (uint8_t)rng.randint(110, 180);
            if (rng.uniform01() < 0.35) {
                float cxp = (x0 + x1) * 0.5f + (float)rng.randint(-8, 8);
                float cyp = (y0 + y1) * 0.5f + (float)rng.randint(-8, 8);
                drawQuadBezierBGRA(buf, ctx.width, ctx.height,
                                   x0 + jx, y0 + jy, cxp + jx, cyp + jy, x1 + jx, y1 + jy,
                                   v, v, v, a, thick, 20);
            } else {
                drawLineBGRA(buf, ctx.width, ctx.height, x0 + jx, y0 + jy, x1 + jx, y1 + jy, v, v, v, a, thick);
            }
        }
    }
private:
    int m_w{0}, m_h{0};
    uint32_t m_seedBase{0};
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
            // radius within UI-configured range
            float rmin = std::min(ctx.sizeMin, ctx.sizeMax);
            float rmax = std::max(ctx.sizeMin, ctx.sizeMax);
            p.radius = rmin + rng.uniform01() * std::max(0.0f, rmax - rmin);
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
            {
                float rmin = std::min(ctx.sizeMin, ctx.sizeMax);
                float rmax = std::max(ctx.sizeMin, ctx.sizeMax);
                p.radius = rmin + rng.uniform01() * std::max(0.0f, rmax - rmin);
            }
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
            {
                float rmin = std::min(ctx.sizeMin, ctx.sizeMax);
                float rmax = std::max(ctx.sizeMin, ctx.sizeMax);
                p.radius = rmin + rng.uniform01() * std::max(0.0f, rmax - rmin);
            }
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
void Renderer::SetSizeMin(float px) {
    m_ctx.sizeMin = std::clamp(px, 0.1f, 200.0f);
    if (m_ctx.sizeMin > m_ctx.sizeMax) m_ctx.sizeMax = m_ctx.sizeMin;
}
void Renderer::SetSizeMax(float px) {
    m_ctx.sizeMax = std::clamp(px, 0.1f, 200.0f);
    if (m_ctx.sizeMax < m_ctx.sizeMin) m_ctx.sizeMin = m_ctx.sizeMax;
}

void Renderer::Setup() {
    if (m_effectName == "black-noise") m_effect = std::make_unique<EffectBlackNoise>();
    else if (m_effectName == "white-noise") m_effect = std::make_unique<EffectWhiteNoise>();
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
