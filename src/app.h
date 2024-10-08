#pragma once

#include <cstdint>

#include <vector>
#include <functional>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include "gl_util.h"
#include "gl_header.h"
#include "imgui.h"

#include "ansi_loader.h"

class PixelViewApp {
    // GLFW and ImGui stuff
    GLFWwindow* m_window = nullptr;
    ImGuiIO* m_io = nullptr;

    // rendering stuff
    GLuint m_tex = 0;
    GLutil::Program m_prog;
    GLint m_locArea;
    GLint m_locSize;

    // UI state
    bool m_fullscreen = false;
    bool m_escapePressed = false;
    bool m_active = true;
    bool m_animate = false;
    bool m_showHelp = false;
    bool m_showConfig = false;
    bool m_showInfo = false;
    bool m_showDemo = false;
    inline bool anyUIvisible() const { return m_showHelp || m_showConfig || m_showDemo; }
    int m_imgWidth = 0;
    int m_imgHeight = 0;
    bool m_panning = false;
    double m_panX = 0.0;
    double m_panY = 0.0;
    bool m_cursorVisible = true;
    double m_hideCursorAt = 0.0;
    enum StatusType { stNone = 0, stSuccess, stError };
    StatusType m_statusType;
    const char* m_statusMessage;
    bool m_statusMsgAlloc = false;
    char* m_fileName = nullptr;
    char* m_infoStr = nullptr;
    bool m_isANSI = false;

    // image view settings
    enum ViewMode {
        vmFree = 0,  //!< free pan/zoom
        vmFit,       //!< auto-fit to the screen, with letterbox/pillarbox
        vmFill,      //!< fill the whole screen, truncate if necessary
        vmPanel,     //!< panel mode: split into strips
    };
    ViewMode m_viewMode = vmFit;
    ViewMode m_prevViewMode = vmFit;
    bool m_integer = false;
    double m_maxCrop = 0.0;
    double m_aspect = 1.0;
    double m_zoom = 1.0;
    double m_x0 = 0.0;
    double m_y0 = 0.0;
    double m_scrollSpeed = 4.0;
    ANSILoader m_ansi;

    // image view state
    double m_screenWidth  = 0.0;
    double m_screenHeight = 0.0;
    double m_viewWidth  = 0.0;
    double m_viewHeight = 0.0;
    double m_minX0 = 0.0;
    double m_minY0 = 0.0;
    double m_scrollX = 0.0;
    double m_scrollY = 0.0;
    double m_minZoom = 1.0/16;
    struct Area { double m[4]; };
    Area m_currentArea = {{2.0, -2.0, -1.0, 1.0}};
    Area m_targetArea = {{2.0, -2.0, -1.0, 1.0}};
    std::vector <Area> m_panelAreas;
    inline bool isZoomed() const { return (m_zoom < 0.9999) || (m_zoom > 1.0001); }
    inline bool isSquarePixels() const { return (m_aspect >= 0.9999) && (m_aspect <= 1.0001); }
    inline bool canDoIntegerZoom() const { return isSquarePixels() && (m_viewMode != vmPanel); }
    inline bool canUsePanelMode() const { return !m_panelAreas.empty(); }
    inline bool wantIntegerZoom() const { return m_integer && canDoIntegerZoom(); }
    inline bool isScrolling() const { return (m_scrollX != 0.0) || (m_scrollY != 0.0); }
    struct WindowGeometry {
        int xpos,  ypos;
        int width, height;
    } m_windowGeometry;

    // main functions
    inline bool imgValid() const { return (m_imgWidth > 0) && (m_imgHeight > 0); }
    void loadSibling(bool absolute, int order);
    void loadImage(const char* filename);
    void loadImage(bool soft=false);
    void loadConfig(const char* filename, double &relX, double &relY);
    void saveConfig();
    bool saveConfig(const char* filename);
    void unloadImage();
    void updateInfo();
    void updateView(bool usePivot, double pivotX, double pivotY);
    void setArea(Area& a, double x0, double y0, double vw, double vh);
    void computePanelGeometry();
    inline void updateView(bool usePivot=true) { updateView(usePivot, m_screenWidth * 0.5, m_screenHeight * 0.5); }
    void cursorPan(double dx, double dy, int mods);
    void cycleViewMode(bool with1x);
    void cycleTopView();
    void changeZoom(double direction, double pivotX, double pivotY);
    inline void changeZoom(double direction) { changeZoom(direction, m_screenWidth * 0.5, m_screenHeight * 0.5); }
    void startScroll(double speed, double dx, double dy);
    inline void startScroll() { startScroll(0.0, 0.0, 0.0); }
    inline void startScroll(double speed) { startScroll(speed, 0.0, 0.0); }
    inline void startScroll(double dx, double dy) { startScroll(0.0, dx, dy); }
    void updateScreenSize();
    void toggleFullscreen();
    void updateCursor(bool startTimeout=false);
    enum StatusMessageType { mtConst, mtCopy, mtSteal };
    void setStatus(StatusType st, StatusMessageType mt, const char* message);
    void setFileStatus(StatusType st, const char* message);
    inline void clearStatus() { setStatus(stNone, mtConst, nullptr); }

    // "universal" view configuration helper function to save some typing;
    // string-controlled:
    // - 'f' = set view mode to "free pan/zoom" (vmFree)
    // - 'p' = enter panel mode (view mode = vmPanel)
    // - 'a' = enable animation
    // - 'x' = disable animation
    // - 's' = stop scrolling
    // - 'n' = do NOT call updateView() at the end (by default, it will call it)
    void viewCfg(const char* actions);

    // UI functions
    void uiHelpWindow();
    void uiConfigWindow();
    void uiStatusWindow();
    void uiInfoWindow();

    // event handling
    void handleKeyEvent(int key, int scancode, int action, int mods);
    void handleMouseButtonEvent(int button, int action, int mods);
    void handleCursorPosEvent(double xpos, double ypos);
    void handleScrollEvent(double xoffset, double yoffset);
    void handleDropEvent(int path_count, const char* paths[]);
    void handleResizeEvent(int width, int height);

public:
    inline PixelViewApp() {}
    int run(int argc, char* argv[]);
};
