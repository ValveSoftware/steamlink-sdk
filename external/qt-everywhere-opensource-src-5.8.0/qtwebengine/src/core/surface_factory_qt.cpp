/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
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

#if defined(USE_OZONE)

#include <EGL/egl.h>

#ifndef QT_LIBDIR_EGL
#define QT_LIBDIR_EGL "/usr/lib"
#endif
#ifndef QT_LIBDIR_GLES2
#define QT_LIBDIR_GLES2 QT_LIBDIR_EGL
#endif

namespace QtWebEngineCore {

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
    base::FilePath libEGLPath = QtWebEngineCore::toFilePath(QT_LIBDIR_EGL);
    libEGLPath = libEGLPath.Append("libEGL.so.1");
    base::NativeLibrary eglLibrary = LoadLibrary(libEGLPath);
    if (!eglLibrary)
        return false;

    base::FilePath libGLES2Path = QtWebEngineCore::toFilePath(QT_LIBDIR_GLES2);
    libGLES2Path = libGLES2Path.Append("libGLESv2.so.2");
    base::NativeLibrary gles2Library = LoadLibrary(libGLES2Path);
    if (!gles2Library)
        return false;

    gl::GLGetProcAddressProc get_proc_address = reinterpret_cast<gl::GLGetProcAddressProc>(base::GetFunctionPointerFromNativeLibrary(eglLibrary, "eglGetProcAddress"));
    if (!get_proc_address) {
        LOG(ERROR) << "eglGetProcAddress not found.";
        base::UnloadNativeLibrary(eglLibrary);
        base::UnloadNativeLibrary(gles2Library);
        return false;
    }

    gl::SetGLGetProcAddressProc(get_proc_address);
    gl::AddGLNativeLibrary(eglLibrary);
    gl::AddGLNativeLibrary(gles2Library);
    return true;
}

intptr_t SurfaceFactoryQt::GetNativeDisplay()
{
    static void *display = GLContextHelper::getNativeDisplay();

    if (display)
        return reinterpret_cast<intptr_t>(display);

    return reinterpret_cast<intptr_t>(EGL_DEFAULT_DISPLAY);
}

} // namespace QtWebEngineCore

#endif // defined(USE_OZONE)

