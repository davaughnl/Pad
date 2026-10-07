#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    Display *d = XOpenDisplay(NULL); if (!d) return 2;
    Window w = XCreateSimpleWindow(d, DefaultRootWindow(d), 0, 0, 300, 200, 0, 0, 0);
    XSelectInput(d,w,KeyPressMask|KeyReleaseMask|ButtonPressMask|ButtonReleaseMask);
    XMapWindow(d,w); XSync(d,False);
    XSetInputFocus(d,w,RevertToParent,CurrentTime); XWarpPointer(d,None,w,0,0,0,0,100,100); XSync(d,False);
    puts("ready"); fflush(stdout);
    for (;;) {
        XEvent e; XNextEvent(d,&e);
        if (e.type == KeyPress || e.type == KeyRelease)
            printf("key %lu %d\n", XLookupKeysym(&e.xkey,0), e.type == KeyPress);
        else if (e.type == ButtonPress || e.type == ButtonRelease)
            printf("mouse %u %d\n",e.xbutton.button,e.type == ButtonPress);
        fflush(stdout);
    }
}
