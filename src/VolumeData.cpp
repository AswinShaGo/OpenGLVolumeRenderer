#include "VolumeData.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cstring>

float VolumeData::sample(int x, int y, int z) const {
    x = std::clamp(x, 0, m_dimX - 1);
    y = std::clamp(y, 0, m_dimY - 1);
    z = std::clamp(z, 0, m_dimZ - 1);
    return m_data[x + y * m_dimX + z * m_dimX * m_dimY];
}

bool VolumeData::loadINI(const std::string& iniPath) {
    std::ifstream f(iniPath);
    if (!f.is_open()) return false;

    int dimX = 0, dimY = 0, dimZ = 0, skip = 0, bits = 8;
    std::string rawFile;
    std::string line;

    while (std::getline(f, line)) {
        // Remove whitespace
        line.erase(std::remove(line.begin(), line.end(), ' '), line.end());
        // Skip section headers and empty lines
        if (line.empty() || line[0] == '[' || line[0] == '#') continue;

        auto sep = line.find(':');
        if (sep == std::string::npos) sep = line.find('=');
        if (sep == std::string::npos) continue;

        std::string key = line.substr(0, sep);
        std::string val = line.substr(sep + 1);

        // Case-insensitive key match
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        if (key == "dimx")        dimX = std::stoi(val);
        else if (key == "dimy")   dimY = std::stoi(val);
        else if (key == "dimz")   dimZ = std::stoi(val);
        else if (key == "skip" || key == "headersize") skip = std::stoi(val);
        else if (key == "format") {
            std::transform(val.begin(), val.end(), val.begin(), ::tolower);
            if (val == "uint16" || val == "int16") bits = 16;
            else bits = 8;
        }
        else if (key == "file")   rawFile = val;
    }

    if (dimX == 0 || dimY == 0 || dimZ == 0) {
        std::cerr << "VolumeData: invalid dimensions in " << iniPath << "\n";
        return false;
    }

    // Derive .raw path: same directory as .ini, or use File= field
    std::string dir = iniPath.substr(0, iniPath.find_last_of("/\\") + 1);
    if (rawFile.empty()) {
        // Assume .raw has same basename: strip .ini extension
        std::string base = iniPath;
        auto dotPos = base.rfind(".ini");
        if (dotPos != std::string::npos) base = base.substr(0, dotPos);
        // If base already ends in .raw, use it; otherwise append .raw
        if (base.size() < 4 || base.substr(base.size() - 4) != ".raw")
            base += ".raw";
        rawFile = base;
    } else if (rawFile.find('/') == std::string::npos && rawFile.find('\\') == std::string::npos) {
        rawFile = dir + rawFile;
    }

    return loadRAW(rawFile, dimX, dimY, dimZ, bits, skip);
}

bool VolumeData::loadRAW(const std::string& rawPath, int dimX, int dimY, int dimZ,
                          int bitsPerVoxel, int headerSkip) {
    std::ifstream f(rawPath, std::ios::binary);
    if (!f.is_open()) {
        std::cerr << "VolumeData: cannot open " << rawPath << "\n";
        return false;
    }

    m_dimX = dimX;
    m_dimY = dimY;
    m_dimZ = dimZ;
    int N = dimX * dimY * dimZ;

    // Skip header
    if (headerSkip > 0) f.seekg(headerSkip);

    m_data.resize(N);

    if (bitsPerVoxel == 8) {
        std::vector<uint8_t> raw(N);
        f.read(reinterpret_cast<char*>(raw.data()), N);
        if (!f) {
            std::cerr << "VolumeData: failed to read " << N << " bytes\n";
            return false;
        }
        // uint8 normalizes directly to [0, 1]
        for (int i = 0; i < N; i++)
            m_data[i] = raw[i] / 255.0f;
    }
    else if (bitsPerVoxel == 16) {
        std::vector<uint16_t> raw(N);
        f.read(reinterpret_cast<char*>(raw.data()), N * 2);
        if (!f) {
            std::cerr << "VolumeData: failed to read " << N * 2 << " bytes\n";
            return false;
        }
        // Find min/max for normalization
        uint16_t minV = *std::min_element(raw.begin(), raw.end());
        uint16_t maxV = *std::max_element(raw.begin(), raw.end());
        float range = (float)(maxV - minV);
        if (range < 1.0f) range = 1.0f;
        for (int i = 0; i < N; i++)
            m_data[i] = (raw[i] - minV) / range;
    }
    else {
        std::cerr << "VolumeData: unsupported bits " << bitsPerVoxel << "\n";
        return false;
    }

    std::cout << "Loaded volume " << dimX << "x" << dimY << "x" << dimZ
              << " (" << bitsPerVoxel << "-bit) from " << rawPath << "\n";

    uploadDensity();
    computeAndUploadGradients();
    return true;
}

void VolumeData::generateProceduralSphere(int dim) {
    m_dimX = m_dimY = m_dimZ = dim;
    int N = dim * dim * dim;
    m_data.resize(N);

    float center = dim * 0.5f;
    float radius = dim * 0.4f;

    for (int z = 0; z < dim; z++)
        for (int y = 0; y < dim; y++)
            for (int x = 0; x < dim; x++) {
                float dx = x - center, dy = y - center, dz = z - center;
                float dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                float density = std::max(0.0f, 1.0f - dist / radius);
                m_data[x + y * dim + z * dim * dim] = density;
            }

    std::cout << "Generated procedural sphere " << dim << "^3\n";
    uploadDensity();
    computeAndUploadGradients();
}

void VolumeData::uploadDensity() {
    if (m_densityTex) glDeleteTextures(1, &m_densityTex);

    glGenTextures(1, &m_densityTex);
    glBindTexture(GL_TEXTURE_3D, m_densityTex);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    float border[] = {0, 0, 0, 0};
    glTexParameterfv(GL_TEXTURE_3D, GL_TEXTURE_BORDER_COLOR, border);

    glTexImage3D(GL_TEXTURE_3D, 0, GL_R16F,
                 m_dimX, m_dimY, m_dimZ, 0,
                 GL_RED, GL_FLOAT, m_data.data());

    glBindTexture(GL_TEXTURE_3D, 0);
}

void VolumeData::computeAndUploadGradients() {
    int N = m_dimX * m_dimY * m_dimZ;
    std::vector<float> grad(N * 3);

    for (int z = 0; z < m_dimZ; z++)
        for (int y = 0; y < m_dimY; y++)
            for (int x = 0; x < m_dimX; x++) {
                int idx = (x + y * m_dimX + z * m_dimX * m_dimY) * 3;
                grad[idx + 0] = (sample(x+1,y,z) - sample(x-1,y,z)) * 0.5f;
                grad[idx + 1] = (sample(x,y+1,z) - sample(x,y-1,z)) * 0.5f;
                grad[idx + 2] = (sample(x,y,z+1) - sample(x,y,z-1)) * 0.5f;
            }

    if (m_gradientTex) glDeleteTextures(1, &m_gradientTex);

    glGenTextures(1, &m_gradientTex);
    glBindTexture(GL_TEXTURE_3D, m_gradientTex);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB16F,
                 m_dimX, m_dimY, m_dimZ, 0,
                 GL_RGB, GL_FLOAT, grad.data());

    glBindTexture(GL_TEXTURE_3D, 0);
    std::cout << "Computed gradients (" << N * 3 * 2 / (1024*1024) << " MB GPU)\n";
}

void VolumeData::bind(int densityUnit, int gradientUnit) const {
    glActiveTexture(GL_TEXTURE0 + densityUnit);
    glBindTexture(GL_TEXTURE_3D, m_densityTex);
    glActiveTexture(GL_TEXTURE0 + gradientUnit);
    glBindTexture(GL_TEXTURE_3D, m_gradientTex);
}

void VolumeData::destroy() {
    if (m_densityTex)  glDeleteTextures(1, &m_densityTex);
    if (m_gradientTex) glDeleteTextures(1, &m_gradientTex);
    m_densityTex = m_gradientTex = 0;
    m_data.clear();
}
