#pragma once
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include "Shader.h"

class SlicePlane {
public:
    bool enabled = false;
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 normal{0.0f, 0.0f, 1.0f};

    // World-to-plane matrix (for shader clipping)
    glm::mat4 matrix() const;

    // Plane-local-to-world matrix (for rendering the visible quad)
    glm::mat4 localToWorld(float scale = 1.0f) const;

    // Visible quad rendering
    void init(const std::string& shaderDir);
    void draw(const glm::mat4& view, const glm::mat4& proj);
    void destroy();

private:
    GLuint m_vao = 0, m_vbo = 0;
    Shader m_shader;

    // Compute basis vectors from normal
    void buildBasis(glm::vec3& right, glm::vec3& up, glm::vec3& n) const;
};
