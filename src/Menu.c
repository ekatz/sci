#include "Menu.h"
#include "Graphics.h"
#include "Window.h"

RGrafPort  g_menuPortStruc = { 0 };
RGrafPort *g_menuPort      = NULL;

MenuBar *s_theMenuBar = NULL;

void InitMenu(void)
{
    size_t size  = sizeof(MenuBar) + (MAXMENUS * sizeof(MenuPage *));
    s_theMenuBar = (MenuBar *)malloc(size);
    memset(s_theMenuBar, 0, size);
    RSetRect(&s_theMenuBar->bar, 0, 0, g_rThePort->portRect.right, BARSIZE);
    s_theMenuBar->hidden = true;
    s_theMenuBar->pages  = FIRST;
}
