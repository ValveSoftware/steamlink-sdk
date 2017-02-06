// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_PUBLIC_CAST_EGL_PLATFORM_H_
#define CHROMECAST_PUBLIC_CAST_EGL_PLATFORM_H_

namespace chromecast {

struct Size;

// Interface representing all the hardware-specific elements of an Ozone
// implementation for Cast.  Supply an implementation of this interface
// to OzonePlatformCast to create a complete Ozone implementation.
class CastEglPlatform {
 public:
  typedef void* (*GLGetProcAddressProc)(const char* name);
  typedef void* NativeDisplayType;
  typedef void* NativeWindowType;

  virtual ~CastEglPlatform() {}

  // Returns an array of EGL properties, which can be used in any EGL function
  // used to select a display configuration. Note that all properties should be
  // immediately followed by the corresponding desired value and array should be
  // terminated with EGL_NONE. Ownership of the array is not transferred to
  // caller. desired_list contains list of desired EGL properties and values.
  virtual const int* GetEGLSurfaceProperties(const int* desired_list) = 0;

  // Initialize/ShutdownHardware are called at most once each over the object's
  // lifetime.  Initialize will be called before creating display type or
  // window.  If Initialize fails, return false (Shutdown will still be called).
  virtual bool InitializeHardware() = 0;
  virtual void ShutdownHardware() = 0;

  // These three are called once after hardware is successfully initialized.
  // The implementation must load the libraries containing EGL and GLES2
  // bindings (return the pointer obtained from dlopen).  It must also supply
  // a function pointer to eglGetProcAddress or equivalent.
  virtual void* GetEglLibrary() = 0;
  virtual void* GetGles2Library() = 0;
  virtual GLGetProcAddressProc GetGLProcAddressProc() = 0;

  // Creates/destroys an EGLNativeDisplayType.  These may be called multiple
  // times over the object's lifetime, for example to release the display when
  // switching to an external application.  There will be at most one display
  // type at a time.
  virtual NativeDisplayType CreateDisplayType(const Size& size) = 0;
  virtual void DestroyDisplayType(NativeDisplayType display_type) = 0;

  // Creates/destroys an EGLNativeWindow.  There will be at most one window at a
  // time, created within a valid display type.
  virtual NativeWindowType CreateWindow(NativeDisplayType display_type,
                                        const Size& size) = 0;
  virtual void DestroyWindow(NativeWindowType window) = 0;

  // Specifies if creating multiple surfaces on a window is broken on this
  // platform and a new window is required. This should return false on most
  // implementations.
  virtual bool MultipleSurfaceUnsupported() = 0;
};

}  // namespace chromecast

#endif  // CHROMECAST_PUBLIC_CAST_EGL_PLATFORM_H_
