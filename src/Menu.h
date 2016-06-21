#ifndef MENU_H
#define MENU_H

#include "GrTypes.h"

#define MARKWIDE  8
#define BARSIZE   10
#define MAXMENUS  10   // maximum titles in menu bar
#define CHECKMARK 0x10 // select menu character
#define FIRST     1    // start index for pointer arrays

typedef struct MenuPage {
    int a;
} MenuPage;

typedef struct MenuBar {
    RRect     bar;
    bool      hidden;
    uint      pages;
    MenuPage *page[1];
} MenuBar;

extern RGrafPort  g_menuPortStruc;
extern RGrafPort *g_menuPort;

void InitMenu(void);

#endif // MENU_H
