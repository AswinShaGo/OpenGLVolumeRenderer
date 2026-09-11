#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Turntable camera: orbits around a fixed origin, like a car viewer.
class Camera {
public:
    void init(float distance = 2.5f, float yaw = 0.0f, float pitch = 15.0f);

    glm::mat4 viewMatrix() const;
    glm::mat4 projMatrix(float aspect) const;
    glm::vec3 position() const;

    void orbit(float dx, float dy);   // left-drag: rotate around origin
    void zoom(float delta);            // scroll: dolly in/out

    float fov   = 45.0f;
    float nearZ = 0.01f;
    float farZ  = 100.0f;

private:
    float m_distance = 2.5f;
    float m_yaw   = 0.0f;    // horizontal angle (unlimited)
    float m_pitch = 15.0f;   // vertical angle (clamped)

    static constexpr float PITCH_MIN = -25.0f;  // slight look from below
    static constexpr float PITCH_MAX =  80.0f;  // nearly top-down
    static constexpr float DIST_MIN  =  0.5f;
    static constexpr float DIST_MAX  = 10.0f;
};
