#pragma once

#include <imgui.h>

// ImGuizmo 1.83 still uses the pre-ImGui-1.90 helper name.
namespace ImGui {

inline void CaptureMouseFromApp() {
    SetNextFrameWantCaptureMouse(true);
}

} // namespace ImGui
