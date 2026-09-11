#include "Application.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <random>
#include <filesystem>
#include <stb_image.h>

// Default orientation: Y up, Z depth 

glm::mat4 Application::defaultRotation() {
    // Rotate -90° around X so the data's Z-axis points into the screen (depth)
    // and Y stays up
    // Rotate 90° around X (Y up), then 180° around Y (face toward camera)
    glm::mat4 r = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0, 1, 0));
    r = r * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1, 0, 0));
    return r;
}

// Init 

bool Application::init(int w, int h, const std::string& title) {
    m_width  = w;
    m_height = h;

    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    m_window = glfwCreateWindow(w, h, title.c_str(), nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1); // vsync

    int version = gladLoadGL(glfwGetProcAddress);
    if (!version) {
        std::cerr << "Failed to load OpenGL\n";
        return false;
    }
    std::cout << "OpenGL " << GLAD_VERSION_MAJOR(version) << "."
              << GLAD_VERSION_MINOR(version) << "\n";

    // Callbacks
    glfwSetWindowUserPointer(m_window, this);
    glfwSetKeyCallback(m_window, keyCallback);
    glfwSetScrollCallback(m_window, scrollCallback);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
    glfwSetCursorPosCallback(m_window, cursorPosCallback);
    glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 430");

    // Style tweaks 
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding   = 0.0f;
    style.FrameRounding    = 7.0f;
    style.GrabRounding     = 7.0f;
    style.FramePadding     = ImVec2(15.0f, 12.0f);
    style.ItemSpacing      = ImVec2(14.0f, 15.0f);
    style.ItemInnerSpacing = ImVec2(12.0f, 8.0f);
    style.WindowPadding    = ImVec2(20.0f, 20.0f);
    style.ScrollbarSize    = 24.0f;
    style.GrabMinSize      = 20.0f;
    style.ScrollbarRounding = 5.0f;
    style.SeparatorTextPadding = ImVec2(20.0f, 6.0f);


    ImGuiIO& imio = ImGui::GetIO();
    imio.FontGlobalScale = 2.5f;

    // Camera
    camera.init(2.5f, -30.0f, 20.0f);

    // Default model orientation: Y up, Z depth
    modelRotation = defaultRotation();

    // Shader
    std::string shaderDir = "shaders/";
    // Try relative to executable first
    if (!std::filesystem::exists(shaderDir + "volume.vert")) {
        // Try parent directory
        shaderDir = "../shaders/";
    }
    if (!volumeShader.loadFromFiles(shaderDir + "volume.vert", shaderDir + "volume.frag")) {
        std::cerr << "Failed to load shaders\n";
        return false;
    }

    // Background image
    initBackground(shaderDir);

    // Proxy cube
    proxyCube.init();

    // Slice plane visible quad
    slicePlane.init(shaderDir);

    // Transfer function
    transferFunc.init();

    // Volume data
    loadVolumeData();

    return true;
}

void Application::loadVolumeData() {
    // Priority: INI > RAW with explicit dims > default sample data > procedural
    if (!iniPath.empty()) {
        if (volume.loadINI(iniPath)) return;
        std::cerr << "Failed to load INI: " << iniPath << "\n";
    }
    if (!rawPath.empty() && rawDimX > 0) {
        if (volume.loadRAW(rawPath, rawDimX, rawDimY, rawDimZ, rawBits)) return;
        std::cerr << "Failed to load RAW: " << rawPath << "\n";
    }

    // Try default sample data locations
    const char* tryPaths[] = {
        "data/VisMale.raw.ini",
        "../data/VisMale.raw.ini",
    };
    for (auto p : tryPaths) {
        if (std::filesystem::exists(p)) {
            if (volume.loadINI(p)) return;
        }
    }

    // Fallback: procedural sphere
    std::cout << "No volume data found, generating procedural sphere\n";
    volume.generateProceduralSphere(128);
}

// Run 

void Application::run() {
    while (!glfwWindowShouldClose(m_window)) {
        glfwPollEvents();
        render();
        glfwSwapBuffers(m_window);
    }
}

// Render

void Application::render() {
    glfwGetFramebufferSize(m_window, &m_width, &m_height);
    if (m_width == 0 || m_height == 0) return;

    glViewport(0, 0, m_width, m_height);
    glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // ImGui new frame
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Draw GUI
    gui.draw(*this);

    // Background image
    drawBackground();

    // Volume rendering pass
    if (volume.loaded()) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);      // render back-faces
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);

        float vpW = viewportWidth();
        float aspect = vpW / (float)m_height;
        if (aspect < 0.1f) aspect = 0.1f;

        // Model matrix: object rotation (Ctrl+LMB) then aspect-ratio scale
        float maxDim = (float)std::max({volume.dimX(), volume.dimY(), volume.dimZ()});
        glm::mat4 scaleM = glm::scale(glm::mat4(1.0f), glm::vec3(
            volume.dimX() / maxDim,
            volume.dimY() / maxDim,
            volume.dimZ() / maxDim));
        glm::mat4 model = modelRotation * scaleM;

        glm::mat4 view = camera.viewMatrix();
        glm::mat4 proj = camera.projMatrix(aspect);

        volumeShader.use();
        volumeShader.setMat4("uModel",      model);
        volumeShader.setMat4("uModelInv",   glm::inverse(model));
        volumeShader.setMat4("uView",       view);
        volumeShader.setMat4("uProjection", proj);
        volumeShader.setVec3("uCameraPos",  camera.position());

        volumeShader.setInt("uRenderMode",    renderMode);
        volumeShader.setInt("uNumSteps",      numSteps);
        volumeShader.setFloat("uIsoThreshold", isoThreshold);
        volumeShader.setFloat("uMinVisibility", minVisibility);
        volumeShader.setFloat("uMaxVisibility", maxVisibility);
        volumeShader.setBool("uLightingEnabled", lightingEnabled);
        // Light from above-front-left to match lab ceiling lighting
        volumeShader.setVec3("uLightDir",    glm::normalize(glm::vec3(-0.2f, -1.0f, -0.3f)));
        volumeShader.setVec3("uAmbientColor", glm::vec3(0.75f, 0.82f, 0.92f));  // cool blue-white ambient
        volumeShader.setBool("uSliceEnabled", slicePlane.enabled);
        volumeShader.setMat4("uSliceMatrix", slicePlane.matrix());

        // Per-frame jitter
        static std::mt19937 rng(42);
        static std::uniform_real_distribution<float> dist(0.0f, 1000.0f);
        volumeShader.setFloat("uJitterOffset", dist(rng));

        // Bind textures
        volume.bind(0, 1);
        transferFunc.bind(2);
        volumeShader.setInt("uVolumeTex",    0);
        volumeShader.setInt("uGradientTex",  1);
        volumeShader.setInt("uTransferFunc", 2);

        // Viewport for volume (leave space for panel on the right)
        glViewport(0, 0, (int)vpW, m_height);
        proxyCube.draw();

        // Restore viewport
        glViewport(0, 0, m_width, m_height);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glEnable(GL_DEPTH_TEST);
    }

    // Visible slice plane quad
    if (slicePlane.enabled && volume.loaded()) {
        float sliceVpW = viewportWidth();
        float sliceAspect = sliceVpW / (float)m_height;
        if (sliceAspect < 0.1f) sliceAspect = 0.1f;
        glViewport(0, 0, (int)sliceVpW, m_height);
        slicePlane.draw(camera.viewMatrix(), camera.projMatrix(sliceAspect));
        glViewport(0, 0, m_width, m_height);
    }

    // Corner bracket gizmos + rotation rings
    if (volume.loaded() && gizmoEnabled) {
        float gvpW = viewportWidth();
        float gvpH = (float)m_height;
        float aspect = gvpW / gvpH;
        if (aspect < 0.1f) aspect = 0.1f;

        float maxDim = (float)std::max({volume.dimX(), volume.dimY(), volume.dimZ()});
        glm::mat4 scaleM = glm::scale(glm::mat4(1.0f), glm::vec3(
            volume.dimX() / maxDim,
            volume.dimY() / maxDim,
            volume.dimZ() / maxDim));
        glm::mat4 mvp = camera.projMatrix(aspect) * camera.viewMatrix() * modelRotation * scaleM;

        // Volume viewport dimensions in pixels
        float vpW = gvpW;
        float vpH = gvpH;

        // Project all 8 corners of the unit cube [-0.5, 0.5]^3 to screen space
        float minSX = 1e9f, minSY = 1e9f, maxSX = -1e9f, maxSY = -1e9f;
        for (int i = 0; i < 8; i++) {
            glm::vec4 corner(
                (i & 1) ? 0.5f : -0.5f,
                (i & 2) ? 0.5f : -0.5f,
                (i & 4) ? 0.5f : -0.5f,
                1.0f);
            glm::vec4 clip = mvp * corner;
            if (clip.w <= 0.0f) continue;
            float ndcX = clip.x / clip.w;
            float ndcY = clip.y / clip.w;
            // NDC [-1,1] → screen pixels (volume viewport)
            float sx = (ndcX * 0.5f + 0.5f) * vpW;
            float sy = (1.0f - (ndcY * 0.5f + 0.5f)) * vpH;  // flip Y for screen
            minSX = std::min(minSX, sx);
            minSY = std::min(minSY, sy);
            maxSX = std::max(maxSX, sx);
            maxSY = std::max(maxSY, sy);
        }

        // Draw L-shaped corner brackets
        float bracketLen = 25.0f;   // arm length in pixels
        float thickness  = 3.0f;
        ImU32 color = IM_COL32(100, 180, 255, 200);  // light blue
        float pad = 8.0f;  // padding outside the bounding box

        float x0 = minSX - pad, y0 = minSY - pad;
        float x1 = maxSX + pad, y1 = maxSY + pad;

        ImDrawList* dl = ImGui::GetForegroundDrawList();
        // Top-left
        dl->AddLine(ImVec2(x0, y0), ImVec2(x0 + bracketLen, y0), color, thickness);
        dl->AddLine(ImVec2(x0, y0), ImVec2(x0, y0 + bracketLen), color, thickness);
        // Top-right
        dl->AddLine(ImVec2(x1, y0), ImVec2(x1 - bracketLen, y0), color, thickness);
        dl->AddLine(ImVec2(x1, y0), ImVec2(x1, y0 + bracketLen), color, thickness);
        // Bottom-left
        dl->AddLine(ImVec2(x0, y1), ImVec2(x0 + bracketLen, y1), color, thickness);
        dl->AddLine(ImVec2(x0, y1), ImVec2(x0, y1 - bracketLen), color, thickness);
        // Bottom-right
        dl->AddLine(ImVec2(x1, y1), ImVec2(x1 - bracketLen, y1), color, thickness);
        dl->AddLine(ImVec2(x1, y1), ImVec2(x1, y1 - bracketLen), color, thickness);

        // Rotation gizmo rings
        if (!slicePlane.enabled) {
            drawRotationGizmo(mvp, vpW, vpH);
        }
    }

    // Plane translation gizmo (when slice is active)
    if (slicePlane.enabled && volume.loaded()) {
        drawPlaneGizmo(viewportWidth(), (float)m_height);
    }

    // ImGui render
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

// Background Image

void Application::initBackground(const std::string& shaderDir) {
    // Load image
    int w, h, channels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load("data/background.png", &w, &h, &channels, 4);
    if (!data) {
        // Try parent dir
        data = stbi_load("../data/background.png", &w, &h, &channels, 4);
    }
    if (!data) {
        std::cerr << "Failed to load background image\n";
        return;
    }

    glGenTextures(1, &m_bgTexture);
    glBindTexture(GL_TEXTURE_2D, m_bgTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    // Fullscreen quad (NDC: covers [-1,1])
    float quad[] = {
        // pos      // uv
        -1, -1,     0, 0,
         1, -1,     1, 0,
         1,  1,     1, 1,
        -1, -1,     0, 0,
         1,  1,     1, 1,
        -1,  1,     0, 1,
    };
    glGenVertexArrays(1, &m_bgVAO);
    glGenBuffers(1, &m_bgVBO);
    glBindVertexArray(m_bgVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_bgVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    m_bgShader.loadFromFiles(shaderDir + "background.vert", shaderDir + "background.frag");
}

void Application::drawBackground() {
    if (!m_bgTexture || !m_bgVAO) return;

    // Draw only in the volume viewport area (left 60%)
    int vpW = (int)viewportWidth();
    glViewport(0, 0, vpW, m_height);

    glDisable(GL_DEPTH_TEST);
    m_bgShader.use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_bgTexture);
    m_bgShader.setInt("uBackgroundTex", 0);
    m_bgShader.setFloat("uDimming", 0.45f);  // dim background for volume contrast

    glBindVertexArray(m_bgVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    // Restore full viewport
    glViewport(0, 0, m_width, m_height);
}

// Rotation Gizmo

void Application::drawRotationGizmo(const glm::mat4& mvp, float vpW, float vpH) {
    const int   NUM_SEGS    = 80;
    const float RING_RADIUS = 0.85f;
    const float HIT_DIST    = 15.0f;   // generous hit distance in pixels
    const float PI2         = 2.0f * 3.14159265f;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImGuiIO& io = ImGui::GetIO();

    // Use GLFW cursor position directly (same coordinate space as framebuffer on Windows)
    double gmx, gmy;
    glfwGetCursorPos(m_window, &gmx, &gmy);
    ImVec2 mousePos((float)gmx, (float)gmy);

    // Axis colors: X=red, Y=green, Z=blue
    struct AxisInfo { glm::vec3 u, v; ImU32 normal, hover, active; };
    AxisInfo axes[3] = {
        { {0,1,0}, {0,0,1}, IM_COL32(220, 70, 70, 180), IM_COL32(255, 100, 100, 230), IM_COL32(255, 140, 140, 255) },
        { {1,0,0}, {0,0,1}, IM_COL32(70, 200, 70, 180),  IM_COL32(100, 240, 100, 230), IM_COL32(140, 255, 140, 255) },
        { {1,0,0}, {0,1,0}, IM_COL32(70, 120, 255, 180), IM_COL32(100, 150, 255, 230), IM_COL32(140, 180, 255, 255) },
    };

    // We need a VP-only matrix (no model transform) for the rings,
    // since rings are in world space surrounding the volume
    glm::mat4 vp = camera.projMatrix(vpW / vpH) * camera.viewMatrix();

    // Project origin to screen (window coords)
    glm::vec4 originClip = vp * glm::vec4(0, 0, 0, 1);
    ImVec2 originScreen(0, 0);
    if (originClip.w > 0.0f) {
        originScreen.x = (originClip.x / originClip.w * 0.5f + 0.5f) * vpW;
        originScreen.y = (1.0f - (originClip.y / originClip.w * 0.5f + 0.5f)) * vpH;
    }

    // Project all 3 rings to screen and compute distances
    ImVec2 allScreenPts[3][NUM_SEGS + 1];
    bool   allValid[3][NUM_SEGS + 1];
    float  ringDist[3] = { 1e9f, 1e9f, 1e9f };

    for (int axis = 0; axis < 3; axis++) {
        for (int i = 0; i <= NUM_SEGS; i++) {
            float t = PI2 * (float)i / (float)NUM_SEGS;
            glm::vec3 p = (cosf(t) * axes[axis].u + sinf(t) * axes[axis].v) * RING_RADIUS;
            glm::vec4 clip = vp * glm::vec4(p, 1.0f);
            if (clip.w <= 0.001f) {
                allValid[axis][i] = false;
                continue;
            }
            allValid[axis][i] = true;
            float ndcX = clip.x / clip.w;
            float ndcY = clip.y / clip.w;
            allScreenPts[axis][i].x = (ndcX * 0.5f + 0.5f) * vpW;
            allScreenPts[axis][i].y = (1.0f - (ndcY * 0.5f + 0.5f)) * vpH;
        }

        // Compute minimum distance from mouse to this ring
        if (m_gizmoAxis < 0) {
            for (int i = 0; i < NUM_SEGS; i++) {
                if (!allValid[axis][i] || !allValid[axis][i + 1]) continue;
                ImVec2 a = allScreenPts[axis][i], b = allScreenPts[axis][i + 1];
                float abx = b.x - a.x, aby = b.y - a.y;
                float apx = mousePos.x - a.x, apy = mousePos.y - a.y;
                float len2 = abx * abx + aby * aby;
                float t = (len2 > 1e-9f) ? std::clamp((abx * apx + aby * apy) / len2, 0.0f, 1.0f) : 0.0f;
                float cx = a.x + t * abx - mousePos.x;
                float cy = a.y + t * aby - mousePos.y;
                float d = sqrtf(cx * cx + cy * cy);
                ringDist[axis] = std::min(ringDist[axis], d);
            }
        }
    }

    // Pick the closest ring within threshold
    m_gizmoHoverAxis = -1;
    if (m_gizmoAxis < 0) {
        float bestDist = HIT_DIST;
        for (int axis = 0; axis < 3; axis++) {
            if (ringDist[axis] < bestDist) {
                bestDist = ringDist[axis];
                m_gizmoHoverAxis = axis;
            }
        }
    }

    // Debug overlay
    {
        char buf[256];
        snprintf(buf, sizeof(buf), "Ring dist: X=%.1f Y=%.1f Z=%.1f  hover=%d active=%d  mouse=(%.0f,%.0f)  origin=(%.0f,%.0f)",
            ringDist[0], ringDist[1], ringDist[2], m_gizmoHoverAxis, m_gizmoAxis,
            mousePos.x, mousePos.y, originScreen.x, originScreen.y);
        dl->AddText(ImVec2(10, vpH - 30), IM_COL32(255, 255, 0, 255), buf);
    }

    // Draw all rings
    for (int axis = 0; axis < 3; axis++) {
        bool isActive  = (m_gizmoAxis == axis);
        bool isHovered = (m_gizmoHoverAxis == axis) && !isActive;
        ImU32 col = isActive ? axes[axis].active : (isHovered ? axes[axis].hover : axes[axis].normal);
        float thick = isActive ? 4.0f : (isHovered ? 3.0f : 2.0f);

        for (int i = 0; i < NUM_SEGS; i++) {
            if (!allValid[axis][i] || !allValid[axis][i + 1]) continue;
            dl->AddLine(allScreenPts[axis][i], allScreenPts[axis][i + 1], col, thick);
        }
    }

    // Click to grab 
    if (m_gizmoAxis < 0 && m_gizmoHoverAxis >= 0 && io.MouseClicked[0] && !io.WantCaptureMouse) {
        m_gizmoAxis = m_gizmoHoverAxis;
        m_gizmoStartRot = modelRotation;
        m_gizmoDragMouseX = mousePos.x;
        m_gizmoDragMouseY = mousePos.y;
        // Cancel orbit — the GLFW callback already set m_leftDown, undo it
        m_leftDown = false;
    }

    // Drag to rotate
    if (m_gizmoAxis >= 0) {
        if (io.MouseDown[0]) {
            m_leftDown = false;  // keep orbit suppressed every frame

            float startAngle = atan2f(m_gizmoDragMouseY - originScreen.y, m_gizmoDragMouseX - originScreen.x);
            float currentAngle = atan2f(mousePos.y - originScreen.y, mousePos.x - originScreen.x);
            float delta = currentAngle - startAngle;

            glm::vec3 rotAxis;
            if (m_gizmoAxis == 0) rotAxis = glm::vec3(1, 0, 0);
            else if (m_gizmoAxis == 1) rotAxis = glm::vec3(0, 1, 0);
            else rotAxis = glm::vec3(0, 0, 1);

            modelRotation = glm::rotate(glm::mat4(1.0f), -delta, rotAxis) * m_gizmoStartRot;
        } else {
            m_gizmoAxis = -1;
        }
    }
}

// Plane Translation Gizmo

void Application::drawPlaneGizmo(float vpW, float vpH) {
    float aspect = vpW / vpH;
    if (aspect < 0.1f) aspect = 0.1f;
    glm::mat4 vp = camera.projMatrix(aspect) * camera.viewMatrix();

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImGuiIO& io = ImGui::GetIO();

    double gmx, gmy;
    glfwGetCursorPos(m_window, &gmx, &gmy);
    ImVec2 mousePos((float)gmx, (float)gmy);

    glm::vec3 n = glm::normalize(slicePlane.normal);
    glm::vec3 pos = slicePlane.position;

    // Project plane center and normal endpoints to screen
    auto project = [&](glm::vec3 worldPt) -> ImVec2 {
        glm::vec4 clip = vp * glm::vec4(worldPt, 1.0f);
        if (clip.w <= 0.001f) return ImVec2(-999, -999);
        float sx = (clip.x / clip.w * 0.5f + 0.5f) * vpW;
        float sy = (1.0f - (clip.y / clip.w * 0.5f + 0.5f)) * vpH;
        return ImVec2(sx, sy);
    };

    // Arrow: from pos - 0.4*n to pos + 0.4*n
    float arrowLen = 0.4f;
    ImVec2 p0 = project(pos - n * arrowLen);
    ImVec2 p1 = project(pos);
    ImVec2 p2 = project(pos + n * arrowLen);

    // Draw the normal line
    ImU32 lineCol = IM_COL32(50, 220, 120, 200);
    ImU32 handleCol = IM_COL32(80, 255, 150, 255);
    ImU32 hoverCol = IM_COL32(150, 255, 200, 255);
    ImU32 activeCol = IM_COL32(255, 255, 100, 255);
    float lineThick = 3.0f;
    float handleRadius = 12.0f;

    dl->AddLine(p0, p2, lineCol, lineThick);

    // Arrowhead at p2
    ImVec2 dir(p2.x - p1.x, p2.y - p1.y);
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
    if (len > 1.0f) {
        dir.x /= len; dir.y /= len;
        ImVec2 perp(-dir.y, dir.x);
        float arrowSize = 14.0f;
        ImVec2 a1(p2.x - dir.x * arrowSize + perp.x * arrowSize * 0.5f,
                  p2.y - dir.y * arrowSize + perp.y * arrowSize * 0.5f);
        ImVec2 a2(p2.x - dir.x * arrowSize - perp.x * arrowSize * 0.5f,
                  p2.y - dir.y * arrowSize - perp.y * arrowSize * 0.5f);
        dl->AddTriangleFilled(p2, a1, a2, lineCol);
    }

    // Draggable handle at plane center
    float dx = mousePos.x - p1.x;
    float dy = mousePos.y - p1.y;
    float distToHandle = sqrtf(dx * dx + dy * dy);
    bool hovered = distToHandle < handleRadius * 1.5f && !io.WantCaptureMouse;

    ImU32 hCol = m_planeDragging ? activeCol : (hovered ? hoverCol : handleCol);
    dl->AddCircleFilled(p1, handleRadius, hCol);
    dl->AddCircle(p1, handleRadius, IM_COL32(255, 255, 255, 180), 0, 2.0f);

    // Click to start drag
    if (hovered && io.MouseClicked[0] && !m_planeDragging) {
        m_planeDragging = true;
        m_planeDragStartY = mousePos.y;
        m_planeDragStartPos = pos;
        m_leftDown = false;  // cancel orbit
    }

    // Drag: move plane along its normal
    if (m_planeDragging) {
        if (io.MouseDown[0]) {
            m_leftDown = false;
            float deltaY = mousePos.y - m_planeDragStartY;
            float sensitivity = 0.002f;
            slicePlane.position = m_planeDragStartPos - n * deltaY * sensitivity;
            // Clamp to [-0.5, 0.5]
            slicePlane.position = glm::clamp(slicePlane.position, glm::vec3(-0.5f), glm::vec3(0.5f));
        } else {
            m_planeDragging = false;
        }
    }
}

// Shutdown

void Application::shutdown() {
    proxyCube.destroy();
    slicePlane.destroy();
    if (m_bgTexture) { glDeleteTextures(1, &m_bgTexture); m_bgTexture = 0; }
    if (m_bgVAO) { glDeleteVertexArrays(1, &m_bgVAO); m_bgVAO = 0; }
    if (m_bgVBO) { glDeleteBuffers(1, &m_bgVBO); m_bgVBO = 0; }
    volume.destroy();
    transferFunc.destroy();

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(m_window);
    glfwTerminate();
}

// GLFW Callbacks

void Application::keyCallback(GLFWwindow* w, int key, int sc, int action, int mods) {
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(w));
    if (action != GLFW_PRESS) return;

    switch (key) {
        case GLFW_KEY_ESCAPE: glfwSetWindowShouldClose(w, GLFW_TRUE); break;
        case GLFW_KEY_1: app->renderMode = 0; break;
        case GLFW_KEY_2: app->renderMode = 1; break;
        case GLFW_KEY_S:
            app->slicePlane.enabled = !app->slicePlane.enabled;
            app->gizmoEnabled = !app->slicePlane.enabled;
            break;
        case GLFW_KEY_G: app->gizmoEnabled = !app->gizmoEnabled; break;
        case GLFW_KEY_R: app->resetRotation(); break;
    }
}

void Application::scrollCallback(GLFWwindow* w, double dx, double dy) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(w));
    app->camera.zoom((float)dy);
}

void Application::mouseButtonCallback(GLFWwindow* w, int btn, int action, int mods) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(w));
    if (btn == GLFW_MOUSE_BUTTON_LEFT) {
        app->m_leftDown = (action == GLFW_PRESS);
        glfwGetCursorPos(w, &app->m_lastMX, &app->m_lastMY);
    }
    if (btn == GLFW_MOUSE_BUTTON_MIDDLE) {
        app->m_middleDown = (action == GLFW_PRESS);
        glfwGetCursorPos(w, &app->m_lastMX, &app->m_lastMY);
    }
}

void Application::cursorPosCallback(GLFWwindow* w, double x, double y) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(w));
    double dx = x - app->m_lastMX;
    double dy = y - app->m_lastMY;
    app->m_lastMX = x;
    app->m_lastMY = y;

    if (app->m_leftDown && app->m_gizmoAxis < 0 && !app->m_planeDragging) {
        bool ctrl = glfwGetKey(w, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS
                 || glfwGetKey(w, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
        if (ctrl) {
            // Ctrl+LMB: rotate the object around its center
            float angleX = (float)dx * 0.5f;  // degrees per pixel
            float angleY = (float)dy * 0.5f;
            glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), glm::radians(angleX), glm::vec3(0, 1, 0));
            glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), glm::radians(angleY), glm::vec3(1, 0, 0));
            app->modelRotation = rotY * rotX * app->modelRotation;
        } else {
            // Plain LMB: orbit camera
            app->camera.orbit((float)dx, (float)-dy);
        }
    }
}

void Application::framebufferSizeCallback(GLFWwindow* w, int width, int height) {
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(w));
    app->m_width  = width;
    app->m_height = height;
}
