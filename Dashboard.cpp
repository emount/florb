#include <iostream>

#include "Dashboard.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"


// Namespace using directives
using std::cerr;
using std::endl;


// Implementation of class Dashboard

// Constructor

Dashboard::Dashboard() { }


// Public methods

void Dashboard::show() {
    cerr << "Showing Florb Dashboard" << endl;

    ImGui::OpenPopup("Dashboard");

    if (ImGui::BeginPopupModal("Dashboard",
                               nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Are you sure you want to proceed?");
        ImGui::Separator();

        if (ImGui::Button("OK", ImVec2(120, 0))) {
            // Handle OK action here
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            // Handle Cancel action here
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
