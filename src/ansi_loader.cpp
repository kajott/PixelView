// SPDX-FileCopyrightText: 2024 Martin J. Fiedler <keyj@emphy.de>
// SPDX-License-Identifier: MIT

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include <algorithm>

#include "imgui.h"
#include "ansilove.h"

#include "string_util.h"
#include "gd.h"

#include "ansi_loader.h"

///////////////////////////////////////////////////////////////////////////////
// MARK: registry
///////////////////////////////////////////////////////////////////////////////

constexpr int binaryExtOffset = 2;
const uint32_t ANSILoader::fileExts[] = {
    // first classic ANSI file extensions
    StringUtil::makeExtCode("asc"),
    StringUtil::makeExtCode("ans"),
    // *then* the special binary formats
    StringUtil::makeExtCode("adf"),
    StringUtil::makeExtCode("bin"),
    StringUtil::makeExtCode("idf"),
    StringUtil::makeExtCode("pcb"),
    StringUtil::makeExtCode("tnd"),
    StringUtil::makeExtCode("xb"),
    0
};

const ANSILoader::FontListEntry ANSILoader::fontList[] = {
    { 0,                              "Default" },
    { ANSILOVE_FONT_TOPAZ,            "Amiga Topaz 1200" },
    { ANSILOVE_FONT_TOPAZ_PLUS,       "Amiga Topaz+ 1200" },
    { ANSILOVE_FONT_TOPAZ500,         "Amiga Topaz 500" },
    { ANSILOVE_FONT_TOPAZ500_PLUS,    "Amiga Topaz+ 500" },
    { ANSILOVE_FONT_MICROKNIGHT,      "Microknight" },
    { ANSILOVE_FONT_MICROKNIGHT_PLUS, "Microknight+" },
    { ANSILOVE_FONT_MOSOUL,           "mO'sOul" },
    { ANSILOVE_FONT_POT_NOODLE,       "P0T-NOoDLE" },
    { ANSILOVE_FONT_TERMINUS,         "Terminus (cp437)" },
    { ANSILOVE_FONT_SPLEEN,           "Spleen (cp437)" },
    { ANSILOVE_FONT_CP437,            "IBM PC 80x25 (cp437)" },
    { ANSILOVE_FONT_CP437_80x50,      "IBM PC 80x50 (cp437)" },
    { ANSILOVE_FONT_CP737,            "IBM PC 80x25 (cp737 - Greek)" },
    { ANSILOVE_FONT_CP775,            "IBM PC 80x25 (cp775 - Baltic)" },
    { ANSILOVE_FONT_CP850,            "IBM PC 80x25 (cp850 - Latin 1)" },
    { ANSILOVE_FONT_CP852,            "IBM PC 80x25 (cp852 - Latin 2)" },
    { ANSILOVE_FONT_CP855,            "IBM PC 80x25 (cp855 - Cyrillic)" },
    { ANSILOVE_FONT_CP857,            "IBM PC 80x25 (cp857 - Turkish)" },
    { ANSILOVE_FONT_CP860,            "IBM PC 80x25 (cp860 - Portuguese)" },
    { ANSILOVE_FONT_CP861,            "IBM PC 80x25 (cp861 - Icelandic)" },
    { ANSILOVE_FONT_CP862,            "IBM PC 80x25 (cp862 - Hebrew)" },
    { ANSILOVE_FONT_CP863,            "IBM PC 80x25 (cp863 - French-Canadian)" },
    { ANSILOVE_FONT_CP865,            "IBM PC 80x25 (cp865 - Nordic)" },
    { ANSILOVE_FONT_CP866,            "IBM PC 80x25 (cp866 - Russian)" },
    { ANSILOVE_FONT_CP869,            "IBM PC 80x25 (cp869 - Greek)" },
    { -1, "" },
};

///////////////////////////////////////////////////////////////////////////////
// MARK: render
///////////////////////////////////////////////////////////////////////////////

void* ANSILoader::render(const char* filename, int &width, int &height) {
    // ansilove context initialization (equivalent to ansilove_init())
    struct ansilove_ctx     ctx;
    struct ansilove_options opt;
    ::memset(static_cast<void*>(&ctx), 0, sizeof(ctx));
    ::memset(static_cast<void*>(&opt), 0, sizeof(opt));
    opt.truecolor = true;
    opt.bits      = options.vga9col ? 9 : 8;
    opt.icecolors = options.iCEcolors;
    opt.font      = static_cast<uint8_t>(options.font);
    opt.columns   = options.autoColumns ? 0 : static_cast<int16_t>(options.columns);
    opt.mode      = static_cast<uint8_t>(options.mode);
    aspect = (options.vga9col && options.aspectCorr) ? (8.0 / 9.0) : 1.0;

    // load the source file
    int size = 0;
    ctx.buffer = reinterpret_cast<uint8_t*>(StringUtil::loadTextFile(filename, size));
    if (!ctx.buffer) { return nullptr; }
    ctx.maplen = ctx.length = static_cast<size_t>(size);
    uint32_t ext = StringUtil::extractExtCode(filename);

    // process tabs-to-spaces
    if (options.tabs2spaces && !StringUtil::checkExt(ext, &fileExts[binaryExtOffset])) {
        uint8_t* pos = ctx.buffer;
        for (size_t cnt = ctx.length;  cnt;  --cnt, ++pos) {
            if (*pos == 26) { break; }  // don't damage the SAUCE record
            if (*pos == 9) { *pos = 32; }
        }
    }

    // run the actual ansilove renderer
    int res;
    switch (ext) {
        case StringUtil::makeExtCode("adf"): res = ansilove_artworx(&ctx, &opt); break;
        case StringUtil::makeExtCode("bin"): res = ansilove_binary (&ctx, &opt); break;
        case StringUtil::makeExtCode("idf"): res = ansilove_icedraw(&ctx, &opt); break;
        case StringUtil::makeExtCode("pcb"): res = ansilove_pcboard(&ctx, &opt); break;
        case StringUtil::makeExtCode("tnd"): res = ansilove_tundra (&ctx, &opt); break;
        case StringUtil::makeExtCode("xb"):  res = ansilove_xbin   (&ctx, &opt); break;
        default:                             res = ansilove_ansi   (&ctx, &opt); break;
    }
    #ifndef NDEBUG
        printf("ansilove renderer returned %d, error code %d\n", res, ctx.error);
    #else
        (void)res;
    #endif

    // free data, determine width and height
    ::free(static_cast<void*>(ctx.buffer));
    width  = ctx.png.length & 0xFFFF;
    height = ctx.png.length >> 16;

    // done!
    return ctx.png.buffer;
}

///////////////////////////////////////////////////////////////////////////////
// MARK: UI
///////////////////////////////////////////////////////////////////////////////

bool ANSILoader::ui() {
    bool changed = false;

    const char* currentFont = fontList[0].name;
    for (const FontListEntry* f = fontList;  f->font >= 0;  ++f) {
        if (f->font == options.font) {
            currentFont = f->name;
            break;
        }
    }
    if (ImGui::BeginCombo("font", currentFont)) {
        for (const FontListEntry* f = fontList;  f->font >= 0;  ++f) {
            bool isCurrent = (f->font == options.font);
            if (ImGui::Selectable(f->name, isCurrent)) {
                options.font = f->font;
                changed = true;
            }
            if (isCurrent) { ImGui::SetItemDefaultFocus(); }
        }
        ImGui::EndCombo();
    }

    if (ImGui::Checkbox("9-pixel wide fonts (VGA)", &options.vga9col)) { changed = true; }
    ImGui::SameLine();
    if (!options.vga9col) { ImGui::BeginDisabled(); }
    if (ImGui::Checkbox("aspect ratio correction", &options.aspectCorr)) { changed = true; }
    if (!options.vga9col) { ImGui::EndDisabled(); }
    if (ImGui::Checkbox("iCE colors", &options.iCEcolors)) { changed = true; }
    if (ImGui::Checkbox("interpret tabs as spaces", &options.tabs2spaces)) { changed = true; }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("columns:");
    ImGui::SameLine(); ImGui::SetNextItemWidth(100.0);
    if (options.autoColumns) { ImGui::BeginDisabled(); }
    if (ImGui::InputInt("##colEntry", &options.columns, 1, 10)) { changed = true; }
    if (options.autoColumns) { ImGui::EndDisabled(); }
    ImGui::SameLine(); 
    if (ImGui::Checkbox("auto", &options.autoColumns)) { changed = true; }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("ANSI render mode:");
    ImGui::SameLine(); if (ImGui::RadioButton("normal",    (options.mode == RenderMode::Normal)))    { options.mode = RenderMode::Normal;    changed = true; }
    ImGui::SameLine(); if (ImGui::RadioButton("CED",       (options.mode == RenderMode::CED)))       { options.mode = RenderMode::CED;       changed = true; }
    ImGui::SameLine(); if (ImGui::RadioButton("Workbench", (options.mode == RenderMode::Workbench))) { options.mode = RenderMode::Workbench; changed = true; }

    return changed;
}

///////////////////////////////////////////////////////////////////////////////
// MARK: config I/O
///////////////////////////////////////////////////////////////////////////////

void ANSILoader::saveConfig(FILE* f) {
    assert(f != nullptr);
    fprintf(f, "ansi_tabs2spaces %d\n", options.tabs2spaces ? 1 : 0);
    fprintf(f, "ansi_vga9col %d\n",     options.vga9col     ? 1 : 0);
    fprintf(f, "ansi_aspect %d\n",      options.aspectCorr  ? 1 : 0);
    fprintf(f, "ansi_icecolors %d\n",   options.iCEcolors   ? 1 : 0);
    fprintf(f, "ansi_font %d\n",        options.font);
    fprintf(f, "ansi_columns %d\n",     options.autoColumns ? 0 : options.columns);
    fprintf(f, "ansi_mode %d\n",        static_cast<uint8_t>(options.mode));
}

ANSILoader::SetOptionResult ANSILoader::setOption(const char* name, int value) {
    if (!name) { return SetOptionResult::UnknownOption; }
    #define HANDLE_OPTION(oname, vmin, vmax) \
                if (!strcmp(name, oname)) { \
                    if ((value < vmin) || (value > vmax)) { return SetOptionResult::OutOfRange; }
    #define END_OPTION return SetOptionResult::OK; }
    HANDLE_OPTION("tabs2spaces", 0,   1) options.tabs2spaces = !!value; END_OPTION
    HANDLE_OPTION("vga9col",     0,   1) options.vga9col     = !!value; END_OPTION
    HANDLE_OPTION("aspect",      0,   1) options.aspectCorr  = !!value; END_OPTION
    HANDLE_OPTION("icecolors",   0,   1) options.iCEcolors   = !!value; END_OPTION
    HANDLE_OPTION("font",        0, 255) options.font        =   value; END_OPTION
    HANDLE_OPTION("columns",     0, 255) options.autoColumns =  !value;
                            if (value) { options.columns     =   value; } END_OPTION
    HANDLE_OPTION("mode",        0,   3) options.mode = static_cast<RenderMode>(value); END_OPTION
    return SetOptionResult::UnknownOption;
}

///////////////////////////////////////////////////////////////////////////////
// MARK: GD stubs
///////////////////////////////////////////////////////////////////////////////

extern "C" gdImagePtr gdImageCreateTrueColor(int sx, int sy) {
    gdImagePtr im = static_cast<gdImagePtr>(::malloc(sizeof(gdImage)));
    if (!im) { return nullptr; }
    im->sx = sx;
    im->sy = sy;
    im->data = static_cast<int*>(::malloc(sx * sy * sizeof(int)));
    gdImageFill(im, 0, 0, 0xFF000000);
    return im;
}

extern "C" void gdImageDestroy(gdImagePtr im) {
    if (!im) { return; }
    if (im->data) { ::free(static_cast<void*>(im->data)); im->data = nullptr; }
    im->sx = im->sy = 0;
    ::free(static_cast<void*>(im));
}

extern "C" int gdImageColorAllocate(gdImagePtr im, int r, int g, int b) {
    (void)im;
    return r | (g << 8) | (b << 16) | 0xFF000000;
}

extern "C" void gdImageColorTransparent(gdImagePtr im, int color) {
    (void)im, (void)color;
}

extern "C" void gdImageFill(gdImagePtr im, int x, int y, int nc) {
    (void)x, (void)y;
    if (!im) { return; }
    int *p = im->data;
    for (int n = im->sx * im->sy;  n;  --n) {
        *p++ = nc;
    }
}

extern "C" void gdImageFilledRectangle(gdImagePtr im, int x1, int y1, int x2, int y2, int color) {
    if (!im) { return; }
    x2 = std::min(x2 + 1, im->sx);
    y2 = std::min(y2 + 1, im->sy);
    int *line = &im->data[im->sx * y1];
    for (int y = y1;  y < y2;  ++y) {
        for (int x = x1;  x < x2;  ++x) {
            line[x] = color;
        }
        line += im->sx;
    }
}

extern "C" void gdImageSetPixel(gdImagePtr im, int x, int y, int color) {
    if (im) { im->data[im->sx * y + x] = color; }
}

extern "C" void gdImageCopyResized(gdImagePtr dst, gdImagePtr src, int dstX, int dstY, int srcX, int srcY, int dstW, int dstH, int srcW, int srcH) {
    (void)dst, (void)src, (void)dstX, (void)dstY, (void)srcX, (void)srcY, (void)dstW, (void)dstH, (void)srcW, (void)srcH;
}

extern "C" void gdImageCopyResampled(gdImagePtr dst, gdImagePtr src, int dstX, int dstY, int srcX, int srcY, int dstW, int dstH, int srcW, int srcH) {
    (void)dst, (void)src, (void)dstX, (void)dstY, (void)srcX, (void)srcY, (void)dstW, (void)dstH, (void)srcW, (void)srcH;
}

extern "C" void* gdImagePngPtr(gdImagePtr im, int *size) {
    // don't actually encode a .png here -- we just steal the data pointer
    // and encode the image dimensions in the size member
    *size = im->sx | (im->sy << 16);
    auto res = im->data;
    im->data = nullptr;
    return static_cast<void*>(res);
}

void gdFree(void* ptr) {
    free(ptr);
}
