#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>

enum class TFPreset { HeatMap = 0, CoolWarm, Medical, Grayscale, Count };

class TransferFunction {
public:
    void init();
    void setPreset(TFPreset p);
    void setAlphaParams(float threshold, float maxAlpha);
    void bind(int unit) const;
    void destroy();

    TFPreset currentPreset() const { return m_preset; }
    float alphaThreshold() const { return m_threshold; }
    float maxAlpha() const { return m_maxAlpha; }

private:
    GLuint m_texture = 0;
    TFPreset m_preset = TFPreset::HeatMap;
    float m_threshold = 0.15f;
    float m_maxAlpha  = 0.90f;

    void rebuild();
    static constexpr int TF_SIZE = 1024;
};
