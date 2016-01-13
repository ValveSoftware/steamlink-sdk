/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "surface_factory_qt.h"

#include "gl_context_qt.h"
#include "type_conversion.h"

#include "base/files/file_path.h"
#include "base/native_library.h"
#include "ui/gl/gl_implementation.h"

#include <QGuiApplication>

#if defined(USE_OZONE) || defined(OS_ANDROID)
#include <EGL/egl.h>

#ifndef QT_LIBDIR_EGL
#define QT_LIBDIR_EGL "/usr/lib"
#endif
#ifndef QT_LIBDIR_GLES2
#define QT_LIBDIR_GLES2 QT_LIBDIR_EGL
#endif

base::NativeLibrary LoadLibrary(const base::FilePath& filename) {
    base::NativeLibraryLoadError error;
    base::NativeLibrary library = base::LoadNativeLibrary(filename, &error);
    if (!library) {
        LOG(ERROR) << "Failed to load " << filename.MaybeAsASCII() << ": " << error.ToString();
        return NULL;
    }
    return library;
}

bool SurfaceFactoryQt::LoadEGLGLES2Bindings(AddGLLibraryCallback add_gl_library, SetGLGetProcAddressProcCallback set_gl_get_proc_address)
{
#if defined(OS_ANDROID)
    // This is done in gl_implementation_android.cc for now. We might need to switch if we
    // start supporting the emulator platform but that would be a more intrusive change.
    Q_UNREACHABLE();
    return false;
#else
    base::FilePath libEGLPath = toFilePath(QT_LIBDIR_EGL);
    libEGLPath = libEGLPath.Append("libEGL.so");
    base::NativeLibrary eglLibrary = LoadLibrary(libEGLPath);
    if (!eglLibrary)
        return false;

    base::FilePath libGLES2Path = toFilePath(QT_LIBDIR_GLES2);
    libGLES2Path = libGLES2Path.Append("libGLESv2.so");
    base::NativeLibrary gles2Library = LoadLibrary(libGLES2Path);
    if (!gles2Library)
        return false;

    gfx::GLGetProcAddressProc get_proc_address = reinterpret_cast<gfx::GLGetProcAddressProc>(base::GetFunctionPointerFromNativeLibrary(eglLibrary, "eglGetProcAddress"));
    if (!get_proc_address) {
        LOG(ERROR) << "eglGetProcAddress not found.";
        base::UnloadNativeLibrary(eglLibrary);
        base::UnloadNativeLibrary(gles2Library);
        return false;
    }

    gfx::SetGLGetProcAddressProc(get_proc_address);
    gfx::AddGLNativeLibrary(eglLibrary);
    gfx::AddGLNativeLibrary(gles2Library);
    return true;
#endif
}

intptr_t SurfaceFactoryQt::GetNativeDisplay()
{
    static void *display = GLContextHelper::getNativeDisplay();

    if (display)
        return reinterpret_cast<intptr_t>(display);

    return reinterpret_cast<intptr_t>(EGL_DEFAULT_DISPLAY);
}

#endif

