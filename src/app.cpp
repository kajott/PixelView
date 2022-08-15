#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS  // prevent MSVC warnings
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cassert>

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
    //glfwSetWindowSizeCallback(m_window, [](GLFWwindow* window, int width, int height)
    //    { static_cast<PixelViewApp*>(glfwGetWindowUserPointer(window))->updateSize(width, height); });
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

        // draw the image
        if (m_imgWidth && m_imgHeight) {
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

////////////////////////////////////////////////////////////////////////////////

void PixelViewApp::unloadImage() {
    m_imgWidth = m_imgHeight = 0;
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

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
}

////////////////////////////////////////////////////////////////////////////////

void PixelViewApp::drawUI() {
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
    static PixelViewApp app;
    return app.run(argc, argv);
}
