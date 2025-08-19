#pragma once
#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include "Utils.h"

struct EffectContext {
    int width{0};
    int height{0};
    int duration{12}; // seconds
    int fps{30};
    float speed{1.0f}; // speed multiplier for motion/time
    int totalFrames() const { return duration * fps; }
    int density{50}; // generic slider 1..100
};

class Effect {
public:
    virtual ~Effect() = default;
    virtual void setup(const EffectContext& ctx) = 0;
    virtual void drawBGRA(std::vector<uint8_t>& buf, int frame, const EffectContext& ctx) = 0;
};

class Renderer {
public:
    Renderer(int w, int h);
    void SetEffect(const std::string& name);
    void SetDuration(int sec);
    void SetFPS(int fps);
    void SetDensity(int density);
    void SetSpeed(float s);

    void Setup();
    void RenderNextFrame();

    const std::vector<uint8_t>& GetFrameBuffer() const { return m_bgra; }
    int GetWidth() const { return m_ctx.width; }
    int GetHeight() const { return m_ctx.height; }
    int GetFPS() const { return m_ctx.fps; }
    int GetDuration() const { return m_ctx.duration; }
    const std::string& GetEffectName() const { return m_effectName; }
    float GetSpeed() const { return m_ctx.speed; }

private:
    void ClearBuffer();

    EffectContext m_ctx;
    std::string m_effectName{"golden-lights"};
    std::unique_ptr<Effect> m_effect;
    std::vector<uint8_t> m_bgra; // size w*h*4
    int m_frame{0};
};
