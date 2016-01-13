/****************************************************************************
*
*    Copyright 2012 - 2014 Vivante Corporation, Sunnyvale, California.
*    All Rights Reserved.
*
*    Permission is hereby granted, free of charge, to any person obtaining
*    a copy of this software and associated documentation files (the
*    'Software'), to deal in the Software without restriction, including
*    without limitation the rights to use, copy, modify, merge, publish,
*    distribute, sub license, and/or sell copies of the Software, and to
*    permit persons to whom the Software is furnished to do so, subject
*    to the following conditions:
*
*    The above copyright notice and this permission notice (including the
*    next paragraph) shall be included in all copies or substantial
*    portions of the Software.
*
*    THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
*    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
*    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
*    IN NO EVENT SHALL VIVANTE AND/OR ITS SUPPLIERS BE LIABLE FOR ANY
*    CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*    TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
*    SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*****************************************************************************/



/*
 * Vivante specific definitions and declarations for EGL library.
 */

#ifndef __eglvivante_h_
#define __eglvivante_h_

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32) || defined(__VC32__) && !defined(__CYGWIN__) && !defined(__SCITECH_SNAP__)
/* Win32 and Windows CE platforms. */
#include <windows.h>
typedef HDC             EGLNativeDisplayType;
typedef HWND            EGLNativeWindowType;
typedef HBITMAP         EGLNativePixmapType;

/*####modified for marvell-bg2*/
#elif defined(EGL_API_DFB)
/*####end for marvell-bg2*/

#include <directfb.h>
typedef IDirectFB * EGLNativeDisplayType;
typedef IDirectFBWindow *  EGLNativeWindowType;
typedef struct _DFBPixmap *  EGLNativePixmapType;

EGLNativePixmapType
dfbCreatePixmap(
    EGLNativeDisplayType Display,
    int Width,
    int Height
    );

EGLNativePixmapType
dfbCreatePixmapWithBpp(
    EGLNativeDisplayType Display,
    int Width,
    int Height,
    int BitsPerPixel
    );

void
dfbGetPixmapInfo(
    EGLNativePixmapType Pixmap,
    int * Width,
    int * Height,
    int * BitsPerPixel,
    int * Stride,
    void* * Bits
    );

void
dfbDestroyPixmap(
    EGLNativePixmapType Pixmap
    );

/*####modified for marvell-bg2*/
#elif defined(EGL_API_FB)
/*####end for marvell-bg2*/

#if defined(WL_EGL_PLATFORM)
/* Wayland types for client apps. */
typedef struct wl_display *      EGLNativeDisplayType;
typedef struct wl_egl_window *   EGLNativeWindowType;
typedef struct wl_egl_pixmap *   EGLNativePixmapType;

#else
/* Linux platform for FBDEV. */
typedef struct _FBDisplay * EGLNativeDisplayType;
typedef struct _FBWindow *  EGLNativeWindowType;
typedef struct _FBPixmap *  EGLNativePixmapType;
#endif
/*####modified for marvell-bg2*/
/* some native egl options */
#define EGL_OPTION_MVGFX_DEBUG          0       /*debug level, the max, the detailed*/
#define EGL_OPTION_MVGFX_PLANE          1       /*gfx plane, only 0 valid for bg2-cd*/
#define EGL_OPTION_MVGFX_RESOLUTION     2       /*framebuffer size*/
#define EGL_OPTION_MVGFX_AUTOSCALING    3       /*auto scaling*/
#define EGL_OPTION_MVGFX_MULTI_BUFFER   4       /*framebuffer number*/

#define DEFAULT_EGL_OPTION_MVGFX_DEBUG  0
#define DEFAULT_EGL_OPTION_MVGFX_PLANE  0
#define DEFAULT_EGL_OPTION_MVGFX_AUTOSCALING   1
#define DEFAULT_EGL_OPTION_MVGFX_MULTI_BUFFER  3

/**
 * fbSetDisplayOption: set native Display options
 * params:
 * option(IN)                       value1(IN)                      value2(IN)
 * EGL_OPTION_MVGFX_DEBUG           0-3(default 0)                  N/A
 * EGL_OPTION_MVGFX_PLANE           0-3(default 0)                  N/A
 * EGL_OPTION_MVGFX_RESOLUTION      1920*1080,1280*720,960*540...(default 1280*720)
 * EGL_OPTION_MVGFX_AUTOSCALING     0-1(default 1)                  N/A
 * EGL_OPTION_MVGFX_MULTI_BUFFER    1-3(default 3)                  N/A
 *
 * return: 0 for success, 1 for error.
 */
int fbSetDisplayOption(
    int option,
    int value1,
    int value2
    );

/**
 * fbGetDisplayOption: get native Display options
 * params:
 * option(IN)                       value1(OUT)                    value2(OUT)
 * EGL_OPTION_MVGFX_DEBUG           current debug level              N/A
 * EGL_OPTION_MVGFX_PLANE           current gfx plane                N/A
 * EGL_OPTION_MVGFX_RESOLUTION      current width               current height
 * EGL_OPTION_MVGFX_AUTOSCALING     current auto scaling              N/A
 * EGL_OPTION_MVGFX_MULTI_BUFFER    current framebuffer number        N/A
 *
 * return: 0 for success, 1 for error.
 */
int fbGetDisplayOption(
    int option,
    int *value1,
    int *value2
    );
/*####end for marvell-bg2*/

EGLNativeDisplayType
fbGetDisplay(
    void *context
    );

EGLNativeDisplayType
fbGetDisplayByIndex(
    int DisplayIndex
    );

void
fbGetDisplayGeometry(
    EGLNativeDisplayType Display,
    int * Width,
    int * Height
    );

void
fbGetDisplayInfo(
    EGLNativeDisplayType Display,
    int * Width,
    int * Height,
    unsigned long * Physical,
    int * Stride,
    int * BitsPerPixel
    );

void
fbDestroyDisplay(
    EGLNativeDisplayType Display
    );

EGLNativeWindowType
fbCreateWindow(
    EGLNativeDisplayType Display,
    int X,
    int Y,
    int Width,
    int Height
    );

void
fbGetWindowGeometry(
    EGLNativeWindowType Window,
    int * X,
    int * Y,
    int * Width,
    int * Height
    );

void
fbGetWindowInfo(
    EGLNativeWindowType Window,
    int * X,
    int * Y,
    int * Width,
    int * Height,
    int * BitsPerPixel,
    unsigned int * Offset
    );

void
fbDestroyWindow(
    EGLNativeWindowType Window
    );

EGLNativePixmapType
fbCreatePixmap(
    EGLNativeDisplayType Display,
    int Width,
    int Height
    );

EGLNativePixmapType
fbCreatePixmapWithBpp(
    EGLNativeDisplayType Display,
    int Width,
    int Height,
    int BitsPerPixel
    );

void
fbGetPixmapGeometry(
    EGLNativePixmapType Pixmap,
    int * Width,
    int * Height
    );

void
fbGetPixmapInfo(
    EGLNativePixmapType Pixmap,
    int * Width,
    int * Height,
    int * BitsPerPixel,
    int * Stride,
    void ** Bits
    );

void
fbDestroyPixmap(
    EGLNativePixmapType Pixmap
    );

#elif defined(__ANDROID__) || defined(ANDROID)

struct egl_native_pixmap_t;

#if ANDROID_SDK_VERSION >= 9
    #include <android/native_window.h>

    typedef struct ANativeWindow*           EGLNativeWindowType;
    typedef struct egl_native_pixmap_t*     EGLNativePixmapType;
    typedef void*                           EGLNativeDisplayType;
#else
    struct android_native_window_t;
    typedef struct android_native_window_t*    EGLNativeWindowType;
    typedef struct egl_native_pixmap_t *        EGLNativePixmapType;
    typedef void*                               EGLNativeDisplayType;
#endif

/*####modified for marvell-bg2*/
#elif defined(__APPLE__)
/*####end for marvell-bg2*/

/* X11 platform. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

typedef Display *   EGLNativeDisplayType;
typedef Window      EGLNativeWindowType;

#ifdef CUSTOM_PIXMAP
typedef void *      EGLNativePixmapType;
#else
typedef Pixmap      EGLNativePixmapType;
#endif /* CUSTOM_PIXMAP */

#elif defined(__QNXNTO__)
#include <screen/screen.h>

/* VOID */
typedef int              EGLNativeDisplayType;
typedef screen_window_t  EGLNativeWindowType;
typedef screen_pixmap_t  EGLNativePixmapType;

#else

#error "Platform not recognized"

/* VOID */
typedef void *  EGLNativeDisplayType;
typedef void *  EGLNativeWindowType;
typedef void *  EGLNativePixmapType;

#endif

#if defined(__EGL_EXPORTS) && !defined(EGLAPI)
#if defined(_WIN32) && !defined(__SCITECH_SNAP__)
#  define EGLAPI    __declspec(dllexport)
# else
#  define EGLAPI
# endif
#endif

#ifndef EGL_EGLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#endif

#ifdef __cplusplus
}
#endif

#endif /* __eglvivante_h_ */
