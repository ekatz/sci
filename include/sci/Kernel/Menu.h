#ifndef SCI_KERNEL_MENU_H
#define SCI_KERNEL_MENU_H

#include "sci/Kernel/GrTypes.h"

#define MARKWIDE  8
#define BARSIZE   10
#define MAXMENUS  10   // maximum titles in menu bar
#define CHECKMARK 0x10 // select menu character
#define FIRST     1    // start index for pointer arrays

// Pseudo selectors for SetMenu and Display.
// Must be duplicated by (define)'s in SYSTEM.SH.
#define p_at      100
#define p_mode    101
#define p_color   102
#define p_back    103
#define p_style   104
#define p_font    105
#define p_width   106
#define p_save    107
#define p_restore 108
#define p_said    109
#define p_text    110
#define p_key     111
#define p_state   112
#define p_value   113
#define p_dispose 114
#define p_time    115
#define p_title   116
#define p_draw    117
#define p_edit    118
#define p_button  119
#define p_icon    120
#define p_noshow  121

typedef struct RMenuItem {
    RRect       bar;
    const char *text;
    int         value;
    int         state;
    int         key;
    uint8_t    *said;
} RMenuItem;

typedef struct MenuPage {
    RRect       bar;
    const char *text;
    RRect       pageRect;
    uint        items;
    Handle      ubits;
    RMenuItem  *item[1];
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

void KMenuSelect(uintptr_t *args);
void KAddMenu(uintptr_t *args);
void KGetMenu(uintptr_t *args);
void KSetMenu(uintptr_t *args);
void KDrawStatus(uintptr_t *args);
void KDrawMenuBar(uintptr_t *args);

#endif // SCI_KERNEL_MENU_H
