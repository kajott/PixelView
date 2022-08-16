#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS  // prevent MSVC warnings
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cassert>

#include <algorithm>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "gl_header.h"
#include "gl_util.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "stb_image.h"

#include "string_util.h"

#include "app.h"

#ifndef NDEBUG  // secondary debug switch for very verbose debug sources
    //#define DEBUG_UPDATE_VIEW
#endif


int PixelViewApp::run(int argc, char *argv[]) {
    (void)argc, (void)argv;

    if (!glfwInit()) {
        const char* err = "unknown error";
        glfwGetError(&err);
        fprintf(stderr, "glfwInit failed: %s\n", err);
        return 1;
    }

    glfwWindowHint(GLFW_RED_BITS,     8);
    glfwWindowHint(GLFW_GREEN_BITS,   8);
    glfwWindowHint(GLFW_BLUE_BITS,    8);
    glfwWindowHint(GLFW_ALPHA_BITS,   8);
    glfwWindowHint(GLFW_DEPTH_BITS,   0);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    #ifndef NDEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
    #endif

    m_window = glfwCreateWindow(
        1024, 768,
        "PixelView",
        nullptr, nullptr);
    if (m_window == nullptr) {
        const char* err = "unknown error";
        glfwGetError(&err);
        fprintf(stderr, "glfwCreateWindow failed: %s\n", err);
        return 1;
    }

    glfwSetWindowUserPointer(m_window, static_cast<void*>(this));
    glfwSetKeyCallback(m_window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
        { static_cast<PixelViewApp*>(glfwGetWindowUserPointer(window))->handleKeyEvent(key, scancode, action, mods); });
    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* window, int button, int action, int mods)
        { static_cast<PixelViewApp*>(glfwGetWindowUserPointer(window))->handleMouseButtonEvent(button, action, mods); });
    glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xpos, double ypos)
        { static_cast<PixelViewApp*>(glfwGetWindowUserPointer(window))->handleCursorPosEvent(xpos, ypos); });
    glfwSetScrollCallback(m_window, [](GLFWwindow* window, double xoffset, double yoffset)
        { static_cast<PixelViewApp*>(glfwGetWindowUserPointer(window))->handleScrollEvent(xoffset, yoffset); });
    glfwSetWindowSizeCallback(m_window, [](GLFWwindow* window, int width, int height)
        { static_cast<PixelViewApp*>(glfwGetWindowUserPointer(window))->handleResizeEvent(width, height); });
    glfwSetDropCallback(m_window, [](GLFWwindow* window, int path_count, const char* paths[])
        { static_cast<PixelViewApp*>(glfwGetWindowUserPointer(window))->handleDropEvent(path_count, paths); });

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);

    #ifdef GL_HEADER_IS_GLAD
        if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
            fprintf(stderr, "failed to load OpenGL 3.3 functions\n");
            return 1;
        }
    #else
        #error no valid GL header / loader
    #endif

    if (!GLutil::init()) {
        fprintf(stderr, "OpenGL initialization failed\n");
        return 1;
    }
    GLutil::enableDebugMessages();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    ImGui::CreateContext();
    m_io = &ImGui::GetIO();
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init(nullptr);

    glGenTextures(1, &m_tex);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    GLutil::checkError("texture setup");

    {
        GLutil::Shader vs(GL_VERTEX_SHADER,
             "#version 330 core"
        "\n" "uniform vec4 uArea;"
        "\n" "out vec2 vPos;"
        "\n" "void main() {"
        "\n" "  vec2 pos = vec2(float(gl_VertexID & 1), float((gl_VertexID & 2) >> 1));"
        "\n" "  vPos = pos;"
        "\n" "  gl_Position = vec4(uArea.xy * pos + uArea.zw, 0., 1.);"
        "\n" "}"
        "\n");
        if (!vs.good()) {
            fprintf(stderr, "vertex shader compilation failed\n");
            return 1;
        }
        GLutil::Shader fs(GL_FRAGMENT_SHADER,
             "#version 330 core"
        "\n" "uniform vec2 uSize;"
        "\n" "uniform sampler2D uTex;"
        "\n" "in vec2 vPos;"
        "\n" "out vec4 oColor;"
        "\n" "void main() {"
        "\n" "  vec2 pos = vPos * uSize;"
        "\n" "  vec2 i = floor(pos + 0.5);"
        "\n" "  pos = (i + clamp((pos - i) / fwidth(pos), -0.5, 0.5)) / uSize;"
        "\n" "  oColor = texture(uTex, pos);"
        "\n" "}"
        "\n");
        if (!fs.good()) {
            fprintf(stderr, "fragment shader compilation failed\n");
            return 1;
        }
        m_prog.link(vs, fs);
        if (!m_prog.good()) {
            fprintf(stderr, "program linking failed\n");
            return 1;
        }
        m_locArea = glGetUniformLocation(m_prog, "uArea");
        m_locSize = glGetUniformLocation(m_prog, "uSize");
    }

    // main loop
    while (m_active && !glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        // process the UI
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        drawUI();
        #ifndef NDEBUG
            if (m_showDemo) {
                ImGui::ShowDemoWindow(&m_showDemo);
            }
        #endif
        ImGui::Render();

        // start display rendering
        GLutil::clearError();
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glViewport(0, 0, int(m_io->DisplaySize.x), int(m_io->DisplaySize.y));
        glClear(GL_COLOR_BUFFER_BIT);

        // apply smooth transitions [TODO]
        m_currentArea = m_targetArea;

        // draw the image
        if (imgValid()) {
            glUseProgram(m_prog);
            glBindTexture(GL_TEXTURE_2D, m_tex);
            glUniform2f(m_locSize, float(m_imgWidth), float(m_imgHeight));
            glUniform4f(m_locArea, float(m_currentArea.m[0]), float(m_currentArea.m[1]), float(m_currentArea.m[2]), float(m_currentArea.m[3]));
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        }

        // draw the GUI and finish the frame
        GLutil::checkError("content draw");
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        GLutil::checkError("GUI draw");
        glfwSwapBuffers(m_window);
    }

    // clean up
    #ifndef NDEBUG
        fprintf(stderr, "exiting ...\n");
    #endif
    glUseProgram(0);
    m_prog.free();
    GLutil::done();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(m_window);
    glfwTerminate();
    #ifndef NDEBUG
        fprintf(stderr, "bye!\n");
    #endif
    return 0;
}

////////////////////////////////////////////////////////////////////////////////

void PixelViewApp::handleKeyEvent(int key, int scancode, int action, int mods) {
    (void)scancode, (void)mods;
    if (((action != GLFW_PRESS) && (action != GLFW_REPEAT)) || m_io->WantCaptureKeyboard) { return; }
    switch (key) {
        case GLFW_KEY_F9:
            m_showDemo = !m_showDemo;
            break;
        default:
            break;
    }
}

void PixelViewApp::handleMouseButtonEvent(int button, int action, int mods) {
    (void)button, (void)action, (void)mods;
    if (m_io->WantCaptureMouse) { return; }
}

void PixelViewApp::handleCursorPosEvent(double xpos, double ypos) {
    (void)xpos, (void)ypos;
}

void PixelViewApp::handleScrollEvent(double xoffset, double yoffset) {
    (void)xoffset, (void)yoffset;
    if (m_io->WantCaptureMouse) { return; }
}

void PixelViewApp::handleDropEvent(int path_count, const char* paths[]) {
    if ((path_count < 1) || !paths || !paths[0] || !paths[0][0]) { return; }
    loadImage(paths[0]);
}

void PixelViewApp::handleResizeEvent(int width, int height) {
    updateView(width, height);
}

////////////////////////////////////////////////////////////////////////////////

void PixelViewApp::loadImage(const char* filename) {
    m_imgWidth = m_imgHeight = 0;
    #ifndef NDEBUG
        printf("loading image: '%s'\n", filename);
    #endif
    void* data = stbi_load(filename, &m_imgWidth, &m_imgHeight, nullptr, 3);
    if (!data) {
        #ifndef NDEBUG
            printf("image loading failed\n");
        #endif
        unloadImage(); return;
    }
    glBindTexture(GL_TEXTURE_2D, m_tex);
    GLutil::checkError("before uploading image texture");
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, m_imgWidth, m_imgHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glFlush();
    glFinish();
    ::free(data);
    if (GLutil::checkError("after uploading image texture")) {
        unloadImage(); return;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    #ifndef NDEBUG
        printf("loaded image successfully (%dx%d pixels)\n", m_imgWidth, m_imgHeight);
    #endif
    m_aspect = 1.0;
    m_viewMode = vmFit;
    m_x0 = m_y0 = 0.0;
    updateView();
}

void PixelViewApp::unloadImage() {
    m_imgWidth = m_imgHeight = 0;
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

////////////////////////////////////////////////////////////////////////////////

void PixelViewApp::updateView() {
    updateView(m_io->DisplaySize.x, m_io->DisplaySize.y);
}

void PixelViewApp::updateView(double screenWidth, double screenHeight) {
    if (!imgValid()) {
        return;  // no image loaded
    }

    // compute raw image size with aspect ratio correction
    double rawWidth  = m_imgWidth  * std::max(m_aspect, 1.0);
    double rawHeight = m_imgHeight / std::min(m_aspect, 1.0);
    bool isInt = wantIntegerZoom();
    bool autofit = (m_viewMode != vmFree);
    #ifdef DEBUG_UPDATE_VIEW
        printf("updateView: mode=%s int=%s imgSize=%dx%d rawSize=%.0fx%.0f screenSize=%.0fx%.0f ",
               (m_viewMode == vmFree) ? "free" : (m_viewMode == vmFill) ? "fill" : "fit ",
               isInt ? "yes" : "no ",
               m_imgWidth,m_imgHeight, rawWidth,rawHeight, screenWidth,screenHeight);
    #endif

    // perform auto-fit computations
    if (autofit) {
        // compute scaling factors in both directions;
        // in integer scaling mode, take the maximum crop into account
        double zoomX = screenWidth  / (isInt ? (rawWidth  * (1.0 - m_maxCrop)) : rawWidth);
        double zoomY = screenHeight / (isInt ? (rawHeight * (1.0 - m_maxCrop)) : rawHeight);
        // select the appropriate zoom factor
        if (m_viewMode == vmFill) { m_zoom = std::max(zoomX, zoomY); }
        else                      { m_zoom = std::min(zoomX, zoomY); }
    }
    #ifdef DEBUG_UPDATE_VIEW
        printf("zoom=%.3f", m_zoom);
    #endif

    // integer zooming; essentially there's three modes of operation here:
    // - enforce exact integer zoom levels if we're very close to an integer
    //   anyway (done to avoid numerical issues when zooming in/out repeatedly)
    // - round zoom level to the next closest integer (if integer zoom is on)
    // - round zoom level to the next *lower* integer (in integer autofit mode)
    bool zoomDown = (m_zoom < 1.0);
    if (zoomDown) { m_zoom = 1.0 / m_zoom; }
    double rounding = (isInt && autofit) ? (zoomDown ? 0.999 : 0.0) : 0.5;
    double intZoom = std::floor(m_zoom + rounding);
    if (isInt || (std::abs(m_zoom - intZoom) < 0.001)) { m_zoom = intZoom; }
    if (zoomDown) { m_zoom = 1.0 / m_zoom; }

    // compute final document size
    double viewWidth  = rawWidth  * m_zoom;
    double viewHeight = rawHeight * m_zoom;
    #ifdef DEBUG_UPDATE_VIEW
        printf("->%.3f viewSize=%.0fx%.0f offset=%.0f,%.0f", m_zoom, viewWidth,viewHeight, m_x0,m_y0);
    #endif

    // constrain image origin
    auto constrain = [&] (double &pos, double &minPos, double viewSize, double screenSize) {
        if (autofit || (viewSize <= screenSize)) {
            pos = std::floor((screenSize - viewSize) * 0.5);
            minPos = 0.0;
        } else {
            minPos = screenSize - viewSize;
            pos = std::floor(std::min(0.0, std::max(minPos, pos)));
        }
    };
    constrain(m_x0, m_minX0, viewWidth,  screenWidth);
    constrain(m_y0, m_minY0, viewHeight, screenHeight);
    #ifdef DEBUG_UPDATE_VIEW
        printf("->%.0f,%.0f\n", m_x0,m_y0);
    #endif

    // convert into transform matrix
    m_targetArea.m[0] =  2.0 * (viewWidth  / screenWidth);
    m_targetArea.m[1] = -2.0 * (viewHeight / screenHeight);
    m_targetArea.m[2] =  2.0 * (m_x0       / screenWidth)  - 1.0;
    m_targetArea.m[3] = -2.0 * (m_y0       / screenHeight) + 1.0;
}

////////////////////////////////////////////////////////////////////////////////

void PixelViewApp::drawUI() {
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
    static PixelViewApp app;
    return app.run(argc, argv);
}
