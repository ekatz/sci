#include "Display.h"
#include "Graphics.h"
#if defined(__WINDOWS__)
#include <GL/gl.h>
#elif defined(__IOS__)
#include <OpenGLES/ES1/gl.h>
#include <dispatch/dispatch.h>
#endif

typedef struct Rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Rgb;

static Rgb s_clut[PAL_CLUT_SIZE] = { 0 };
static Rgb s_pixels[MAXWIDTH * MAXHEIGHT] = { 0 };

#if defined(__WINDOWS__)
HDC   g_hDcWnd = NULL;
HGLRC g_hWgl   = NULL;
#elif defined(__IOS__)
static dispatch_queue_t s_dispatchQueue = NULL;
#endif

static GLuint        s_texture       = 0;
static const GLfloat s_texVertices[] = {
    0.f, 0.f,
    0.f, 1.f,
    1.f, 0.f,
    1.f, 1.f
};
static const GLfloat s_vertices[] = {
    -1.f,  1.f,
    -1.f, -1.f,
     1.f,  1.f,
     1.f, -1.f
};

#if defined(__IOS__)
void SwapBuffers(void);
void DispatchDisplay(void *);
#endif

void InitDisplay(void)
{
#if defined(__WINDOWS__)
    g_hWgl = wglCreateContext(g_hDcWnd);
    wglMakeCurrent(g_hDcWnd, g_hWgl);
#elif defined(__IOS__)
    s_dispatchQueue = dispatch_queue_create(NULL, 0);
#endif

    glClearColor(0.0, 0.0, 0.0, 1.0);
    glViewport(0, 0, DISPLAYWIDTH, DISPLAYHEIGHT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glGenTextures(1, &s_texture);
    glBindTexture(GL_TEXTURE_2D, s_texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

#if defined(__IOS__)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
#endif
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
    int      width;
    int      i;
    uint8_t *index;
    Rgb     *pixel;

    if (bottom > MAXHEIGHT) {
        bottom = MAXHEIGHT;
    }
    if (right > MAXWIDTH) {
        right = MAXWIDTH;
    }
    width = right - left;

    // Move to correct row.
    pixel = s_pixels + (top * MAXWIDTH);
    index = bits + (top * MAXWIDTH);

    // Offset to left.
    pixel += left;
    index += left;

    if ((mapSet & VMAP) != 0) {
        for (; top < bottom; ++top) {
            for (i = 0; i < width; ++i) {
                pixel[i] = s_clut[index[i]];
            }

            // Jump to next line.
            pixel += MAXWIDTH;
            index += MAXWIDTH;
        }
    } else if ((mapSet & PMAP) != 0) {
        static const Rgb c_colors[0xf] = {
            {   0,   0,   0 },
            {   0,   0, 160 },
            {   0, 160,   0 },
            {   0, 160, 160 },
            { 160,   0,   0 },
            { 160,   0, 160 },
            { 160,  80,   0 },
            { 160, 160, 160 },
            {  80,  80,  80 },
            {  80,  80, 255 },
            {  80, 255,   0 },
            {  80, 255, 255 },
            { 255,  80,  80 },
            { 255,  80, 255 },
            { 255, 255,  80 },
        };

        for (; top < bottom; ++top) {
            for (i = 0; i < width; ++i) {
                pixel[i] = c_colors[index[i] >> 4];
            }

            // Jump to next line.
            pixel += MAXWIDTH;
            index += MAXWIDTH;
        }
    } else {
        return;
    }

#if defined(__IOS__)
    dispatch_async_f(s_dispatchQueue, NULL, DispatchDisplay);
}

void DoDisplay(void)
{
#endif

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#if defined(__IOS__)
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
#endif
    glEnable(GL_TEXTURE_2D);
#if defined(__WINDOWS__)
    glEnableClientState(GL_TEXTURE_COORD_ARRAY_EXT);
#elif defined(__IOS__)
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
#endif
    glEnableClientState(GL_VERTEX_ARRAY);

    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGB,
                 MAXWIDTH,
                 MAXHEIGHT,
                 0,
                 GL_RGB,
                 GL_UNSIGNED_BYTE,
                 s_pixels);
    glBindTexture(GL_TEXTURE_2D, s_texture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    // vertex
    glVertexPointer(2, GL_FLOAT, 0, s_vertices);

    // texCoods
    glTexCoordPointer(2, GL_FLOAT, 0, s_texVertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableClientState(GL_VERTEX_ARRAY);
#if defined(__WINDOWS__)
    glDisableClientState(GL_TEXTURE_COORD_ARRAY_EXT);
#elif defined(__IOS__)
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
#endif
    glDisable(GL_TEXTURE_2D);

#if defined(__WINDOWS__)
    SwapBuffers(g_hDcWnd);
#endif
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
