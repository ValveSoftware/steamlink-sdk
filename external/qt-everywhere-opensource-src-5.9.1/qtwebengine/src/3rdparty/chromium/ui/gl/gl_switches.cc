// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "ui/gl/gl_switches.h"

namespace gl {

const char kGLImplementationDesktopName[]     = "desktop";
const char kGLImplementationCoreProfileName[] = "core_profile";
const char kGLImplementationOSMesaName[]      = "osmesa";
const char kGLImplementationAppleName[]       = "apple";
const char kGLImplementationEGLName[]         = "egl";
const char kGLImplementationANGLEName[]       = "angle";
const char kGLImplementationSwiftShaderName[] = "swiftshader";
const char kGLImplementationMockName[]        = "mock";

const char kANGLEImplementationDefaultName[]  = "default";
const char kANGLEImplementationD3D9Name[]     = "d3d9";
const char kANGLEImplementationD3D11Name[]    = "d3d11";
const char kANGLEImplementationOpenGLName[]   = "gl";
const char kANGLEImplementationOpenGLESName[] = "gles";
const char kANGLEImplementationNullName[]     = "null";

}  // namespace gl

namespace switches {

// Ask the GLX driver for the default context instead of trying to get the
// highest version possible.
const char kCreateDefaultGLContext[] = "create-default-gl-context";

// Disables use of D3D11.
const char kDisableD3D11[]                  = "disable-d3d11";

// Disables use of ES3 backend (use ES2 backend instead).
const char kDisableES3GLContext[]           = "disable-es3-gl-context";

// Stop the GPU from synchronizing on the vsync before presenting.
// We can select from the options below:
//  beginframe: Next frame can start without any delay on cc::scheduler in
//              renderer compositors.
//  gpu: Disable display and browser vsync.
//  default: Set both flags.
const char kDisableGpuVsync[]               = "disable-gpu-vsync";

// Turns on GPU logging (debug build only).
const char kEnableGPUServiceLogging[]       = "enable-gpu-service-logging";

// Turns on calling TRACE for every GL call.
const char kEnableGPUServiceTracing[]       = "enable-gpu-service-tracing";

// Select which ANGLE backend to use. Options are:
//  default: Attempts several ANGLE renderers until one successfully
//           initializes, varying ES support by platform.
//  d3d9: Legacy D3D9 renderer, ES2 only.
//  d3d11: D3D11 renderer, ES2 and ES3.
//  warp: D3D11 renderer using software rasterization, ES2 and ES3.
//  gl: Desktop GL renderer, ES2 and ES3.
//  gles: GLES renderer, ES2 and ES3.
const char kUseANGLE[]                      = "use-angle";

// Select which implementation of GL the GPU process should use. Options are:
//  desktop: whatever desktop OpenGL the user has installed (Linux and Mac
//           default).
//  egl: whatever EGL / GLES2 the user has installed (Windows default - actually
//       ANGLE).
//  osmesa: The OSMesa software renderer.
const char kUseGL[]                         = "use-gl";

const char kSwiftShaderPath[]               = "swiftshader-path";

// Inform Chrome that a GPU context will not be lost in power saving mode,
// screen saving mode, etc.  Note that this flag does not ensure that a GPU
// context will never be lost in any situations, say, a GPU reset.
const char kGpuNoContextLost[]              = "gpu-no-context-lost";

// Disables the use of DirectComposition to draw to the screen.
const char kDisableDirectComposition[] = "disable-direct-composition";

// Indicates whether the dual GPU switching is supported or not.
const char kSupportsDualGpus[]              = "supports-dual-gpus";

// Flag used for Linux tests: for desktop GL bindings, try to load this GL
// library first, but fall back to regular library if loading fails.
const char kTestGLLib[]                     = "test-gl-lib";

// Use hardware gpu, if available, for tests.
const char kUseGpuInTests[] = "use-gpu-in-tests";

// Enable OpenGL ES 3 APIs.
const char kEnableES3APIs[] = "enable-es3-apis";

// Disable OpenGL ES 3 APIs. This in turn will disable WebGL2.
const char kDisableES3APIs[] = "disable-es3-apis";

// Enable use of the SGI_video_sync extension, which can have
// driver/sandbox/window manager compatibility issues.
const char kEnableSgiVideoSync[] = "enable-sgi-video-sync";

// Disables GL drawing operations which produce pixel output. With this
// the GL output will not be correct but tests will run faster.
const char kDisableGLDrawingForTests[] = "disable-gl-drawing-for-tests";

// Forces the use of OSMesa instead of hardware gpu.
const char kOverrideUseGLWithOSMesaForTests[] =
    "override-use-gl-with-osmesa-for-tests";

// Disables specified comma separated GL Extensions if found.
const char kDisableGLExtensions[] = "disable-gl-extensions";

// Use EGL_KHR_swap_buffers_with_damage to implement PostSubBuffers
const char kEnableSwapBuffersWithDamage[] = "enable-swap-buffers-with-damage";

// This is the list of switches passed from this file that are passed from the
// GpuProcessHost to the GPU Process. Add your switch to this list if you need
// to read it in the GPU process, else don't add it.
const char* kGLSwitchesCopiedFromGpuProcessHost[] = {
    kDisableGpuVsync,
    kDisableD3D11,
    kEnableGPUServiceLogging,
    kEnableGPUServiceTracing,
    kEnableSgiVideoSync,
    kGpuNoContextLost,
    kDisableGLDrawingForTests,
    kOverrideUseGLWithOSMesaForTests,
    kUseANGLE,
    kDisableDirectComposition,
    kEnableSwapBuffersWithDamage,
};
const int kGLSwitchesCopiedFromGpuProcessHostNumSwitches =
    arraysize(kGLSwitchesCopiedFromGpuProcessHost);

}  // namespace switches
