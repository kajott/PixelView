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
    //#define DEBUG_ANIMATION
#endif

static constexpr int    defaultWindowWidth   = 1024;
static constexpr int    defaultWindowHeight  =  768;
static constexpr double zoomStepSize         =    1.4142135623730951;  // sqrt(2)
static constexpr double animationSpeed       =    0.125;
static constexpr double cursorPanSpeedSlow   =    8.0;  // pixels per keypress (with Shift)
static constexpr double cursorPanSpeedNormal =   64.0;  // pixels per keypress
static constexpr double cursorPanSpeedFast   =  512.0;  // pixels per keypress (with Ctrl)
static constexpr double cursorHideDelay      =    0.5;  // mouse cursor hide delay (seconds)

static const double presetScrollSpeeds[] = { 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0, 24.0 };
static constexpr int numPresetScrollSpeeds = int(sizeof(presetScrollSpeeds) / sizeof(*presetScrollSpeeds));


int PixelViewApp::run(int argc, char *argv[]) {
    if (argc > 1) {
        m_fileName = StringUtil::copy(argv[1]);
        m_fullscreen = true;
    }

    if (!glfwInit()) {
        const char* err = "unknown error";
        glfwGetError(&err);
        fprintf(stderr, "glfwInit failed: %s\n", err);
        return 1;
    }

    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
    glfwWindowHint(GLFW_RED_BITS,     mode->redBits);
    glfwWindowHint(GLFW_GREEN_BITS,   mode->greenBits);
    glfwWindowHint(GLFW_BLUE_BITS,    mode->blueBits);
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
        m_fullscreen ? mode->width  : defaultWindowWidth,
        m_fullscreen ? mode->height : defaultWindowHeight,
        "PixelView",
        m_fullscreen ? monitor : nullptr,
        nullptr);
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
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* window, int width, int height)
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
    m_io->IniFilename = nullptr;
    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init(nullptr);

    glGenTextures(1, &m_tex);
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
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
        "\n" "float mapPos(in float pos, in float deriv) {"
        "\n" "  float d = abs(deriv);"
        "\n" "  if (d >= 1.03125) { return pos; }"
        "\n" "  float i = floor(pos + 0.5);"
        "\n" "  return i + clamp((pos - i) / d, -0.5, 0.5);"
        "\n" "}"
        "\n" "void main() {"
        "\n" "  vec2 rpos = vPos * uSize;"
        "\n" "  vec2 mpos = vec2(mapPos(rpos.x, dFdx(rpos).x),"
        "\n" "                   mapPos(rpos.y, dFdy(rpos).y));"
        "\n" "  oColor = texture(uTex, mpos / uSize, -0.25);"
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

    // set a default window geometry when switching back from fullscreen
    m_windowGeometry.width  = defaultWindowWidth;
    m_windowGeometry.height = defaultWindowHeight;
    m_windowGeometry.xpos   = (mode->width  - defaultWindowWidth)  >> 1;
    m_windowGeometry.ypos   = (mode->height - defaultWindowHeight) >> 1;

    // initialize screen geometry and load document
    updateScreenSize();
    updateCursor();
    if (m_fileName) {
        loadImage();
    }

    // main loop
    while (m_active && !glfwWindowShouldClose(m_window)) {
        glfwPollEvents();

        // hide the cursor
        if ((m_hideCursorAt > 0.0) && (glfwGetTime() > m_hideCursorAt) && !m_panning) {
            updateCursor();
            m_hideCursorAt = 0.0;
        }

        // process the UI
        if (!m_cursorVisible) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_None);
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        if (m_showHelp)   { uiHelpWindow(); }
        if (m_showConfig) { uiConfigWindow(); }
        #ifndef NDEBUG
            if (m_showDemo) { ImGui::ShowDemoWindow(&m_showDemo); }
        #endif
        ImGui::Render();

        // start display rendering
        GLutil::clearError();
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glViewport(0, 0, int(m_io->DisplaySize.x), int(m_io->DisplaySize.y));
        glClear(GL_COLOR_BUFFER_BIT);

        // auto-scroll
        if (isScrolling()) {
            m_x0 -= m_scrollX * m_scrollSpeed;
            m_y0 -= m_scrollY * m_scrollSpeed;
            if ((m_x0 > 0.0) || (m_x0 < m_minX0)) { m_scrollX = 0.0; }
            if ((m_y0 > 0.0) || (m_y0 < m_minY0)) { m_scrollY = 0.0; }
            updateView();
        }

        // apply smooth transitions
        if (m_animate) {
            double sad = 0.0;
            for (int i = 0;  i < 4;  ++i) {
                double diff = m_targetArea.m[i] - m_currentArea.m[i];
                m_currentArea.m[i] += animationSpeed * diff;
                sad += std::fabs(diff);
            }
            if (sad < (std::min(m_targetArea.m[0], -m_targetArea.m[1]) * (1.0 / 256))) {
                m_animate = false;
                #ifdef DEBUG_ANIMATION
                    printf("animation stopped.\n");
                #endif
            }
        } else {
            m_currentArea = m_targetArea;
        }

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
    ::free((void*)m_fileName);
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
    if (key != GLFW_KEY_ESCAPE) { m_escapePressed = false; }
    switch (key) {
        case GLFW_KEY_TAB:
        case GLFW_KEY_F2: m_showConfig = !m_showConfig; updateCursor(); break;
        case GLFW_KEY_F1: m_showHelp   = !m_showHelp;   updateCursor(); break;
        case GLFW_KEY_F9: m_showDemo   = !m_showDemo;   updateCursor(); break;
        case GLFW_KEY_F11: toggleFullscreen(); break;
        case GLFW_KEY_F10:
        case GLFW_KEY_Q: m_active = false; break;
        case GLFW_KEY_I: if (canDoIntegerZoom()) { m_integer = !m_integer; viewCfg("a"); } break;
        case GLFW_KEY_S: if (isScrolling()) { m_scrollX = m_scrollY = 0.0; } else { startScroll(); } break;
        case GLFW_KEY_Z:
        case GLFW_KEY_Y:
        case GLFW_KEY_KP_DIVIDE:   cycleViewMode(true);   break;
        case GLFW_KEY_F:
        case GLFW_KEY_KP_MULTIPLY: cycleViewMode(false);  break;
        case GLFW_KEY_KP_ADD:      changeZoom(+1.0);      break;
        case GLFW_KEY_KP_SUBTRACT: changeZoom(-1.0);      break;
        case GLFW_KEY_LEFT:   cursorPan(-1.0, 0.0, mods); break;
        case GLFW_KEY_RIGHT:  cursorPan(+1.0, 0.0, mods); break;
        case GLFW_KEY_UP:     cursorPan(0.0, -1.0, mods); break;
        case GLFW_KEY_DOWN:   cursorPan(0.0, +1.0, mods); break;
        case GLFW_KEY_HOME: m_x0 =     0.0;  m_y0 =     0.0;  viewCfg("fsa"); break;
        case GLFW_KEY_END:  m_x0 = m_minX0;  m_y0 = m_minY0;  viewCfg("fsa"); break;
        case GLFW_KEY_ESCAPE: if (m_escapePressed) { m_active = false; } else { m_escapePressed = true; m_scrollX = m_scrollY = 0.0; viewCfg("x"); } break;
        default:
            if ((key >= GLFW_KEY_1) && (key < (GLFW_KEY_1 + numPresetScrollSpeeds))) {
                startScroll(presetScrollSpeeds[key - GLFW_KEY_1]);
            }
            break;
    }
}

void PixelViewApp::handleMouseButtonEvent(int button, int action, int mods) {
    (void)mods;
    if (action == GLFW_RELEASE) {
        m_panning = false;
    } else if (!m_io->WantCaptureMouse && ((button == GLFW_MOUSE_BUTTON_LEFT) || (button == GLFW_MOUSE_BUTTON_MIDDLE))) {
        double x = m_io->DisplaySize.x * 0.5;
        double y = m_io->DisplaySize.y * 0.5;
        glfwGetCursorPos(m_window, &x, &y);
        m_panX = m_x0 - x;
        m_panY = m_y0 - y;
        m_scrollX = m_scrollY = 0.0;
        m_panning = true;
    }
    m_escapePressed = false;
}

void PixelViewApp::handleCursorPosEvent(double xpos, double ypos) {
    updateCursor(true);
    if (m_panning && ((glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_LEFT)   == GLFW_PRESS)
                  ||  (glfwGetMouseButton(m_window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS))) {
        m_x0 = xpos + m_panX;
        m_y0 = ypos + m_panY;
        viewCfg("fsx");
    }
}

void PixelViewApp::handleScrollEvent(double xoffset, double yoffset) {
    (void)xoffset;
    if (m_io->WantCaptureMouse) { return; }
    updateCursor(true);
    double xpos = m_io->DisplaySize.x * 0.5;
    double ypos = m_io->DisplaySize.y * 0.5;
    glfwGetCursorPos(m_window, &xpos, &ypos);
    changeZoom(yoffset, xpos, ypos);
    m_escapePressed = false;
}

void PixelViewApp::handleDropEvent(int path_count, const char* paths[]) {
    if ((path_count < 1) || !paths || !paths[0] || !paths[0][0]) { return; }
    loadImage(paths[0]);
    m_escapePressed = false;
}

void PixelViewApp::handleResizeEvent(int width, int height) {
    m_screenWidth  = width;
    m_screenHeight = height;
    updateView();
}

////////////////////////////////////////////////////////////////////////////////

void PixelViewApp::viewCfg(const char* actions) {
    if (!actions) { return; }
    bool callUpdateView = true;
    for (;  *actions;  ++actions) {
        switch (*actions) {
            case 'f': m_viewMode     = vmFree; break;
            case 'a': m_animate      = true;   break;
            case 'x': m_animate      = false;  break;
            case 's': m_scrollX      =
                      m_scrollY      = 0.0;    break;
            case 'n': callUpdateView = false;  break;
            default: break;
        }
    }
    if (callUpdateView) { updateView(); }
}

void PixelViewApp::changeZoom(double direction, double pivotX, double pivotY) {
    if (std::fabs(direction) < 0.01) { return; }
    double zstep;
    double zdir = (direction < 0.0) ? (-1.0) : 1.0;

    // convert zoom into pseudo-logarithmic scale
    if (wantIntegerZoom()) {
        zstep = (m_zoom >= 1.0) ? (m_zoom - 1.0) : (1.0 - 1.0 / m_zoom);
    } else {
        zstep = std::log(m_zoom) / std::log(zoomStepSize);
    }

    // go to the closest stop in the relevant direction;
    // if we're already pretty close to a stop, just go one further
    double istep = std::floor(zstep + 0.5);
    if (std::fabs(zstep - istep) < 0.125) {
        zstep = istep + zdir;
    } else {
        zstep = std::floor(zstep) + zdir;
    }

    // convert back to standard zoom value
    if (wantIntegerZoom()) {
        m_zoom = (zstep >= 0.0) ? (zstep + 1.0) : (1.0 / (1.0 - zstep));
    } else {
        m_zoom = std::pow(zoomStepSize, zstep);
    }

    // perform the actual zoom action
    viewCfg("fsan");
    updateView(true, pivotX, pivotY);
}

void PixelViewApp::cursorPan(double dx, double dy, int mods) {
    if (mods & GLFW_MOD_ALT) {
        startScroll(0.0, dx, dy);
        return;
    }
    double speed = (mods & GLFW_MOD_CONTROL) ? cursorPanSpeedFast
                 : (mods & GLFW_MOD_SHIFT)   ? cursorPanSpeedSlow
                 :                             cursorPanSpeedNormal;
    m_x0 = std::floor(m_x0) - dx * speed;
    m_y0 = std::floor(m_y0) - dy * speed;
    viewCfg("fsa");
}

void PixelViewApp::cycleViewMode(bool with1x) {
    if (with1x && ((m_viewMode == vmFill) || (true && (std::fabs(m_zoom - 1.0) > 0.03125)))) {
        m_zoom = 1.0;
        m_viewMode = vmFree;
    } else {
        m_viewMode = (m_viewMode == vmFit) ? vmFill : vmFit;
    }
    viewCfg("sa");
}

void PixelViewApp::startScroll(double speed, double dx, double dy) {
    if (speed != 0.0) {
        // speed is specified -> set the speed
        m_scrollSpeed = speed;
    }
    if ((dx != 0.0) || (dy != 0.0)) {
        // direction is specified -> set the direction
        m_scrollX = dx;
        m_scrollY = dy;
    } else if (!isScrolling()) {
        // direction is not specified, and we're not scrolling already -> auto-scroll!
        // detect in which direction we'd have the longest way to go and use that
        double longestDist = 0.0;
        for (int dir = 0;  dir < 4;  ++dir) {
            double d = 1.0 - (dir & 2), pos, minPos;
            if (dir & 1) { dx = d; dy = 0.0; pos = m_x0; minPos = m_minX0; }
            else         { dx = 0.0; dy = d; pos = m_y0; minPos = m_minY0; }
            if (minPos >= 0.0) { continue; /* can't scroll on this axis at all */ }
            double dist = (dir & 2) ? (-pos) : (pos - minPos);
            if (dist > longestDist) {
                longestDist = dist;
                m_scrollX = dx;
                m_scrollY = dy;
            }
        }
    }
    #ifndef NDEBUG
        printf("scroll: direction %.0f,%.0f speed %.0f\n", m_scrollX, m_scrollY, m_scrollSpeed);
    #endif
    if (isScrolling()) { viewCfg("fx"); }
}

void PixelViewApp::updateScreenSize() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(m_window, &w, &h);
    m_screenWidth  = w;
    m_screenHeight = h;
}

void PixelViewApp::toggleFullscreen() {
    if (m_fullscreen) {
        // leave fullscreen mode -> restore old window settings
        glfwSetWindowMonitor(m_window, nullptr,
            m_windowGeometry.xpos,
            m_windowGeometry.ypos,
            m_windowGeometry.width,
            m_windowGeometry.height,
            GLFW_DONT_CARE);
        m_fullscreen = false;
    } else {
        // enter fullscreen mode -> save old window geometry and switch to FS
        glfwGetWindowSize(m_window, &m_windowGeometry.width, &m_windowGeometry.height);
        glfwGetWindowPos(m_window, &m_windowGeometry.xpos, &m_windowGeometry.ypos);
        GLFWmonitor *monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(m_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        m_fullscreen = true;
    }
    updateCursor();
    glfwSwapInterval(1);
    updateScreenSize();
    viewCfg("x");
}

void PixelViewApp::updateCursor(bool startTimeout) {
    if (!m_fullscreen || anyUIvisible()) {
        m_hideCursorAt = 0.0;
        m_cursorVisible = true;
    } else if (startTimeout) {
        m_cursorVisible = true;
        m_hideCursorAt = glfwGetTime() + cursorHideDelay;
    } else {
        m_cursorVisible = false;
    }
}

////////////////////////////////////////////////////////////////////////////////

void PixelViewApp::loadImage(const char* filename) {
    ::free((void*)m_fileName);
    m_fileName = StringUtil::copy(filename);
    if (!m_fileName) { unloadImage(); }
    loadImage();
}

void PixelViewApp::loadImage() {
    m_imgWidth = m_imgHeight = 0;
    m_viewWidth = m_viewHeight = 0.0;
    #ifndef NDEBUG
        printf("loading image: '%s'\n", m_fileName);
    #endif
    void* data = stbi_load(m_fileName, &m_imgWidth, &m_imgHeight, nullptr, 3);
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
    glGenerateMipmap(GL_TEXTURE_2D);
    GLutil::checkError("mipmap generation");
    glBindTexture(GL_TEXTURE_2D, 0);
    #ifndef NDEBUG
        printf("loaded image successfully (%dx%d pixels)\n", m_imgWidth, m_imgHeight);
    #endif
    m_aspect = 1.0;
    m_viewMode = vmFit;
    m_x0 = m_y0 = 0.0;
    viewCfg("xs");
}

void PixelViewApp::unloadImage() {
    m_imgWidth = m_imgHeight = 0;
    glBindTexture(GL_TEXTURE_2D, m_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glBindTexture(GL_TEXTURE_2D, 0);
}

////////////////////////////////////////////////////////////////////////////////

void PixelViewApp::updateView(bool usePivot, double pivotX, double pivotY) {
    if (!imgValid()) {
        return;  // no image loaded
    }

    // compute pivot position into relative coordinates
    double pivotRelX = (m_viewWidth  > 1.0) ? ((pivotX - m_x0) / m_viewWidth)  : 0.5;
    double pivotRelY = (m_viewHeight > 1.0) ? ((pivotY - m_y0) / m_viewHeight) : 0.5;

    // compute raw image size with aspect ratio correction
    double rawWidth  = m_imgWidth  * std::max(m_aspect, 1.0);
    double rawHeight = m_imgHeight / std::min(m_aspect, 1.0);
    bool isInt = wantIntegerZoom();
    bool autofit = (m_viewMode != vmFree);
    #ifdef DEBUG_UPDATE_VIEW
        printf("updateView: mode=%s int=%s imgSize=%dx%d rawSize=%.0fx%.0f screenSize=%.0fx%.0f ",
               (m_viewMode == vmFree) ? "free" : (m_viewMode == vmFill) ? "fill" : "fit ",
               isInt ? "yes" : "no ",
               m_imgWidth,m_imgHeight, rawWidth,rawHeight, m_screenWidth,m_screenHeight);
    #endif

    // perform auto-fit computations
    if (autofit) {
        // compute scaling factors in both directions;
        // in integer scaling mode, take the maximum crop into account
        double zoomX = m_screenWidth  / (isInt ? (rawWidth  * (1.0 - m_maxCrop)) : rawWidth);
        double zoomY = m_screenHeight / (isInt ? (rawHeight * (1.0 - m_maxCrop)) : rawHeight);
        // select the appropriate zoom factor
        if (m_viewMode == vmFill) { m_zoom = std::max(zoomX, zoomY); }
        else                      { m_zoom = std::min(zoomX, zoomY); }
    }

    // constrain minimum zoom (essentially like autofit, but don't respect
    // "fill to screen" mode and ignore the maximum crop; also, always allow
    // at least 1x zoom)
    m_minZoom = std::min(m_screenWidth / rawWidth, m_screenHeight / rawHeight);
    if (m_minZoom >= 1.0) {
        m_minZoom = 1.0;
    } else if (isInt) {
        m_minZoom = 1.0 / std::ceil(1.0 / m_minZoom);
    }
    m_zoom = std::max(m_zoom, m_minZoom);
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
    m_viewWidth  = rawWidth  * m_zoom;
    m_viewHeight = rawHeight * m_zoom;
    #ifdef DEBUG_UPDATE_VIEW
        printf("->%.3f viewSize=%.0fx%.0f offset=%.0f,%.0f", m_zoom, m_viewWidth,m_viewHeight, m_x0,m_y0);
    #endif

    // reconstruct image origin from pivot
    if (usePivot) {
        m_x0 = pivotX - pivotRelX * m_viewWidth;
        m_y0 = pivotY - pivotRelY * m_viewHeight;
    }

    // constrain image origin
    auto constrain = [&] (double &pos, double &minPos, double viewSize, double screenSize) {
        minPos = std::min(0.0, screenSize - viewSize);
        if (autofit || (minPos >= 0.0)) {
            pos = (screenSize - viewSize) * 0.5;
        } else {
            pos = std::min(0.0, std::max(minPos, pos));
        }
    };
    constrain(m_x0, m_minX0, m_viewWidth,  m_screenWidth);
    constrain(m_y0, m_minY0, m_viewHeight, m_screenHeight);
    #ifdef DEBUG_UPDATE_VIEW
        printf("->%.0f,%.0f (min=%.0f,%.0f)\n", m_x0,m_y0, m_minX0,m_minY0);
    #endif

    // convert into transform matrix
    m_targetArea.m[0] =  2.0 * (m_viewWidth      / m_screenWidth);
    m_targetArea.m[1] = -2.0 * (m_viewHeight     / m_screenHeight);
    m_targetArea.m[2] =  2.0 * (std::floor(m_x0) / m_screenWidth)  - 1.0;
    m_targetArea.m[3] = -2.0 * (std::floor(m_y0) / m_screenHeight) + 1.0;
}

////////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {
    static PixelViewApp app;
    return app.run(argc, argv);
}
