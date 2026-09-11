#pragma once
#include <glad/gl.h>
#include <string>
#include <vector>

class VolumeData {
public:
    bool loadRAW(const std::string& rawPath, int dimX, int dimY, int dimZ,
                 int bitsPerVoxel = 8, int headerSkip = 0);
    bool loadINI(const std::string& iniPath);

    void bind(int densityUnit, int gradientUnit) const;
    void destroy();

    int dimX() const { return m_dimX; }
    int dimY() const { return m_dimY; }
    int dimZ() const { return m_dimZ; }
    bool loaded() const { return m_densityTex != 0; }

    void generateProceduralSphere(int dim = 128);

private:
    GLuint m_densityTex  = 0;
    GLuint m_gradientTex = 0;
    int m_dimX = 0, m_dimY = 0, m_dimZ = 0;
    std::vector<float> m_data;

    void uploadDensity();
    void computeAndUploadGradients();
    float sample(int x, int y, int z) const;
};
