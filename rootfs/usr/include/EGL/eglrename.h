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



#ifndef __eglrename_h_
#define __eglrename_h_

#if defined(_EGL_APPENDIX)

#define _EGL_RENAME_2(api, appendix)    api ## appendix
#define _EGL_RENAME_1(api, appendix)    _EGL_RENAME_2(api, appendix)
#define gcmEGL(api)                     _EGL_RENAME_1(api, _EGL_APPENDIX)

#define eglBindAPI                      gcmEGL(eglBindAPI)
#define eglBindTexImage                 gcmEGL(eglBindTexImage)
#define eglChooseConfig                 gcmEGL(eglChooseConfig)
#define eglClientWaitSyncKHR            gcmEGL(eglClientWaitSyncKHR)
#define eglCopyBuffers                  gcmEGL(eglCopyBuffers)
#define eglCreateContext                gcmEGL(eglCreateContext)
#define eglCreateImageKHR               gcmEGL(eglCreateImageKHR)
#define eglCreatePbufferFromClientBuffer \
            gcmEGL(eglCreatePbufferFromClientBuffer)
#define eglCreatePbufferSurface         gcmEGL(eglCreatePbufferSurface)
#define eglCreatePixmapSurface          gcmEGL(eglCreatePixmapSurface)
#define eglCreateSyncKHR                gcmEGL(eglCreateSyncKHR)
#define eglCreateWindowSurface          gcmEGL(eglCreateWindowSurface)
#define eglDestroyContext               gcmEGL(eglDestroyContext)
#define eglDestroyImageKHR              gcmEGL(eglDestroyImageKHR)
#define eglDestroyImageKHR              gcmEGL(eglDestroyImageKHR)
#define eglDestroySurface               gcmEGL(eglDestroySurface)
#define eglDestroySyncKHR               gcmEGL(eglDestroySyncKHR)
#define eglGetConfigAttrib              gcmEGL(eglGetConfigAttrib)
#define eglGetConfigs                   gcmEGL(eglGetConfigs)
#define eglGetCurrentContext            gcmEGL(eglGetCurrentContext)
#define eglGetCurrentDisplay            gcmEGL(eglGetCurrentDisplay)
#define eglGetCurrentSurface            gcmEGL(eglGetCurrentSurface)
#define eglGetDisplay                   gcmEGL(eglGetDisplay)
#define eglGetError                     gcmEGL(eglGetError)
#define eglGetProcAddress               gcmEGL(eglGetProcAddress)
#define eglGetSyncAttribKHR             gcmEGL(eglGetSyncAttribKHR)
#define eglInitialize                   gcmEGL(eglInitialize)
#define eglLockSurfaceKHR               gcmEGL(eglLockSurfaceKHR)
#define eglMakeCurrent                  gcmEGL(eglMakeCurrent)
#define eglQueryAPI                     gcmEGL(eglQueryAPI)
#define eglQueryContext                 gcmEGL(eglQueryContext)
#define eglQueryString                  gcmEGL(eglQueryString)
#define eglQuerySurface                 gcmEGL(eglQuerySurface)
#define eglReleaseTexImage              gcmEGL(eglReleaseTexImage)
#define eglReleaseThread                gcmEGL(eglReleaseThread)
#define eglSignalSyncKHR                gcmEGL(eglSignalSyncKHR)
#define eglSurfaceAttrib                gcmEGL(eglSurfaceAttrib)
#define eglSwapBuffers                  gcmEGL(eglSwapBuffers)
#define eglSwapBuffersRegionEXT         gcmEGL(eglSwapBuffersRegionEXT)
#define eglSwapInterval                 gcmEGL(eglSwapInterval)
#define eglTerminate                    gcmEGL(eglTerminate)
#define eglUnlockSurfaceKHR             gcmEGL(eglUnlockSurfaceKHR)
#define eglWaitClient                   gcmEGL(eglWaitClient)
#define eglWaitGL                       gcmEGL(eglWaitGL)
#define eglWaitNative                   gcmEGL(eglWaitNative)
#define eglBindWaylandDisplayWL             gcmEGL(eglBindWaylandDisplayWL)
#define eglUnbindWaylandDisplayWL           gcmEGL(eglUnbindWaylandDisplayWL)
#define eglQueryWaylandBufferWL             gcmEGL(eglQueryWaylandBufferWL)
#endif /* _EGL_APPENDIX */
#endif /* __eglrename_h_ */
