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

const ANSILoader::RenderOptions ANSILoader::defaults;

int ANSILoader::maxSize = 65535;

constexpr int binaryExtOffset = 2;
const uint32_t ANSILoader::fileExts[] = {
    // first classic ANSI file extensions
    StringUtil::makeExtCode("asc"),
    StringUtil::makeExtCode("ans"),
    StringUtil::makeExtCode("nfo"),
    StringUtil::makeExtCode("diz"),
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

    // load the source file
    uint32_t ext = StringUtil::extractExtCode(filename);
    int size = 0;
    char* data = StringUtil::loadTextFile(filename, size);
    if (!data) { hasSAUCE = false; return nullptr; }
    ctx.buffer = reinterpret_cast<uint8_t*>(data);
    ctx.maplen = ctx.length = static_cast<size_t>(size);

    // process tabs-to-spaces and SAUCE
    if (options.tabs2spaces && !StringUtil::checkExt(ext, &fileExts[binaryExtOffset])) {
        uint8_t* pos = ctx.buffer;
        for (size_t cnt = ctx.length;  cnt;  --cnt, ++pos) {
            if (*pos == 26) { break; }  // don't damage the SAUCE record
            if (*pos == 9) { *pos = 32; }
        }
    }
    auto sauceStatus = parseSAUCE(reinterpret_cast<char*>(ctx.buffer), size);
    #ifndef NDEBUG
        printf("SAUCE record status: %s\n", sauceStatus);
    #else
        (void)sauceStatus;
    #endif

    // set ansilove rendering options
    opt.truecolor = true;
    opt.bits      = options.vga9col ? 9 : 8;
    opt.icecolors = options.iCEcolors;
    opt.font      = static_cast<uint8_t>(options.font);
    opt.columns   = options.autoColumns ? 0 : static_cast<int16_t>(options.columns);
    opt.mode      = static_cast<uint8_t>(options.mode);
    aspect = !options.aspectCorr ? 1.0
           :  options.vga9col    ? (20.0 / 27.0)
                                 : ( 5.0 /  6.0);

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
    width  =  ctx.png.length        & 0xFFFF;
    height = (ctx.png.length >> 16) & 0xFFFF;

    // done!
    return ctx.png.buffer;
}

///////////////////////////////////////////////////////////////////////////////
// MARK: UI
///////////////////////////////////////////////////////////////////////////////

bool ANSILoader::ui() {
    bool changed = false;

    if (ImGui::Checkbox("interpret tabs as spaces", &options.tabs2spaces)) { changed = true; }

    if (ImGui::Checkbox("auto-configure using SAUCE record", &options.useSAUCE)) {
        if (options.useSAUCE) { changed = true; }
    }
    ImGui::SameLine(ImGui::GetWindowWidth() - 25.f);
    ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.0f, 0.7f, 0.0f, 1.0f));
    ImGui::RadioButton("##hasSAUCE", hasSAUCE);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip(hasSAUCE ? "file has a valid SAUCE record" : "file does not have a valid SAUCE record");
    }
    ImGui::PopStyleColor();
    ImGui::BeginDisabled(hasSAUCE && options.useSAUCE);

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

    if (ImGui::Checkbox("9-pixel wide fonts", &options.vga9col)) { changed = true; }
    if (ImGui::Checkbox("aspect ratio correction", &options.aspectCorr)) { changed = true; }
    if (ImGui::Checkbox("iCE colors", &options.iCEcolors)) { changed = true; }

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("columns:");
    ImGui::SameLine(); ImGui::SetNextItemWidth(100.0);
    ImGui::BeginDisabled(options.autoColumns);
    if (ImGui::InputInt("##colEntry", &options.columns, 1, 10)) { changed = true; }
    ImGui::EndDisabled();
    ImGui::SameLine(); 
    if (ImGui::Checkbox("auto", &options.autoColumns)) { changed = true; }

    ImGui::EndDisabled();  // useSAUCE

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("ANSI rendering mode:");
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
    fprintf(f, "ansi_use_sauce %d\n",   options.useSAUCE    ? 1 : 0);
    if (!options.useSAUCE || !hasSAUCE) {
        fprintf(f, "ansi_vga9col %d\n",     options.vga9col     ? 1 : 0);
        fprintf(f, "ansi_aspect %d\n",      options.aspectCorr  ? 1 : 0);
        fprintf(f, "ansi_icecolors %d\n",   options.iCEcolors   ? 1 : 0);
        fprintf(f, "ansi_font %d\n",        options.font);
        fprintf(f, "ansi_columns %d\n",     options.autoColumns ? 0 : options.columns);
    }
    fprintf(f, "ansi_mode %d\n",        static_cast<uint8_t>(options.mode));
}

ANSILoader::SetOptionResult ANSILoader::setOption(const char* name, int value) {
    if (!name) { return SetOptionResult::UnknownOption; }
    #define HANDLE_OPTION(oname, vmin, vmax) \
                if (!strcmp(name, oname)) { \
                    if ((value < vmin) || (value > vmax)) { return SetOptionResult::OutOfRange; }
    #define END_OPTION return SetOptionResult::OK; }
    HANDLE_OPTION("tabs2spaces", 0,   1) options.tabs2spaces = !!value; END_OPTION
    HANDLE_OPTION("use_sauce",   0,   1) options.useSAUCE    = !!value; END_OPTION
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
// MARK: SAUCE parser
///////////////////////////////////////////////////////////////////////////////

const char* ANSILoader::parseSAUCE(char* data, int size) {
    // initial sanity checks
    hasSAUCE = false;
    if (size < 128) { return "file too small"; }
    data = &data[size - 128];
    if (strncmp(data, "SAUCE", 5)) { return "no SAUCE header"; }

    // extract relevant header fields
    uint8_t dataType = static_cast<uint8_t>(data[ 94]);
    uint8_t fileType = static_cast<uint8_t>(data[ 95]);
    uint8_t tInfo1   = static_cast<uint8_t>(data[ 96]);
    uint8_t tFlags   = static_cast<uint8_t>(data[105]);
    data += 106;  // move to TInfoS; guaranteed to be null-terminated because we loaded the entire file with loadTextFile()
    #ifndef NDEBUG
        printf("SAUCE: DataType=%d FileType=%d TInfo1=%d TFlags=0x%02X TInfoS='%s'\n", dataType, fileType, tInfo1, tFlags, data);
    #endif

    // check data and file types; extract column count
    int columns = 0;
    switch (dataType) {
        case 1:  // Character
                 switch (fileType) {
                    case 0:  // ASCII
                    case 1:  // ANSi
                    case 2:  // ANSiMation
                        columns = tInfo1;
                        break;
                    default: return "unsupported FileType";
                 }
                 break;
        case 5:  // BinaryText
                 columns = 2 * fileType;
                 break;
        default: return "unsupported DataType";
    }

    // at this point, we know that we have a proper and supported SAUCE at hand
    hasSAUCE = true;
    if (!options.useSAUCE) { return "valid, but ignored"; }

    // copy basic data into the options structure
    options.autoColumns = !columns;
    if (columns) { options.columns = columns; }
    options.iCEcolors = ((tFlags & 1) == 1);
    switch ((tFlags >> 1) & 3) {
        case 1: options.vga9col = false; break;
        case 2: options.vga9col = true;  break;
        default: break;
    }
    switch ((tFlags >> 3) & 3) {
        case 1: options.aspectCorr = true;  break;
        case 2: options.aspectCorr = false; break;
        default: break;
    }

    // to determine the font name, we first "canonicalize" it by converting
    // it to all-lowercase, strip any non-alphanumeric character, extract
    // the last number (if any), and whether it ends with a plus sign
    int num = 0;
    bool inNumber = false;
    bool plus = false;
    char *pOut = data;
    for (const char *pIn = data;  *pIn;  pIn++) {
        char c = *pIn;
        plus = (c == '+');
        if (((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z'))) {
            inNumber = false;
            *pOut++ = StringUtil::ce_tolower(c);
        } else if ((c >= '0') && (c <= '9')) {
            if (!inNumber) { num = 0; }
            num = num * 10 + c - '0';
            inNumber = true;
            *pOut++ = c;
        } else {
            inNumber = false;
        }
    }
    *pOut = '\0';

    // now parse the actual font name
    options.font = 0;
    if (!strncmp(data, "ibm", 3) || strstr(data, "vga") || strstr(data, "ega")) {
        if (strstr(data, "vga50") || strstr(data, "ega43")) {
            options.font = ANSILOVE_FONT_CP437_80x50;
        } else switch (num) {
            case 737: options.font = ANSILOVE_FONT_CP737; break;
            case 775: options.font = ANSILOVE_FONT_CP775; break;
            case 850: options.font = ANSILOVE_FONT_CP850; break;
            case 852: options.font = ANSILOVE_FONT_CP852; break;
            case 855: options.font = ANSILOVE_FONT_CP855; break;
            case 857: options.font = ANSILOVE_FONT_CP857; break;
            case 860: options.font = ANSILOVE_FONT_CP860; break;
            case 861: options.font = ANSILOVE_FONT_CP861; break;
            case 862: options.font = ANSILOVE_FONT_CP862; break;
            case 863: options.font = ANSILOVE_FONT_CP863; break;
            case 865: options.font = ANSILOVE_FONT_CP865; break;
            case 866: options.font = ANSILOVE_FONT_CP866; break;
            case 869: options.font = ANSILOVE_FONT_CP869; break;
            default:  options.font = ANSILOVE_FONT_CP437; break;
        }
    } else if (strstr(data, "topaz")) {
        options.font = (num == 1) ? (plus ? ANSILOVE_FONT_TOPAZ500_PLUS : ANSILOVE_FONT_TOPAZ500)
                                    : (plus ? ANSILOVE_FONT_TOPAZ_PLUS    : ANSILOVE_FONT_TOPAZ);
    }
    else if (strstr(data, "knight"))   { options.font = plus ? ANSILOVE_FONT_MICROKNIGHT_PLUS : ANSILOVE_FONT_MICROKNIGHT; }
    else if (strstr(data, "mosoul"))   { options.font = ANSILOVE_FONT_MOSOUL; }
    else if (strstr(data, "noodle"))   { options.font = ANSILOVE_FONT_POT_NOODLE; }
    else if (strstr(data, "terminus")) { options.font = ANSILOVE_FONT_TERMINUS; }
    else if (strstr(data, "spleen"))   { options.font = ANSILOVE_FONT_SPLEEN; }
    return options.font ? "valid and used" : "valid and used, but unknown font";
}

///////////////////////////////////////////////////////////////////////////////
// MARK: GD stubs
///////////////////////////////////////////////////////////////////////////////

extern "C" gdImagePtr gdImageCreateTrueColor(int sx, int sy) {
    if ((sx < 1) || (sy < 1)) {return nullptr; }
    #ifndef DEBUG
        if (std::max(sx, sy) > ANSILoader::maxSize) {
            printf("desired image size (%dx%d) exceeds maximum of %d pixels, truncating output\n", sx, sy, ANSILoader::maxSize);
        }
    #endif
    sx = std::min(sx, ANSILoader::maxSize);
    sy = std::min(sy, ANSILoader::maxSize);
    gdImagePtr im = static_cast<gdImagePtr>(::malloc(sizeof(gdImage)));
    if (!im) { return nullptr; }
    im->data = static_cast<int*>(::malloc(sx * sy * sizeof(int)));
    if (!im->data) {
        ::free(static_cast<void*>(im->data));
        ::free(static_cast<void*>(im));
        return nullptr;
    }
    im->sx = sx;
    im->sy = sy;
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
    if (im && (x >= 0) && (y >= 0) && (x < im->sx) && (y < im->sy)) {
        im->data[im->sx * y + x] = color;
    }
}

extern "C" void gdImageCopyResized(gdImagePtr dst, gdImagePtr src, int dstX, int dstY, int srcX, int srcY, int dstW, int dstH, int srcW, int srcH) {
    (void)dst, (void)src, (void)dstX, (void)dstY, (void)srcX, (void)srcY, (void)dstW, (void)dstH, (void)srcW, (void)srcH;
}

extern "C" void gdImageCopyResampled(gdImagePtr dst, gdImagePtr src, int dstX, int dstY, int srcX, int srcY, int dstW, int dstH, int srcW, int srcH) {
    (void)dst, (void)src, (void)dstX, (void)dstY, (void)srcX, (void)srcY, (void)dstW, (void)dstH, (void)srcW, (void)srcH;
}

extern "C" void* gdImagePngPtr(gdImagePtr im, int *size) {
    // don't actually encode a .png here -- we just steal the data pointer
    // and encode the image dimensions in the size parameter
    *size = im->sx | (im->sy << 16);
    auto res = im->data;
    im->data = nullptr;
    return static_cast<void*>(res);
}

void gdFree(void* ptr) {
    free(ptr);
}
