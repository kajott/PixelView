// SPDX-FileCopyrightText: 2021-2022 Martin J. Fiedler <keyj@emphy.de>
// SPDX-License-Identifier: MIT

#ifdef _MSC_VER
    #define _CRT_SECURE_NO_WARNINGS  // prevent MSVC warnings
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>

#include <algorithm>

#include "string_util.h"

#include "app.h"

////////////////////////////////////////////////////////////////////////////////

void PixelViewApp::loadConfig(const char* filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        #ifndef NDEBUG
            printf("could not open config file '%s'\n", filename);
        #endif
        return;
    }
    double relX = -1.0, relY = -1.0;

    constexpr int maxLineLen = 80;
    char line[maxLineLen];
    bool isNewLine = true;
    while (fgets(line, maxLineLen, f)) {
        if (!*line) { break; /* EOF */ }
        int lineLen = int(strlen(line));
        bool isValidLine = isNewLine;
        isNewLine = (line[lineLen - 1] == '\n');
        if (!isValidLine) { continue; /* tail end of a split line */ }

        // prepare line for parsing: strip comments and whitespace
        char *key = strchr(line, '#');
        if (key) { *key = '\0'; }
        key = StringUtil::skipWhitespace(line);
        StringUtil::trimTrailingWhitespace(key);
        if (!*key) { continue; /* empty or comment line */ }
        StringUtil::stringToLower(key);
        // /*DEBUG*/ printf("config line: raw='%s'\n", key);

        // split into key/value pair
        int sepPos = int(strcspn(key, " \t\r\f\v"));
        if (!sepPos) { sepPos = int(strlen(key)); }
        char *value = StringUtil::skipWhitespace(&key[sepPos]);
        key[sepPos] = '\0';
        char *end = nullptr;
        double fval = ::strtod(value, &end);
        bool isFloat = (end && (*end == '\0'));
        // /*DEBUG*/ printf("config line: key='%s' value='%s' isfloat=%s fval=%g\n", key, value, isFloat?"yes":"no", fval);

        // some helper functions for parsing
        auto invalidValue = [&] () {
            #ifndef NDEBUG
                printf("config file error: invalid enumeration value '%s' for key '%s'\n", value, key);
            #endif
        };
        auto needFloat = [&] (double vmin, double vmax) -> bool {
            if (!isFloat) {
                #ifndef NDEBUG
                    printf("config file error: invalid numerical value '%s' for key '%s'\n", value, key);
                #endif
                *key = '\0';  // avoid additional "invalid key" error
                return false;
            }
            if ((fval < vmin) || (fval > vmax)) {
                #ifndef NDEBUG
                    printf("config file error: numerical value %g ('%s') for key '%s' out of range (%g...%g)\n", fval, value, key, vmin, vmax);
                #endif
                *key = '\0';  // avoid additional "invalid key" error
                return false;
            }
            return true;
        };

        // parse the actual line
        if (!strcmp(key, "mode")) {
                 if (!strcmp(value, "free")) { m_viewMode = vmFree; }
            else if (!strcmp(value, "fit"))  { m_viewMode = vmFit;  }
            else if (!strcmp(value, "fill")) { m_viewMode = vmFill; }
            else                             { invalidValue();      }
        }
        if (!strcmp(key, "integer")) {
                 if (!strcmp(value, "yes") || !strcmp(value, "true")  || !strcmp(value, "on")  || (isFloat && (fval != 0.0))) { m_integer = true;  }
            else if (!strcmp(value, "no")  || !strcmp(value, "false") || !strcmp(value, "off") || (isFloat && (fval == 0.0))) { m_integer = false; }
            else                                                                                                              { invalidValue();    }
        }
        else if (!strcmp(key, "aspect")      && needFloat(1E-2, 1E+2)) { m_aspect      = fval; }
        else if (!strcmp(key, "maxcrop")     && needFloat(0.0,  99.9)) { m_maxCrop     = fval * 0.01; }
        else if (!strcmp(key, "zoom")        && needFloat(1E-6, 1E+6)) { m_zoom        = fval; }
        else if (!strcmp(key, "relx")        && needFloat(0.0, 100.0)) {   relX        = fval * 0.01; }
        else if (!strcmp(key, "rely")        && needFloat(0.0, 100.0)) {   relY        = fval * 0.01; }
        else if (!strcmp(key, "scrollspeed") && needFloat(0.0, 1E+10)) { m_scrollSpeed = fval; }
        else if (*key) {
            #ifndef NDEBUG
                printf("config file error: unrecognized key '%s'\n", key);
            #endif
        }
    }
    fclose(f);
    #ifndef NDEBUG
        printf("loaded configuration from file '%s'\n", filename);
    #endif

    // convert relX/relY into X0/Y0, if needed
    if (m_viewMode == vmFree) {
        updateView();  // required to set minX0/minY0
        m_x0 = relX * m_minX0;
        m_y0 = relY * m_minY0;
    }
}

////////////////////////////////////////////////////////////////////////////////

bool PixelViewApp::saveConfig(const char* filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        #ifndef NDEBUG
            printf("saving config file '%s' FAILED.\n", filename);
        #endif
        return false;
    }
    fprintf(f, "# PixelView display configuration file\n");
    if (!isSquarePixels()) {
        fprintf(f, "aspect %g\n", m_aspect);
    }
    if ((m_maxCrop > 0.0) || ((m_viewMode == vmFill) && m_integer)) {
        fprintf(f, "maxcrop %.0f\n", m_maxCrop * 100.0);
    }
    fprintf(f, "mode %s\n", (m_viewMode == vmFree) ? "free" : (m_viewMode == vmFill) ? "fill" : "fit");
    if (canDoIntegerZoom()) {
        fprintf(f, "integer %s\n", m_integer ? "yes" : "no");
    }
    if (m_viewMode == vmFree) {
        fprintf(f, "zoom %g\n", m_zoom);
        fprintf(f, "relx %.1f\n", std::min(100.0, std::max(0.0, (m_minX0 >= 0.0) ? 50.0 : (100.0 * m_x0 / m_minX0))));
        fprintf(f, "rely %.1f\n", std::min(100.0, std::max(0.0, (m_minY0 >= 0.0) ? 50.0 : (100.0 * m_y0 / m_minY0))));
    }
    fprintf(f, "scrollspeed %.0f\n", m_scrollSpeed);
    fclose(f);
    #ifndef NDEBUG
        printf("saved configuration into file '%s'\n", filename);
    #endif
    return true;
}

////////////////////////////////////////////////////////////////////////////////
