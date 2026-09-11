#pragma once
#include <glad/gl.h>

class ProxyCube {
public:
    void init();
    void draw() const;
    void destroy();

private:
    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
};
