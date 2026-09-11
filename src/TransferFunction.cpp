#include "TransferFunction.h"
#include <vector>
#include <algorithm>

static glm::vec3 lerpV(const glm::vec3& a, const glm::vec3& b, float t) {
    return a + (b - a) * t;
}

void TransferFunction::init() {
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_1D, m_texture);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_1D, 0);
    rebuild();
}

void TransferFunction::setPreset(TFPreset p) {
    m_preset = p;
    rebuild();
}

void TransferFunction::setAlphaParams(float threshold, float maxAlpha) {
    m_threshold = threshold;
    m_maxAlpha  = maxAlpha;
    rebuild();
}

void TransferFunction::rebuild() {
    glm::vec3 low, mid, high;
    switch (m_preset) {
        default:
        case TFPreset::HeatMap:
            low  = {0.00f, 0.00f, 0.00f};
            mid  = {1.00f, 0.30f, 0.00f};
            high = {1.00f, 1.00f, 1.00f};
            break;
        case TFPreset::CoolWarm:
            low  = {0.05f, 0.05f, 0.90f};
            mid  = {0.75f, 0.75f, 0.75f};
            high = {0.90f, 0.10f, 0.10f};
            break;
        case TFPreset::Medical:
            low  = {0.10f, 0.05f, 0.05f};
            mid  = {0.75f, 0.55f, 0.40f};
            high = {1.00f, 0.95f, 0.85f};
            break;
        case TFPreset::Grayscale:
            low  = {0.0f, 0.0f, 0.0f};
            mid  = {0.5f, 0.5f, 0.5f};
            high = {1.0f, 1.0f, 1.0f};
            break;
    }

    std::vector<float> pixels(TF_SIZE * 4);
    for (int i = 0; i < TF_SIZE; i++) {
        float t = i / (float)(TF_SIZE - 1);

        glm::vec3 col = t < 0.5f
            ? lerpV(low, mid,  t / 0.5f)
            : lerpV(mid, high, (t - 0.5f) / 0.5f);

        float alpha = t < m_threshold
            ? 0.0f
            : std::min(1.0f, m_maxAlpha * (t - m_threshold) / (1.0f - m_threshold));

        pixels[i * 4 + 0] = col.r;
        pixels[i * 4 + 1] = col.g;
        pixels[i * 4 + 2] = col.b;
        pixels[i * 4 + 3] = alpha;
    }

    glBindTexture(GL_TEXTURE_1D, m_texture);
    glTexImage1D(GL_TEXTURE_1D, 0, GL_RGBA16F, TF_SIZE, 0,
                 GL_RGBA, GL_FLOAT, pixels.data());
    glBindTexture(GL_TEXTURE_1D, 0);
}

void TransferFunction::bind(int unit) const {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_1D, m_texture);
}

void TransferFunction::destroy() {
    if (m_texture) glDeleteTextures(1, &m_texture);
    m_texture = 0;
}
