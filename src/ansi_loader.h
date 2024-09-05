// SPDX-FileCopyrightText: 2024 Martin J. Fiedler <keyj@emphy.de>
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>

namespace ANSI {

//! ANSI rendering options
struct RenderOptions {
    enum class Mode : uint8_t { //!< rendering modes
        Normal        = 0,      //!< normal mode, no special handling
        CED           = 1,      //!< CED mode (black on gray, forced 78 columns)
        Workbench     = 3,      //!< Amiga Workbench palette
    };

    bool   vga9col    = false;  //!< output 9-pixel wide fonts, like VGA
    bool   iCEcolors  = true;   //!< use iCE colors (= allow bright background)
    int    font       = 0;      //!< ansilove internal font ID
    int    columns    = 0;      //!< number of columns (default: auto-detect)
    Mode mode = Mode::Normal;   //!< rendering mode

    inline RenderOptions() = default;
};

struct FontListEntry {  //!< one entry in the ansilove font list
    int font;           //!< font ID (negative = end of list)
    char name[60];      //!< human-readable name
};
extern const FontListEntry fontList[];

//! list of extension codes (as used in string_util.h) for all recognized file types
extern const uint32_t fileExts[];

//! render an ANSI file into a 32-bit image
void* render(RenderOptions& options, const char* filename, int &width, int &height);

}  // namespace ANSI
