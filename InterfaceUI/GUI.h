#pragma once
#include <imgui/imconfig.h>
#include <imgui/imgui.h>
#if defined(_WIN32)
#include <imgui/backends/imgui_impl_glfw.h>
#endif
#include <imgui/backends/imgui_impl_vulkan.h>


// inline 避免 header 被多个 TU 包含时产生重复定义（ODR 违规）。
// 原实现为 static，每个包含此头文件的翻译单元都会生成一份副本，造成代码膨胀。
inline void HelpMarker2(const char* desc)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}
