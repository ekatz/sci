#include "sci/Driver/Input/Input.h"

void PollInputEvent(void)
{
#ifdef __WINDOWS__
    MSG msg;

    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            exit(1);
        }

        TranslateMessage(&msg);
        DispatchMessageA(&msg);
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
