#include "Input.h"

void PollInputEvent(void)
{
#ifdef __WINDOWS__
    MSG msg;

    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            exit(1);
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
#endif
}

ushort GetModifiers(void)
{
#ifndef NOT_IMPL
#error Not finished
#endif
    return 0;
}