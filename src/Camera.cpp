#include "Camera.h"
#include <algorithm>
#include <cmath>

void Camera::init(float distance, float yaw, float pitch) {
    m_distance = distance;
    m_yaw = yaw;
    m_pitch = std::clamp(pitch, PITCH_MIN, PITCH_MAX);
}

glm::vec3 Camera::position() const {
    float yr = glm::radians(m_yaw);
    float pr = glm::radians(m_pitch);
    return glm::vec3(
        cosf(pr) * sinf(yr),
        sinf(pr),
        cosf(pr) * cosf(yr)
    ) * m_distance;
}

glm::mat4 Camera::viewMatrix() const {
    // Always looks at origin
    return glm::lookAt(position(), glm::vec3(0.0f), glm::vec3(0, 1, 0));
}

glm::mat4 Camera::projMatrix(float aspect) const {
    return glm::perspective(glm::radians(fov), aspect, nearZ, farZ);
}

void Camera::orbit(float dx, float dy) {
    m_yaw   += dx * 0.3f;
    m_pitch += dy * 0.3f;
    m_pitch = std::clamp(m_pitch, PITCH_MIN, PITCH_MAX);
}

void Camera::zoom(float delta) {
    m_distance -= delta * 0.15f;
    m_distance = std::clamp(m_distance, DIST_MIN, DIST_MAX);
}
