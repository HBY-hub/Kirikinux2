// SPDX-License-Identifier: AGPL-3.0-only
//
// Minimal X11 helpers for the windowEx plugin.
//
// windowEx's System.getDisplayMonitors / getMonitorInfo / getCursorPos were
// ported as stubs that return TJS_S_OK without writing *result, i.e. they
// return void where the original returns an array / dictionary / dictionary.
// Games then fail with `Member "count" does not exist` (the KR port of
// おっぱいバニー学園 dies in mainwindow.tjs's windowMaxSize getter that way).
//
// The X11 macros (None, Bool, Window, ...) clash with the Win32 compatibility
// layer windowEx.cpp includes, so the Xlib calls live in this separate
// translation unit and are exposed through plain C functions.

#include <X11/Xlib.h>

namespace {

Display *display() {
    static Display *dpy = XOpenDisplay(nullptr);
    return dpy;
}

} // namespace

extern "C" int KR2X11ScreenWidth() {
    Display *dpy = display();
    if(!dpy)
        return 0;
    return DisplayWidth(dpy, DefaultScreen(dpy));
}

extern "C" int KR2X11ScreenHeight() {
    Display *dpy = display();
    if(!dpy)
        return 0;
    return DisplayHeight(dpy, DefaultScreen(dpy));
}

extern "C" int KR2X11CursorPos(int *x, int *y) {
    Display *dpy = display();
    if(!dpy)
        return 0;
    ::Window root = DefaultRootWindow(dpy);
    ::Window child = 0;
    int rootX = 0, rootY = 0, winX = 0, winY = 0;
    unsigned int mask = 0;
    if(!XQueryPointer(dpy, root, &root, &child, &rootX, &rootY, &winX, &winY,
                      &mask))
        return 0;
    if(x)
        *x = rootX;
    if(y)
        *y = rootY;
    return 1;
}

extern "C" void KR2X11WarpCursor(int x, int y) {
    Display *dpy = display();
    if(!dpy)
        return;
    XWarpPointer(dpy, None, DefaultRootWindow(dpy), 0, 0, 0, 0, x, y);
    XFlush(dpy);
}
