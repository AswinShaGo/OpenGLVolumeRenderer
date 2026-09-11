#include "Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>

Shader::~Shader() {
    if (m_program) glDeleteProgram(m_program);
}

std::string Shader::readFile(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::cerr << "Shader: cannot open " << path << "\n";
        return "";
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

GLuint Shader::compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::cerr << "Shader compile error:\n" << log << "\n";
        glDeleteShader(s);
        return 0;
    }
    return s;
}

bool Shader::loadFromFiles(const std::string& vertPath, const std::string& fragPath) {
    std::string vSrc = readFile(vertPath);
    std::string fSrc = readFile(fragPath);
    if (vSrc.empty() || fSrc.empty()) return false;

    GLuint vs = compile(GL_VERTEX_SHADER, vSrc.c_str());
    GLuint fs = compile(GL_FRAGMENT_SHADER, fSrc.c_str());
    if (!vs || !fs) return false;

    m_program = glCreateProgram();
    glAttachShader(m_program, vs);
    glAttachShader(m_program, fs);
    glLinkProgram(m_program);

    GLint ok;
    glGetProgramiv(m_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(m_program, sizeof(log), nullptr, log);
        std::cerr << "Shader link error:\n" << log << "\n";
        glDeleteProgram(m_program);
        m_program = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return m_program != 0;
}

void Shader::use() const { glUseProgram(m_program); }

GLint Shader::loc(const std::string& name) {
    auto it = m_uniformCache.find(name);
    if (it != m_uniformCache.end()) return it->second;
    GLint l = glGetUniformLocation(m_program, name.c_str());
    m_uniformCache[name] = l;
    return l;
}

void Shader::setInt(const std::string& n, int v) { glUniform1i(loc(n), v); }
void Shader::setFloat(const std::string& n, float v) { glUniform1f(loc(n), v); }
void Shader::setBool(const std::string& n, bool v) { glUniform1i(loc(n), (int)v); }
void Shader::setVec3(const std::string& n, const glm::vec3& v) { glUniform3fv(loc(n), 1, &v[0]); }
void Shader::setVec4(const std::string& n, const glm::vec4& v) { glUniform4fv(loc(n), 1, &v[0]); }
void Shader::setMat4(const std::string& n, const glm::mat4& v) { glUniformMatrix4fv(loc(n), 1, GL_FALSE, &v[0][0]); }
void Shader::setMat3(const std::string& n, const glm::mat3& v) { glUniformMatrix3fv(loc(n), 1, GL_FALSE, &v[0][0]); }
