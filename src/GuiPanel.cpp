#include "GuiPanel.h"
#include "Application.h"
#include <imgui.h>

static const char* presetNames[] = { "Heat Map", "Cool-Warm", "Medical", "Grayscale" };

void GuiPanel::draw(Application& app) {
    ImGuiIO& io = ImGui::GetIO();

    // Position panel on the right (40% of window)
    float panelW = app.panelWidth();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - panelW, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, io.DisplaySize.y), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
                           | ImGuiWindowFlags_NoCollapse;
    ImGui::Begin("Volume Controls", nullptr, flags);

    // Reset 
    if (ImGui::Button("Reset View (R)", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
        app.resetRotation();
    }

    // Render Mode
    ImGui::SeparatorText("RENDER MODE");
    ImGui::RadioButton("DVR", &app.renderMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Isosurface", &app.renderMode, 1);

    if (app.renderMode == 1) {
        ImGui::SliderFloat("ISO Threshold", &app.isoThreshold, 0.01f, 1.0f);
    }

    // Colour Preset
    ImGui::SeparatorText("COLOUR PRESET");
    float btnW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    for (int i = 0; i < 4; i++) {
        if (i % 2 != 0) ImGui::SameLine();
        bool selected = (app.tfPreset == i);
        if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.46f, 0.78f, 1.0f));
        if (ImGui::Button(presetNames[i], ImVec2(btnW, 0))) {
            app.tfPreset = i;
            app.transferFunc.setPreset(static_cast<TFPreset>(i));
        }
        if (selected) ImGui::PopStyleColor();
    }

    // Opacity
    ImGui::SeparatorText("OPACITY");
    bool alphaChanged = false;
    alphaChanged |= ImGui::SliderFloat("Air cutoff", &app.alphaThreshold, 0.0f, 0.5f);
    alphaChanged |= ImGui::SliderFloat("Max opacity", &app.maxAlpha, 0.1f, 1.0f);
    if (alphaChanged)
        app.transferFunc.setAlphaParams(app.alphaThreshold, app.maxAlpha);

    // Visibility
    ImGui::SeparatorText("VISIBILITY");
    ImGui::SliderFloat("Min density", &app.minVisibility, 0.0f, 1.0f);
    ImGui::SliderFloat("Max density", &app.maxVisibility, 0.0f, 1.0f);

    // Cross-Section Plane
    ImGui::SeparatorText("CROSS-SECTION PLANE");
    bool sliceChanged = ImGui::Checkbox("Enable Slice (S)", &app.slicePlane.enabled);
    if (sliceChanged) {
        // Slice on → rotation gizmo off; Slice off → rotation gizmo on
        app.gizmoEnabled = !app.slicePlane.enabled;
    }
    if (app.slicePlane.enabled) {
        ImGui::SliderFloat3("Position", &app.slicePlane.position.x, -0.5f, 0.5f);
        ImGui::SliderFloat3("Normal",   &app.slicePlane.normal.x,   -1.0f, 1.0f);
    }

    // Gizmo
    ImGui::SeparatorText("GIZMO");
    ImGui::Checkbox("Show rotation gizmo (G)", &app.gizmoEnabled);

    // Lighting
    ImGui::SeparatorText("LIGHTING");
    ImGui::Checkbox("Gradient lighting", &app.lightingEnabled);

    // Sampling
    ImGui::SeparatorText("SAMPLING");
    ImGui::SliderInt("Ray steps", &app.numSteps, 64, 1024);

    // Info
    ImGui::Separator();
    ImGui::Text("FPS: %.0f", io.Framerate);
    if (app.volume.loaded())
        ImGui::Text("Volume: %dx%dx%d", app.volume.dimX(), app.volume.dimY(), app.volume.dimZ());
    ImGui::Text("Keys: 1=DVR 2=ISO S=Slice");

    ImGui::End();
}
