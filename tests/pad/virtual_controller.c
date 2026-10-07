/* Test-only SDL virtual device, controlled on SDL's polling thread. */
#define _GNU_SOURCE
#include <SDL2/SDL.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static int fd = -1, index_ = -1;
static SDL_Joystick *joy;
static void attach(void) {
    index_ = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 2, 4, 1);
    if (index_ >= 0) joy = SDL_JoystickOpen(index_);
}
static void initialize(void) {
    const char *path = getenv("PAD_TEST_SOCKET");
    if (fd >= 0 || !path) return;
    struct sockaddr_un addr = {.sun_family = AF_UNIX};
    snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", path);
    fd = socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    unlink(path);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr))) abort();
    attach();
}
int SDL_Init(Uint32 flags) {
    int (*real)(Uint32) = dlsym(RTLD_NEXT, "SDL_Init");
    int rc = real(flags);
    if (!rc && (flags & SDL_INIT_JOYSTICK)) initialize();
    return rc;
}
int SDL_InitSubSystem(Uint32 flags) {
    int (*real)(Uint32) = dlsym(RTLD_NEXT, "SDL_InitSubSystem");
    int rc = real(flags);
    if (!rc && (flags & SDL_INIT_JOYSTICK)) initialize();
    return rc;
}
static void commands(void) {
    if (fd >= 0) {
        char buf[128], op[32]; int n, a = 0, b = 0, rc = 0;
        struct sockaddr_un peer; socklen_t len = sizeof(peer);
        while ((n = recvfrom(fd, buf, sizeof(buf)-1, 0, (struct sockaddr *)&peer, &len)) > 0) {
            buf[n] = 0; sscanf(buf, "%31s %d %d", op, &a, &b);
            if (!strcmp(op,"button")) rc = joy ? SDL_JoystickSetVirtualButton(joy,a,b) : -1;
            else if (!strcmp(op,"axis")) rc = joy ? SDL_JoystickSetVirtualAxis(joy,a,b) : -1;
            else if (!strcmp(op,"detach")) { if (joy) SDL_JoystickClose(joy); joy = NULL; rc = SDL_JoystickDetachVirtual(index_); index_ = -1; }
            else if (!strcmp(op,"attach")) { if (!joy) attach(); rc = index_ < 0 ? -1 : 0; }
            else if (!strcmp(op,"ping")) rc = joy ? 0 : -1;
            else rc = -1;
            SDL_JoystickUpdate();
            snprintf(buf,sizeof(buf),"%d",rc);
            sendto(fd,buf,strlen(buf),0,(struct sockaddr *)&peer,len);
            len = sizeof(peer);
        }
    }
}
void SDL_PumpEvents(void) {
    void (*real)(void) = dlsym(RTLD_NEXT, "SDL_PumpEvents");
    commands(); real();
}
int SDL_PollEvent(SDL_Event *event) {
    int (*real)(SDL_Event *) = dlsym(RTLD_NEXT, "SDL_PollEvent");
    commands(); return real(event);
}
