// Added: entire file. See iosAngle.h for why iOS does not use SDL_GL_*.

#include "Platform/iOS/iosAngle.h"

#ifdef TARGET_IOS

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include <SDL.h>
#include <SDL_metal.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include "stdPlatform.h"
#include "Platform/iOS/iosTouch.h" // (the tray's FPS: 60 or 120)

static SDL_MetalView iosAngle_metalView = NULL;
static EGLDisplay    iosAngle_display   = EGL_NO_DISPLAY;
static EGLSurface    iosAngle_surface   = EGL_NO_SURFACE;
static EGLContext    iosAngle_context   = EGL_NO_CONTEXT;
static char          iosAngle_errorMsg[256] = {0};

static int iosAngle_Fail(const char* pWhat)
{
    snprintf(iosAngle_errorMsg, sizeof(iosAngle_errorMsg), "%s (EGL error 0x%04x)", pWhat, (unsigned)eglGetError());
    stdPlatform_Printf("iosAngle: %s\n", iosAngle_errorMsg);
    return 0;
}

const char* iosAngle_GetError(void)
{
    return iosAngle_errorMsg;
}

// ANGLE picks a backend per-display rather than per-build, so ask explicitly for
// Metal. The generic eglGetDisplay(EGL_DEFAULT_DISPLAY) would also land on Metal
// on Apple platforms today, but only by default-ordering -- naming it means a
// future ANGLE that prefers something else here still does what we tested.
static EGLDisplay iosAngle_GetMetalDisplay(void)
{
    PFNEGLGETPLATFORMDISPLAYEXTPROC pGetPlatformDisplayEXT =
        (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");

    if (pGetPlatformDisplayEXT) {
        const EGLint aDisplayAttribs[] = {
            EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_METAL_ANGLE,
            EGL_NONE
        };
        EGLDisplay display = pGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, EGL_DEFAULT_DISPLAY, aDisplayAttribs);
        if (display != EGL_NO_DISPLAY) {
            return display;
        }
        stdPlatform_Printf("iosAngle: Metal platform display unavailable, falling back to the default display\n");
    }

    return eglGetDisplay(EGL_DEFAULT_DISPLAY);
}

int iosAngle_CreateContext(SDL_Window* pWindow)
{
    iosAngle_errorMsg[0] = 0;

    if (iosAngle_context != EGL_NO_CONTEXT) {
        iosAngle_DestroyContext();
    }

    // SDL3 hands out a UIView whose backing layer is a CAMetalLayer, already
    // sized and content-scaled for the window (SDL_WINDOW_HIGH_PIXEL_DENSITY is
    // honoured here). ANGLE's Metal backend takes any CALayer as its
    // EGLNativeWindowType and uses a CAMetalLayer as-is, so this is a direct handoff
    // with no intermediate blit -- the game renders into the layer the compositor
    // shows.
    iosAngle_metalView = SDL_Metal_CreateView(pWindow);
    if (!iosAngle_metalView) {
        snprintf(iosAngle_errorMsg, sizeof(iosAngle_errorMsg), "SDL_Metal_CreateView failed: %s", SDL_GetError());
        stdPlatform_Printf("iosAngle: %s\n", iosAngle_errorMsg);
        return 0;
    }

    void* pLayer = SDL_Metal_GetLayer(iosAngle_metalView);
    if (!pLayer) {
        snprintf(iosAngle_errorMsg, sizeof(iosAngle_errorMsg), "SDL_Metal_GetLayer failed: %s", SDL_GetError());
        stdPlatform_Printf("iosAngle: %s\n", iosAngle_errorMsg);
        goto fail;
    }

    iosAngle_display = iosAngle_GetMetalDisplay();
    if (iosAngle_display == EGL_NO_DISPLAY) {
        iosAngle_Fail("eglGetDisplay failed");
        goto fail;
    }

    EGLint eglMajor = 0, eglMinor = 0;
    if (!eglInitialize(iosAngle_display, &eglMajor, &eglMinor)) {
        iosAngle_Fail("eglInitialize failed");
        goto fail;
    }
    stdPlatform_Printf("iosAngle: EGL %d.%d, %s\n", eglMajor, eglMinor, eglQueryString(iosAngle_display, EGL_VENDOR));

    // Matches what the engine asks SDL for elsewhere: 8888 colour, 24-bit depth,
    // 8-bit stencil, no multisampling.
    const EGLint aConfigAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RED_SIZE,        8,
        EGL_GREEN_SIZE,      8,
        EGL_BLUE_SIZE,       8,
        EGL_ALPHA_SIZE,      8,
        EGL_DEPTH_SIZE,      24,
        EGL_STENCIL_SIZE,    8,
        EGL_NONE
    };

    EGLConfig config = NULL;
    EGLint numConfigs = 0;
    if (!eglChooseConfig(iosAngle_display, aConfigAttribs, &config, 1, &numConfigs) || numConfigs < 1) {
        iosAngle_Fail("eglChooseConfig found no GLES3 config");
        goto fail;
    }

    iosAngle_surface = eglCreateWindowSurface(iosAngle_display, config, (EGLNativeWindowType)pLayer, NULL);
    if (iosAngle_surface == EGL_NO_SURFACE) {
        iosAngle_Fail("eglCreateWindowSurface failed");
        goto fail;
    }

    const EGLint aContextAttribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 0,
        EGL_NONE
    };
    iosAngle_context = eglCreateContext(iosAngle_display, config, EGL_NO_CONTEXT, aContextAttribs);
    if (iosAngle_context == EGL_NO_CONTEXT) {
        iosAngle_Fail("eglCreateContext failed");
        goto fail;
    }

    if (!eglMakeCurrent(iosAngle_display, iosAngle_surface, iosAngle_surface, iosAngle_context)) {
        iosAngle_Fail("eglMakeCurrent failed");
        goto fail;
    }

    stdPlatform_Printf("iosAngle: %s / %s\n",
                       (const char*)glGetString(GL_RENDERER),
                       (const char*)glGetString(GL_VERSION));
    return 1;

fail:
    iosAngle_DestroyContext();
    return 0;
}

void iosAngle_DestroyContext(void)
{
    if (iosAngle_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(iosAngle_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (iosAngle_context != EGL_NO_CONTEXT) {
            eglDestroyContext(iosAngle_display, iosAngle_context);
        }
        if (iosAngle_surface != EGL_NO_SURFACE) {
            eglDestroySurface(iosAngle_display, iosAngle_surface);
        }
        eglTerminate(iosAngle_display);
    }

    iosAngle_context = EGL_NO_CONTEXT;
    iosAngle_surface = EGL_NO_SURFACE;
    iosAngle_display = EGL_NO_DISPLAY;

    // The view has to outlive the surface that draws into its layer, so it goes last.
    if (iosAngle_metalView) {
        SDL_Metal_DestroyView(iosAngle_metalView);
        iosAngle_metalView = NULL;
    }
}

static uint64_t iosAngle_Ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

// The tray's FPS at 60 on a 120 Hz screen: the screen is asked for 60, but
// frames could still come at the screen's 120 (vsync), so they are kept at
// least 1/60 s apart here (the waits are battery saved). At 120 the screen
// alone paces them.
static void iosAngle_FrameCap(void)
{
    static uint64_t next = 0;
    const int fps = iosTouch_MaxFps();
    if (fps <= 0 || fps >= 120) {
        next = 0;
        return;
    }
    const uint64_t period = 1000000000ull / (uint64_t)fps;
    const uint64_t now = iosAngle_Ns();
    // (far behind, after a pause or a load: from now on)
    if (next == 0 || now > next + period) next = now;
    if (now < next) {
        const uint64_t wait = next - now;
        struct timespec ts = { (time_t)(wait / 1000000000ull), (long)(wait % 1000000000ull) };
        nanosleep(&ts, NULL);
    }
    next += period;
}

// The frame-time log: every 5 s of play, the frames a second and the time
// per frame (the average and the worst), with the rate asked for. Only
// frames of play count (iosAngle_MarkGameFrame); a gap of over 0.5 s (a
// menu, a load, the app away) starts the count again.
static int iosAngle_bGameFrame = 0;

void iosAngle_MarkGameFrame(void)
{
    iosAngle_bGameFrame = 1;
}

static void iosAngle_LogFrame(void)
{
    static uint64_t tLast = 0, tStart = 0, sum = 0, worst = 0;
    static int frames = 0;
    const uint64_t now = iosAngle_Ns();
    if (!iosAngle_bGameFrame) {
        tLast = 0;
        return;
    }
    iosAngle_bGameFrame = 0;
    if (tLast == 0 || now - tLast > 500000000ull) {
        tLast = tStart = now;
        sum = worst = 0;
        frames = 0;
        return;
    }
    const uint64_t dt = now - tLast;
    tLast = now;
    frames++;
    sum += dt;
    if (dt > worst) worst = dt;
    if (now - tStart >= 5000000000ull) {
        printf("iOS: frames %.1f fps; per frame %.2f ms (worst %.1f); asked for %d\n",
               frames * 1e9 / (double)(now - tStart), sum * 1e-6 / frames, worst * 1e-6, iosTouch_MaxFps());
        fflush(stdout);
        tStart = now;
        sum = worst = 0;
        frames = 0;
    }
}

void iosAngle_SwapBuffers(void)
{
    if (iosAngle_display == EGL_NO_DISPLAY || iosAngle_surface == EGL_NO_SURFACE) {
        return;
    }
    iosAngle_FrameCap();
    eglSwapBuffers(iosAngle_display, iosAngle_surface);
    iosAngle_LogFrame();
}

void iosAngle_SetSwapInterval(int interval)
{
    if (iosAngle_display == EGL_NO_DISPLAY) {
        return;
    }
    eglSwapInterval(iosAngle_display, interval ? 1 : 0);
}

#endif // TARGET_IOS
