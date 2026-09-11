#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

class Shader {
public:
    Shader() = default;
    ~Shader();

    bool loadFromFiles(const std::string& vertPath, const std::string& fragPath);
    void use() const;
    GLuint id() const { return m_program; }

    void setInt(const std::string& name, int v);
    void setFloat(const std::string& name, float v);
    void setBool(const std::string& name, bool v);
    void setVec3(const std::string& name, const glm::vec3& v);
    void setVec4(const std::string& name, const glm::vec4& v);
    void setMat4(const std::string& name, const glm::mat4& v);
    void setMat3(const std::string& name, const glm::mat3& v);

private:
    GLuint m_program = 0;
    std::unordered_map<std::string, GLint> m_uniformCache;

    GLint loc(const std::string& name);
    static std::string readFile(const std::string& path);
    static GLuint compile(GLenum type, const char* src);
};
