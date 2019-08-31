#include "sci/Driver/Audio/LPcmDriver.h"
#include "sci/Driver/Display/Display.h"
#include "sci/Kernel/Audio.h"
#include "sci/Kernel/Dialog.h"
#include "sci/Kernel/Event.h"
#include "sci/Kernel/Graphics.h"
#include "sci/Kernel/Menu.h"
#include "sci/Kernel/Midi.h"
#include "sci/Kernel/Mouse.h"
#include "sci/Kernel/Palette.h"
#include "sci/Kernel/Picture.h"
#include "sci/Kernel/Resource.h"
#include "sci/Kernel/Restart.h"
#include "sci/Kernel/Sound.h"
#include "sci/Kernel/Text.h"
#include "sci/Kernel/VolLoad.h"
#include "sci/Kernel/Window.h"
#include "sci/PMachine/PMachine.h"
#include "sci/Utils/ErrMsg.h"
#include "sci/Utils/Path.h"
#include "sci/Utils/Timer.h"
#include <GL/gl.h>

HWND g_hWndMain = NULL;

void Run(void)
{
    g_picRect.top = 0;

    InitPath(NULL, NULL);

    InstallSoundServer();

    InitTimer();
    InitResource();
    InitScripts();

    CInitGraph();

    InitEvent(16);

    InitPalette();

    InitWindow();

    InitDialog(DoAlert);

    InitAudioDriver();
    InitSoundDriver();

    // Load offsets to often used object properties.  (In script.c)
    LoadPropOffsets();

    // Open up a menu port.
    ROpenPort(&g_menuPortStruc);
    g_menuPort = &g_menuPortStruc;
    InitMenu();

    // Open up a picture window.
    RSetFont(0);
    g_picWind = RNewWindow(&g_picRect, "", NOBORDER | NOSAVE, 0, 1);
    RSetPort(&g_picWind->port);
    InitPicture();

    // We return here on a restart.
    setjmp(g_restartBuf);

    // Turn control over to the pseudo-machine.
    PMachine();
}

static LRESULT CALLBACK WindowProc(HWND   hWnd,
                                   UINT   uMsg,
                                   WPARAM wParam,
                                   LPARAM lParam)
{
    if (uMsg == WM_MOUSEMOVE) {
        g_mousePosX = (int)(short)((LOWORD(lParam) * MAXWIDTH) / DISPLAYWIDTH);
        g_mousePosY =
          (int)(short)((HIWORD(lParam) * MAXHEIGHT) / DISPLAYHEIGHT);
        return 0;
    }

    if (uMsg == WM_LBUTTONDOWN) {
        REventRecord event = { 0 };
        g_buttonState      = 1;
        event.type         = mouseDown;
        event.where.h = (short)((LOWORD(lParam) * MAXWIDTH) / DISPLAYWIDTH);
        event.where.v = (short)((HIWORD(lParam) * MAXHEIGHT) / DISPLAYHEIGHT);
        RPostEvent(&event);
        return 0;
    }

    if (uMsg == WM_LBUTTONUP) {
        REventRecord event = { 0 };
        g_buttonState      = 0;
        event.type         = mouseUp;
        event.where.h = (short)((LOWORD(lParam) * MAXWIDTH) / DISPLAYWIDTH);
        event.where.v = (short)((HIWORD(lParam) * MAXHEIGHT) / DISPLAYHEIGHT);
        RPostEvent(&event);
        return 0;
    }

    if (uMsg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    if (uMsg == WM_PAINT) {
        HDC         hdc;
        PAINTSTRUCT ps;

        hdc = BeginPaint(hWnd, &ps);
        if (g_vHndl != NULL) {
            Display(0, 0, MAXHEIGHT, MAXWIDTH, g_vHndl, VMAP);
        }
        EndPaint(hWnd, &ps);
        return 0;
    }

    if (uMsg == MM_WOM_DONE) {
        AudioDrv(A_FILLBUFF, 0);
        return 0;
    }

    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

int CALLBACK WinMain(_In_ HINSTANCE hInstance,
                     _In_ HINSTANCE hPrevInstance,
                     _In_ LPSTR     lpCmdLine,
                     _In_ int       nCmdShow)
{
    WNDCLASSA             wndClass;
    RECT                  rect;
    const RRect          *bounds;
    HWND                  hWnd;
    GLuint                pixelFormat;
    PIXELFORMATDESCRIPTOR pfd = // pfd Tells Windows How We Want Things To Be
      {
          sizeof(PIXELFORMATDESCRIPTOR), // Size Of This Pixel Format Descriptor
          1,                             // Version Number
          PFD_DRAW_TO_WINDOW |           // Format Must Support Window
            PFD_SUPPORT_OPENGL |         // Format Must Support OpenGL
            PFD_DOUBLEBUFFER,            // Must Support Double Buffering
          PFD_TYPE_RGBA,                 // Request An RGBA Format
          8,                             // Select Our Color Depth
          0,
          0,
          0,
          0,
          0,
          0, // Color Bits Ignored
          0, // No Alpha Buffer
          0, // Shift Bit Ignored
          0, // No Accumulation Buffer
          0,
          0,
          0,
          0,              // Accumulation Bits Ignored
          16,             // 16Bit Z-Buffer (Depth Buffer)
          0,              // No Stencil Buffer
          0,              // No Auxiliary Buffer
          PFD_MAIN_PLANE, // Main Drawing Layer
          0,              // Reserved
          0,
          0,
          0 // Layer Masks Ignored
      };

    if (hPrevInstance != NULL) {
        MessageBoxA(
          GetFocus(), "Cannot run two copies of game!", "Sierra", MB_OK);
        return 0;
    }

    wndClass.style         = CS_HREDRAW | CS_VREDRAW;
    wndClass.lpfnWndProc   = WindowProc;
    wndClass.cbClsExtra    = 0;
    wndClass.cbWndExtra    = 0;
    wndClass.hInstance     = hInstance;
    wndClass.hIcon         = NULL;
    wndClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = GetStockObject(BLACK_BRUSH);
    wndClass.lpszMenuName  = "SciWin";
    wndClass.lpszClassName = "SciWin";

    if (RegisterClassA(&wndClass) == 0) {
        return 0;
    }

    bounds      = GetBounds();
    rect.top    = 0;             // bounds->top;
    rect.left   = 0;             // bounds->left;
    rect.bottom = DISPLAYHEIGHT; // bounds->bottom;
    rect.right  = DISPLAYWIDTH;  // bounds->right;
    AdjustWindowRect(&rect, WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW, FALSE);

    hWnd = CreateWindowA("SciWin",
                         "Sierra On-Line",
                         WS_CLIPCHILDREN | WS_OVERLAPPEDWINDOW,
                         0,
                         0,
                         rect.right - rect.left,
                         rect.bottom - rect.top + 1,
                         NULL,
                         NULL,
                         hInstance,
                         NULL);

    g_hWndMain = hWnd;
    g_hDcWnd   = GetDC(hWnd);

    pixelFormat = ChoosePixelFormat(g_hDcWnd, &pfd);
    SetPixelFormat(g_hDcWnd, pixelFormat, &pfd);

    ShowWindow(hWnd, SW_SHOW);
    UpdateWindow(hWnd);

    Run();
    return 0;
}
