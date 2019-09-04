#include "sci/Kernel/Menu.h"
#include "sci/Kernel/Animate.h"
#include "sci/Kernel/Dialog.h"
#include "sci/Kernel/Event.h"
#include "sci/Kernel/Graphics.h"
#include "sci/Kernel/Mouse.h"
#include "sci/Kernel/Selector.h"
#include "sci/Kernel/Sound.h"
#include "sci/Kernel/Text.h"
#include "sci/Kernel/Window.h"
#include "sci/Driver/Input/Input.h"
#include "sci/PMachine/PMachine.h"

RGrafPort  g_menuPortStruc = { 0 };
RGrafPort *g_menuPort      = NULL;

static MenuBar    *s_theMenuBar = NULL;
static const char *s_statStr    = NULL;

// Alt key remapping table.
static char s_altKey[] = {
    30, 48, 46, 32, 18, 33, 34, 35, 23, // a - i
    36, 37, 38, 50, 49, 24, 25, 16, 19, // j - r
    31, 20, 22, 47, 17, 45, 21, 44      // s - z
};

// Clear the menu bar to color.
static void ClearBar(uint8_t color);

static void DoDrawMenuBar(bool show);

// Make a direction event based selection.
static uint KeySelect(void);

// Return matrix ID of item selected via mouse.
static uint MouseSelect(void);

// Return menu INDEX that mouse may be in.
static uint FindTitle(RPoint *mp);

// Return item INDEX that mouse may be in.
static uint FindItem(uint m, RPoint *mp);

// Invert rectangle of this item.
static void Invert(uint m, uint i);

// Drop down and draw this menu page.
static void DropDown(uint m);

// Restore bits under this menu.
static void PullUp(uint m);

// Step through items to size menu page.
static void SizePage(MenuPage *menu);

// Format a string indicating key equivalence.
static char *GetKeyStr(char *str, int key);

// Unhighlight, move up, and re-highlight.
static uint PickUp(uint m, uint i);

// Unhighlight, move down, and re-highlight.
static uint PickDown(uint m, uint i);

void InitMenu(void)
{
    size_t size  = sizeof(MenuBar) + (MAXMENUS * sizeof(MenuPage *));
    s_theMenuBar = (MenuBar *)malloc(size);
    memset(s_theMenuBar, 0, size);
    RSetRect(&s_theMenuBar->bar, 0, 0, g_rThePort->portRect.right, BARSIZE);
    s_theMenuBar->hidden = true;
    s_theMenuBar->pages  = FIRST;
}

void KSetMenu(argList)
{
    RMenuItem *item;
    size_t     i;

    item = s_theMenuBar->page[arg(1) >> 8]->item[arg(1) & 255];
    for (i = 2; i < arg(0); i += 2) {
        switch (arg(i)) {
            case p_said:
                item->said = (uint8_t *)arg(i + 1);
                break;
            case p_text:
                item->text = (const char *)arg(i + 1);
                break;
            case p_key:
                item->key = (int)arg(i + 1);
                break;
            case p_state:
                item->state = (int)arg(i + 1);
                break;
            case p_value:
                item->value = (int)arg(i + 1);
                break;
        }
    }
}

void KGetMenu(argList)
{
    RMenuItem *item;

    item = s_theMenuBar->page[arg(1) >> 8]->item[arg(1) & 255];
    switch (arg(2)) {
        case p_said:
            *acc = (uintptr_t)item->said;
            break;

        case p_text:
            *acc = (uintptr_t)item->text;
            break;

        case p_key:
            *acc = (uintptr_t)item->key;
            break;

        case p_state:
            *acc = (uintptr_t)item->state;
            break;

        case p_value:
            *acc = (uintptr_t)item->value;
            break;
    }
}

// Return of -1 indicates no entry selected.
// Otherwise, high byte is menu number, low byte is number of entry in menu.
void KMenuSelect(argList)
{
    MenuPage  *menu;
    RMenuItem *item;
    uint8_t   *saidSpec;
    ushort     type, message;
    uint       m, i;
    Obj       *event  = (Obj *)arg(1);
    bool       blocks = true;

    *acc = (uintptr_t)-1;

    if (argCount == 2 && !arg(2))
        blocks = false;

    // If we are not inited we return false.
    if (s_theMenuBar == NULL) {
        *acc = 0;
        return;
    }

    // Are we done before we start?
    if (IndexedProp(event, evClaimed) != FALSE) {
        return;
    }

    message = (ushort)IndexedProp(event, evMsg);
    type    = (ushort)IndexedProp(event, evType);
    switch (type) {
        case keyDown:
        case joyDown:
            if (type == joyDown || message == ESC) {
                IndexedProp(event, evClaimed) = TRUE;
                if (blocks) {
                    PauseSnd(NULL, true);
                }
                *acc = KeySelect();
                if (blocks) {
                    PauseSnd(NULL, false);
                }
                break;
            }

        case saidEvent:
            // Check applicable property against event.
            for (m = FIRST; m < s_theMenuBar->pages; m++) {
                menu = s_theMenuBar->page[m];
                for (i = FIRST; i < menu->items; i++) {
                    item = menu->item[i];
                    if (item->state & dActive) {
                        switch (type) {
                            case keyDown:
                                // Make an ASCII message uppercase.
                                if (message < 0x100) {
                                    message = (ushort)toupper((char)message);
                                }
                                if ((word)item->key &&
                                    (word)item->key == message) {
                                    IndexedProp(event, evClaimed) = TRUE;
                                    *acc = (i | (m << 8));
                                }
                                break;

                            case saidEvent:
                                saidSpec = item->said;
                                if (saidSpec) {
                                    IndexedProp(event, evClaimed) = TRUE;
                                    *acc = (i | (m << 8));
                                }
                                break;
                        }
                    }
                }
            }
            break;

        case mouseDown:
            if (s_theMenuBar->bar.bottom > (int16_t)IndexedProp(event, evY)) {
                IndexedProp(event, evClaimed) = TRUE;
                if (blocks) {
                    PauseSnd(NULL, true);
                }
                *acc = MouseSelect();
                if (blocks) {
                    PauseSnd(NULL, false);
                }
            }
            break;
    }
}

void KDrawStatus(argList)
{
    RGrafPort  *oldPort;
    uint8_t     background = vWHITE;
    uint8_t     foreground = vBLACK;
    const char *str        = (const char *)arg(1);

    if (argCount >= 2) {
        foreground = (uint8_t)arg(2);

        if (argCount >= 3) {
            background = (uint8_t)arg(3);
        }
    }

    RGetPort(&oldPort);
    RSetPort(g_menuPort);

    if ((s_statStr = str) != NULL) {
        // Erase the bar rectangle
        ClearBar(background);

        PenColor(foreground);
        RMoveTo(0, 1);

        DrawString(str);

        // Show our handiwork.
        ShowBits(&s_theMenuBar->bar, VMAP);
    }

    RSetPort(oldPort);
}

static void ClearBar(uint8_t color)
{
    RGrafPort *oldPort;
    RRect      r;

    RGetPort(&oldPort);
    RSetPort(g_menuPort);

    // Erase the bar rectangle.
    RCopyRect(&s_theMenuBar->bar, &r);
    RFillRect(&r, VMAP, color, 0, 0);

    // Draw the separator as a black rectangle.
    r.top = r.bottom - 1;
    RFillRect(&r, VMAP, vBLACK, 0, 0);
    RSetPort(oldPort);
}

void KAddMenu(argList)
{
    MenuPage  *menu;
    RMenuItem *item = NULL;
    int        m, i, key;
    char       valStr[10];
    char      *data, *argData;
    bool       newItem;

    argData = strdup((char *)arg(2));

    // If we have no theMenuBar we init one.
    if (s_theMenuBar == NULL) {
        InitMenu();
    }

    // Add to end of existing menubar.
    for (m = FIRST; m <= MAXMENUS; m++) {
        if (s_theMenuBar->page[m] == NULL) {
            // We've got another one.
            ++s_theMenuBar->pages;

            // Determine entries required.
            data = argData;
            for (i = 1; *data != '\0'; data++) {
                if (*data == ':') {
                    ++i;
                }
            }

            // Allocate a menu for these items.
            s_theMenuBar->page[m] = menu =
              (MenuPage *)malloc(sizeof(MenuPage) + (i * sizeof(RMenuItem *)));
            menu->text  = (const char *)arg(1);
            menu->items = FIRST;

            // Scan the definition string for item properties.
            i       = FIRST;
            data    = argData;
            newItem = true;
            while (*data != '\0') {
                if (newItem) {
                    ++menu->items;
                    menu->item[i++] = item =
                      (RMenuItem *)malloc(sizeof(RMenuItem));
                    memset(item, 0, sizeof(RMenuItem));
                    item->text  = data;
                    item->state = dActive;
                    newItem     = false;
                }

                switch (*data) {
                    case ':':
                        newItem = true;
                        *data   = '\0';
                        break;

                    case '!':
                        // State is inactive.
                        item->state = 0;
                        *data       = '\0';
                        break;

                    case '=':
                        // Initial value
                        *data++     = '\0';
                        key         = 0;
                        valStr[key] = 0;
                        while (isdigit(*data)) {
                            valStr[key++] = *data++;
                            valStr[key]   = 0;
                        }
                        data--;
                        item->value = atoi(valStr);
                        break;

                    case '`':
                        // Next byte(s) are a key code.
                        *data++ = '\0';
                        key     = *data++;
                        switch (key) {
                            case '@':
                                // Alt key.
                                key       = toupper(*data);
                                item->key = s_altKey[key - 'A'] << 8;
                                break;

                            case '#':
                                // Function key.
                                key = toupper(*data);

                                // Allow F10 key as `#0.
                                if (key == '0') {
                                    key += 10;
                                }
                                item->key = (key - '1' + 0x3b) << 8;
                                break;

                            case '^':
                                key       = toupper(*data);
                                item->key = key - 64;
                                break;

                            default:
                                --data;
                                item->key = toupper(*data);
                        }
                        break;
                }
                ++data;
            }

            // Menu is added.
            break;
        }
    }
}

void KDrawMenuBar(argList)
{
    DoDrawMenuBar((bool)arg(1));
}

static void DoDrawMenuBar(bool show)
{
    MenuPage  *menu;
    RGrafPort *oldPort;
    uint       i;
    int        lastX = 8;

    RGetPort(&oldPort);
    RSetPort(g_menuPort);

    if (show) {
        s_theMenuBar->hidden = false;
        ClearBar(vWHITE);
        PenColor(vBLACK);

        // Step through the menu titles and draw them.
        for (i = FIRST; i < s_theMenuBar->pages; i++) {
            menu = s_theMenuBar->page[i];
            RTextSize(&menu->bar, menu->text, -1, 0);
            menu->bar.bottom = BARSIZE - 2;
            MoveRect(&menu->bar, lastX, 1);
            RMoveTo(menu->bar.left, menu->bar.top);
            DrawString(menu->text);
            menu->bar.top -= 1;
            lastX = menu->bar.right;
        }
    } else {
        // Hide the bar.
        s_theMenuBar->hidden = true;
        ClearBar(vBLACK);
    }

    // Show our handy work.
    ShowBits(&s_theMenuBar->bar, VMAP);

    // Restore old port.
    RSetPort(oldPort);
}

static uint KeySelect(void)
{
    REventRecord event;
    bool         done  = false;
    uint         ret   = 0;
    Handle       uBits = NULL;
    RGrafPort   *oldPort;
    uint         menu, item; // here these are indexes

    RGetPort(&oldPort);
    RSetPort(g_menuPort);

    // Do we need to redraw.
    if (s_statStr != NULL || s_theMenuBar->hidden) {
        uBits = SaveBits(&s_theMenuBar->bar, VMAP);
        DoDrawMenuBar(true);
    }

    // Start with first page.
    DropDown((menu = FIRST));
    // First item of page.
    item = PickDown(menu, 0);

    // Flush any events that may have accumulated while drawing.
    RFlushEvents(allEvents);

    while (!done) {
        RGetNextEvent(allEvents, &event);
        MapKeyToDir(&event);
        switch (event.type) {
            case joyDown:
                done = true;
                if (item != 0) {
                    ret = item | (menu << 8);
                } else {
                    ret = 0;
                }
                break;

            case keyDown:
                switch (event.message) {
                    case ESC:
                        done = true;
                        ret  = 0;
                        break;

                    case CR:
                        done = true;
                        if (item != 0) {
                            ret = item | (menu << 8);
                        } else {
                            ret = 0;
                        }
                        break;
                }
                break;

            case direction | keyDown:
            case direction:
                switch (event.message) {
                    case dirE:
                        PullUp(menu);
                        if ((++menu) >= s_theMenuBar->pages) {
                            menu = FIRST;
                        }
                        DropDown(menu);
                        item = PickDown(menu, 0);
                        break;

                    case dirW:
                        PullUp(menu);
                        if ((--menu) == 0) {
                            menu = s_theMenuBar->pages - 1;
                        }
                        DropDown(menu);
                        item = PickDown(menu, 0);
                        break;

                    case dirN:
                        item = PickUp(menu, item);
                        break;

                    case dirS:
                        item = PickDown(menu, item);
                        break;
                }
                // Flush any events that may have accumulated while drawing.
                RFlushEvents(allEvents);
        }
    }

    PullUp(menu);

    // Do we need to restore something hidden?
    if (uBits) {
        RestoreBits(uBits);
        ShowBits(&s_theMenuBar->bar, VMAP);
        s_theMenuBar->hidden = true;
    }

    RSetPort(oldPort);
    return ret;
}

static uint MouseSelect(void)
{
    uint       menu, item; // here these are indexes
    RGrafPort *oldPort;
    RPoint     mp;
    Handle     uBits = NULL;

    RGetPort(&oldPort);
    RSetPort(g_menuPort);

    // Do we need to redraw.
    if (s_statStr != NULL || s_theMenuBar->hidden) {
        // Show our handy work.
        uBits = SaveBits(&s_theMenuBar->bar, VMAP);
        DoDrawMenuBar(true);
    }

    // Start with none selected.
    item = menu = 0;
    do {
        PollInputEvent();
        RGetMouse(&mp);
        if (RPtInRect(&mp, &s_theMenuBar->bar)) {
            Invert(menu, item);
            item = 0;
            if (menu != FindTitle(&mp)) {
                PullUp(menu);
                menu = FindTitle(&mp);
                if (menu != 0) {
                    DropDown(menu);
                }
            }
        } else {
            if (item != FindItem(menu, &mp)) {
                Invert(menu, item);
                item = FindItem(menu, &mp);
                if (item != 0) {
                    Invert(menu, item);
                }
            }
        }
    } while (RStillDown());

    // Get rid of that mouseUp.
    RFlushEvents(mouseUp);

    PullUp(menu);

    // Do we need to restore something hidden?
    if (uBits) {
        RestoreBits(uBits);
        ShowBits(&s_theMenuBar->bar, VMAP);
        s_theMenuBar->hidden = true;
    }

    RSetPort(oldPort);

    // If item is non zero we have a valid selection.
    if (item != 0) {
        item |= (menu << 8);
    }

    return item;
}

static uint FindTitle(RPoint *mp)
{
    MenuPage *menu;
    uint      m;
    RRect     r;

    // Step through the menu titles and check for mouse in one of them.
    for (m = FIRST; m < s_theMenuBar->pages; m++) {
        menu = s_theMenuBar->page[m];
        RCopyRect(&menu->bar, &r);
        ++r.bottom;
        if (RPtInRect(mp, &r)) {
            return m;
        }
    }
    return 0;
}

static uint FindItem(uint m, RPoint *mp)
{
    RMenuItem *item;
    MenuPage  *menu;
    uint       i;

    // Don't bother with null menu.
    if (m != 0) {
        menu = s_theMenuBar->page[m];
        // Step through the items and check for mouse in one of them.
        for (i = FIRST; i < menu->items; i++) {
            item = menu->item[i];
            if ((dActive & item->state) && RPtInRect(mp, &item->bar)) {
                return i;
            }
        }
    }
    return 0;
}

static void Invert(uint m, uint i)
{
    RMenuItem *item;

    if (m != 0 && i != 0) {
        item = s_theMenuBar->page[m]->item[i];
        RInvertRect(&item->bar);
        ShowBits(&item->bar, VMAP);
    }
}

static void DropDown(uint m)
{
    uint       i, cnt;
    MenuPage  *menu = s_theMenuBar->page[m];
    RMenuItem *item;

    RRect   r;
    int16_t leftX, lastX, lastY = BARSIZE;
    char    text[10];

    // Invert the title.
    RInvertRect(&menu->bar);
    ShowBits(&menu->bar, VMAP);

    // Init the page rectangle to owners size.
    SizePage(menu);

    menu->ubits = SaveBits(&menu->pageRect, VMAP);
    REraseRect(&menu->pageRect);
    RFrameRect(&menu->pageRect);
    ShowBits(&menu->pageRect, VMAP);
    lastX = menu->pageRect.right - 1;
    leftX = menu->pageRect.left + 1;

    // Step through the menu and draw each item.
    for (i = FIRST; i < menu->items; i++) {
        item = menu->item[i];

        // Resize left of text rectangle to size of page.
        item->bar.left = leftX;

        // Resize right of text rectangle to max for all.
        item->bar.right = lastX;

        // Get set to draw the item.
        RCopyRect(&item->bar, &r);
        REraseRect(&r);
        RMoveTo(r.left, r.top);
        if (dActive & item->state) {
            RTextFace(PLAIN);
        } else {
            RTextFace(DIM);
        }

        // Extend '-' to full width.
        if (*item->text == '-') {
            cnt = (r.right + 1 - r.left) / RCharWidth('-');
            while (cnt-- != 0) {
                RDrawChar('-');
            }
        } else {
            if (dSelected & item->state) {
                RDrawChar(CHECKMARK);
            }
            RMoveTo(r.left, r.top);
            RMove(MARKWIDE, 0);
            ShowString(item->text);
        }

        // Draw in the key equivalent.
        if (item->key) {
            GetKeyStr(text, item->key);
            RMoveTo((int)r.right - (int)RStringWidth(text), r.top);
            ShowString(text);
        }
    }
    RTextFace(0);
    ShowBits(&menu->pageRect, VMAP);
}

static void PullUp(uint m)
{
    MenuPage *menu = s_theMenuBar->page[m];

    if (m != 0) {
        RestoreBits(menu->ubits);
        ReAnimate(&menu->pageRect);

        // Re-invert the title.
        RInvertRect(&menu->bar);
        ShowBits(&menu->bar, VMAP);
    }
}

static void SizePage(MenuPage *menu)
{
    RMenuItem *item;
    uint       hasAKey = 0; // length of longest key string
    RRect      workRect, r;
    int16_t    lastY = BARSIZE;
    char       text[10];
    uint       i;

    // Init the our rectangle to owners size.
    RCopyRect(&menu->bar, &r);
    ++r.bottom;
    r.top   = r.bottom;
    r.right = r.left;
    lastY   = r.top;

    // Step through the list and size individual rectangles.
    // Union them into one rectangle.
    for (i = FIRST; i < menu->items; i++) {
        item = menu->item[i];
        RTextSize(&workRect, item->text, -1, 0);
        MoveRect(&workRect, r.left, lastY);
        workRect.right += MARKWIDE;

        // Add some room at right if item has a key equivalent.
        if (item->key != 0) {
            if (RStringWidth(GetKeyStr(text, item->key)) > hasAKey) {
                hasAKey = RStringWidth(GetKeyStr(text, item->key));
            }
        }

        RUnionRect(&workRect, &r, &r);

        // Now give this rectangle to the item.
        RCopyRect(&workRect, &item->bar);

        lastY = r.bottom;
    }

    // Make a little space.
    r.right += 7;
    RInsetRect(&r, -1, -1);

    // Add some space for KEYSTRINGS.
    if (hasAKey != 0) {
        r.right += (int16_t)hasAKey;
    }

    // If our rectangle is off the right we slide it back.
    if (r.right >= MAXWIDTH) {
        ROffsetRect(&r, MAXWIDTH - r.right, 0);
    }

    // Set callers rect to this value.
    RCopyRect(&r, &menu->pageRect);
}

static char *GetKeyStr(char *str, int key)
{
    int i;

    // Null by default.
    *str = '\0';
    if (key < 256) // normal alpha or control
    {
        if (key < ' ') {
            sprintf(str, "%c%c ", 3, key + 0x40);
        } else {
            sprintf(str, "%c ", key);
        }
    } else {
        key /= 256;
        // Function keys.
        if (key >= 59 && key <= 68) {
            key -= 58;
            sprintf(str, "F%d ", key);
        } else {
            // Alt keys are selected by occurrence in the alt key table.
            for (i = 0; i < 26; i++) {
                if ((int)s_altKey[i] == key) {
                    sprintf(str, "%c%c ", 2, i + 0x41);
                    break;
                }
            }
        }
    }
    return str;
}

static uint PickUp(uint m, uint i)
{
    MenuPage  *menu = s_theMenuBar->page[m];
    RMenuItem *item;

    // We do nothing if we are at top.
    while (i != 0) {
        item = menu->item[i];

        // Deselect current.
        if (dActive & item->state) {
            RInvertRect(&item->bar);
            ShowBits(&item->bar, VMAP);
        }
        --i;

        if (i != 0) {
            item = menu->item[i];
            if (dActive & item->state) {
                // Select current and exit.
                RInvertRect(&item->bar);
                ShowBits(&item->bar, VMAP);
                return i;
            }
        }
    }
    return i;
}

static uint PickDown(uint m, uint i)
{
    MenuPage  *menu = s_theMenuBar->page[m];
    RMenuItem *item;
    uint       newI;

    /* we do nothing if we are at the bottom */
    while ((newI = i + 1) < menu->items) {
        if (i != 0) {
            item = menu->item[i];
            /* deselect current */
            if (dActive & item->state) {
                RInvertRect(&item->bar);
                ShowBits(&item->bar, VMAP);
            }
        }
        ++i;

        if (i < menu->items) {
            item = menu->item[i];
            if (dActive & item->state) {
                // Select current and exit.
                RInvertRect(&item->bar);
                ShowBits(&item->bar, VMAP);
                return i;
            }
        }
    }
    return i;
}
