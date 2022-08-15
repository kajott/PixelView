#pragma once

#include <cstdint>

#include <array>
#include <functional>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "gl_util.h"
#include "gl_header.h"
#include "imgui.h"

class PixelViewApp {
    // GLFW and ImGui stuff
    GLFWwindow* m_window = nullptr;
    ImGuiIO* m_io = nullptr;

    // rendering stuff
    GLuint m_tex = 0;
    GLutil::Program m_prog;
    GLint m_locArea;
    GLint m_locSize;

    // image geometry
    struct Area { double m[4]; };
    Area m_currentArea = {{2.0, -2.0, -1.0, 1.0}};

    // UI state
    bool m_active = true;
    bool m_showDemo = false;
    int m_imgWidth = 0;
    int m_imgHeight = 0;

    // main functions
    void unloadImage();
    void loadImage(const char* filename);

    // UI functions
    void drawUI();

    // event handling
    void handleKeyEvent(int key, int scancode, int action, int mods);
    void handleMouseButtonEvent(int button, int action, int mods);
    void handleCursorPosEvent(double xpos, double ypos);
    void handleScrollEvent(double xoffset, double yoffset);
    void handleDropEvent(int path_count, const char* paths[]);

public:
    inline PixelViewApp() {}
    int run(int argc, char* argv[]);
};
