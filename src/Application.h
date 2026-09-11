#pragma once

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <string>

#include "Camera.h"
#include "Shader.h"
#include "VolumeData.h"
#include "TransferFunction.h"
#include "SlicePlane.h"
#include "ProxyCube.h"
#include "GuiPanel.h"

class Application {
public:
    bool init(int w, int h, const std::string& title);
    void run();
    void shutdown();

    // Panel takes 25% of window width
    float panelWidth() const { return m_width * 0.25f; }
    float viewportWidth() const { return m_width - panelWidth(); }

    // Public state (accessed by GuiPanel) 
    Camera          camera;
    Shader          volumeShader;
    VolumeData      volume;
    TransferFunction transferFunc;
    SlicePlane      slicePlane;
    ProxyCube       proxyCube;
    GuiPanel        gui;

    int   renderMode     = 0;      // 0 = DVR, 1 = ISO
    int   numSteps       = 512;
    float isoThreshold   = 0.3f;
    float minVisibility  = 0.0f;
    float maxVisibility  = 1.0f;
    float alphaThreshold = 0.15f;
    float maxAlpha       = 0.90f;
    bool  lightingEnabled = false;
    int   tfPreset       = 0;
    glm::mat4 modelRotation = glm::mat4(1.0f);  // object rotation (Ctrl+LMB drag)
    static glm::mat4 defaultRotation();
    void resetRotation() { modelRotation = defaultRotation(); }
    bool  gizmoEnabled    = true;

    // Data loading options (set before init or from CLI)
    std::string rawPath;
    std::string iniPath;
    int rawDimX = 0, rawDimY = 0, rawDimZ = 0;
    int rawBits = 8;

private:
    GLFWwindow* m_window = nullptr;
    int m_width = 1600, m_height = 900;
    float m_frameJitter = 0.0f;

    // Background image
    GLuint m_bgTexture = 0;
    GLuint m_bgVAO = 0, m_bgVBO = 0;
    Shader m_bgShader;
    void initBackground(const std::string& shaderDir);
    void drawBackground();

    bool m_leftDown   = false;
    bool m_middleDown = false;
    double m_lastMX = 0, m_lastMY = 0;

    // Rotation gizmo state
    int       m_gizmoAxis       = -1;                  // active axis (-1=none, 0=X, 1=Y, 2=Z)
    int       m_gizmoHoverAxis  = -1;                  // hovered axis
    float     m_gizmoStartAngle = 0.0f;                // screen-space angle at drag start
    glm::mat4 m_gizmoStartRot   = glm::mat4(1.0f);    // modelRotation snapshot
    float     m_gizmoDragMouseX = 0.0f;                // mouse X at drag start
    float     m_gizmoDragMouseY = 0.0f;                // mouse Y at drag start

    void drawRotationGizmo(const glm::mat4& mvp, float vpW, float vpH);
    void drawPlaneGizmo(float vpW, float vpH);

    // Plane gizmo drag state
    bool  m_planeDragging    = false;
    float m_planeDragStartY  = 0.0f;
    glm::vec3 m_planeDragStartPos = glm::vec3(0.0f);

    void render();
    void loadVolumeData();

    static void keyCallback(GLFWwindow* w, int key, int sc, int action, int mods);
    static void scrollCallback(GLFWwindow* w, double dx, double dy);
    static void mouseButtonCallback(GLFWwindow* w, int btn, int action, int mods);
    static void cursorPosCallback(GLFWwindow* w, double x, double y);
    static void framebufferSizeCallback(GLFWwindow* w, int width, int height);
};
