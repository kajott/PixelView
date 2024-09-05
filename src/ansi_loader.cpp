// SPDX-FileCopyrightText: 2024 Martin J. Fiedler <keyj@emphy.de>
// SPDX-License-Identifier: MIT

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "ansilove.h"

#include "string_util.h"

#include "gd.h"

#include "ansi_loader.h"

///////////////////////////////////////////////////////////////////////////////

namespace ANSI {

extern const uint32_t fileExts[] = {
    StringUtil::makeExtCode("asc"),
    StringUtil::makeExtCode("ans"),
    StringUtil::makeExtCode("adf"),
    StringUtil::makeExtCode("bin"),
    StringUtil::makeExtCode("idf"),
    StringUtil::makeExtCode("pcb"),
    StringUtil::makeExtCode("tnd"),
    StringUtil::makeExtCode("xb"),
    0
};

void* render(const char* filename, int &width, int &height) {
    // ansilove context initialization (equivalent to ansilove_init())
    struct ansilove_ctx     ctx;
    struct ansilove_options opt;
    ::memset(static_cast<void*>(&ctx), 0, sizeof(ctx));
    ::memset(static_cast<void*>(&opt), 0, sizeof(opt));
    opt.bits = 8;

    // load the source file
    int size = 0;
    ctx.buffer = reinterpret_cast<uint8_t*>(StringUtil::loadTextFile(filename, size));
    if (!ctx.buffer) { return nullptr; }
    ctx.maplen = ctx.length = static_cast<size_t>(size);

    // run the actual ansilove renderer
    int res;
    switch (StringUtil::extractExtCode(filename)) {
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

    // export data and clean up
    ::free(static_cast<void*>(ctx.buffer));
    width  = ctx.png.length & 0xFFFF;
    height = ctx.png.length >> 16;
    return ctx.png.buffer;
}

}  // namespace ANSI

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
    for (int y = y1;  y <= y2;  ++y) {
        int *line = &im->data[im->sx * y];
        for (int x = x1;  x <= x2;  ++x) {
            line[x] = color;
        }
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
