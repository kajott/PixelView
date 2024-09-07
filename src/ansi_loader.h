// SPDX-FileCopyrightText: 2024 Martin J. Fiedler <keyj@emphy.de>
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <cstdio>

//! ANSI loader / renderer class
class ANSILoader {

public:  // types

    //! rendering mode fpr ANSI files
    enum class RenderMode : uint8_t {
        Normal         = 0,      //!< normal mode, no special handling
        CED            = 1,      //!< CED mode (black on gray, forced 78 columns)
        Workbench      = 3,      //!< Amiga Workbench palette
    };

    //! rendering options
    struct RenderOptions {
        bool   tabs2spaces = false;  //!< convert tabs to spaces (.ans only)
        bool   useSAUCE    = true;   //!< use SAUCE record for configuration, if available
        bool   vga9col     = false;  //!< output 9-pixel wide fonts, like VGA
        bool   aspectCorr  = false;  //!< correct aspect ratio in vga9col mode
        bool   iCEcolors   = true;   //!< use iCE colors (= allow bright background)
        int    font        = 0;      //!< ansilove internal font ID
        bool   autoColumns = true;   //!< set number of columns automatically
        int    columns     = 80;     //!< number of columns (default: auto-detect)
        RenderMode mode = RenderMode::Normal;  //!< rendering mode
        inline RenderOptions() = default;
    };

    //! result code for setOption()
    enum class SetOptionResult {
        OK = 0,         //!< everything is fine
        UnknownOption,  //!< unknown option name
        OutOfRange,     //!< value is out of range
    };

public:  // directly accessible member variables

    //! default rendering options
    static const RenderOptions defaults;

    //! rendering options
    RenderOptions options;

    // metadata about the last rendered file
    double aspect   = 1.0;    //!< expected aspect ratio
    bool   hasSAUCE = false;  //!< SAUCE configuration information is available

public:  // type and font registry

    struct FontListEntry {  //!< one entry in the ansilove font list
        int font;           //!< font ID (negative = end of list)
        char name[60];      //!< human-readable name
    };
    static const FontListEntry fontList[];

    //! list of extension codes (as used in string_util.h) for all recognized file types
    static const uint32_t fileExts[];

public:  // methods

    inline ANSILoader() = default;

    //! reset options to defaults
    inline void loadDefaults() { options = defaults; }

    //! render an ANSI file into a 32-bit image
    void* render(const char* filename, int &width, int &height);

    //! run the UI for the ANSI options; return true if reloading is required
    bool ui();

    //! save configuration into a config file
    void saveConfig(FILE* f);

    //! set a single configuration item
    SetOptionResult setOption(const char* name, int value);

private:
    const char* parseSAUCE(char* data, int size);
};
