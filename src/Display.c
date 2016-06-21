#include "Display.h"
#include "Graphics.h"
#include <GL/gl.h>

typedef struct Rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    //  uint8_t x;
} Rgb;

static Rgb s_clut[PAL_CLUT_SIZE] = { 0 };

HDC   g_hDcWnd = NULL;
HGLRC g_hWgl   = NULL;

void InitDisplay(void)
{
    g_hWgl = wglCreateContext(g_hDcWnd);
    wglMakeCurrent(g_hDcWnd, g_hWgl);

    glClearColor(0.0, 0.0, 0.0, 1.0);
    glViewport(0, 0, MAXWIDTH, MAXHEIGHT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
}

void EndDisplay(void)
{
#ifndef NOT_IMPL
#error Not finished
#endif
}

void Display(int      top,
             int      left,
             int      bottom,
             int      right,
             uint8_t *bits,
             uint     mapSet)
{
    static Rgb s_pixels[MAXWIDTH * MAXHEIGHT] = { 0 };

    int      width = right - left;
    uint     i;
    uint8_t *index;
    Rgb     *pixel;

    if ((mapSet & VMAP) == 0) {
        return;
    }

    // Move to correct row.
    pixel = s_pixels + (top * MAXWIDTH);
    index = bits + (top * MAXWIDTH);

    // Offset to left.
    pixel += left;
    index += left;

    for (; top < bottom; ++top) {
        for (i = 0; i < width; ++i) {
            pixel[i] = s_clut[index[i]];
        }

        // Jump to next line.
        pixel += MAXWIDTH;
        index += MAXWIDTH;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glRasterPos2f(-1, 1);
    glPixelZoom(1, -1);
    glDrawPixels(MAXWIDTH, MAXHEIGHT, GL_RGB, GL_UNSIGNED_BYTE, s_pixels);
    glLoadIdentity();
    SwapBuffers(g_hDcWnd);
}

void GetDisplayCLUT(RPalette *pal)
{
    uint i;

    for (i = 0; i < PAL_CLUT_SIZE; ++i) {
        pal->gun[i].flags = PAL_IN_USE;
        pal->gun[i].r     = s_clut[i].r;
        pal->gun[i].g     = s_clut[i].g;
        pal->gun[i].b     = s_clut[i].b;
    }
}

void SetDisplayCLUT(const RPalette *pal)
{
    uint i;

    for (i = 0; i < PAL_CLUT_SIZE; ++i) {
        int intensity = (int)pal->palIntensity[i];
        s_clut[i].r =
          (uint8_t)(((int)pal->gun[i].r * intensity) / MAX_INTENSITY);
        s_clut[i].g =
          (uint8_t)(((int)pal->gun[i].g * intensity) / MAX_INTENSITY);
        s_clut[i].b =
          (uint8_t)(((int)pal->gun[i].b * intensity) / MAX_INTENSITY);
    }
}

bool DisplayCursor(bool show)
{
#ifndef NOT_IMPL
#error Not finished
#endif
    return true;
}
