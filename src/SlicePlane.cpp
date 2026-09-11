#include "SlicePlane.h"
#include <cmath>

void SlicePlane::buildBasis(glm::vec3& right, glm::vec3& up, glm::vec3& n) const {
    n = glm::normalize(normal);
    if (glm::length(n) < 0.001f) n = glm::vec3(0, 0, 1);

    glm::vec3 ref = (std::abs(glm::dot(n, glm::vec3(0, 1, 0))) < 0.99f)
        ? glm::vec3(0, 1, 0)
        : glm::vec3(1, 0, 0);

    right = glm::normalize(glm::cross(ref, n));
    up    = glm::cross(n, right);
}

glm::mat4 SlicePlane::matrix() const {
    glm::vec3 right, up, n;
    buildBasis(right, up, n);

    glm::mat4 m(1.0f);
    m[0] = glm::vec4(right, 0);
    m[1] = glm::vec4(up,    0);
    m[2] = glm::vec4(n,     0);
    m[3] = glm::vec4(position, 1);

    return glm::inverse(m);
}

glm::mat4 SlicePlane::localToWorld(float scale) const {
    glm::vec3 right, up, n;
    buildBasis(right, up, n);

    glm::mat4 m(1.0f);
    m[0] = glm::vec4(right * scale, 0);
    m[1] = glm::vec4(up    * scale, 0);
    m[2] = glm::vec4(n,             0);
    m[3] = glm::vec4(position, 1);

    return m;
}

// Visible quad geometry 

void SlicePlane::init(const std::string& shaderDir) {
    // Unit quad in XY plane, Z=0, range [-1,1]
    float verts[] = {
        -1, -1, 0,
         1, -1, 0,
         1,  1, 0,
        -1, -1, 0,
         1,  1, 0,
        -1,  1, 0,
    };

    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindVertexArray(0);

    m_shader.loadFromFiles(shaderDir + "slice_plane.vert", shaderDir + "slice_plane.frag");
}

void SlicePlane::draw(const glm::mat4& view, const glm::mat4& proj) {
    if (!enabled || m_vao == 0) return;

    // Scale the quad to cover the volume bounding box (slightly larger than [-0.5, 0.5])
    glm::mat4 model = localToWorld(0.8f);

    m_shader.use();
    m_shader.setMat4("uModel", model);
    m_shader.setMat4("uView", view);
    m_shader.setMat4("uProjection", proj);
    m_shader.setVec4("uPlaneColor", glm::vec4(0.1f, 0.8f, 0.3f, 0.10f));
    m_shader.setVec4("uRimColor",   glm::vec4(0.3f, 1.0f, 0.4f, 0.75f));
    m_shader.setFloat("uRimWidth",  0.06f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);

    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
}

void SlicePlane::destroy() {
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
}
