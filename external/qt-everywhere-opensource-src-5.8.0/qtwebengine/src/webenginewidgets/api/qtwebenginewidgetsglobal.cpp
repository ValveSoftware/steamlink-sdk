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

#include "qtwebenginewidgetsglobal.h"

#include <QCoreApplication>
#include <QOpenGLContext>

namespace QtWebEngineCore
{
    extern void initialize();
}

QT_BEGIN_NAMESPACE

#ifndef QT_NO_OPENGL
Q_GUI_EXPORT QOpenGLContext *qt_gl_global_share_context();
#endif

static void initialize()
{
#ifndef QT_NO_OPENGL
    if (QCoreApplication::instance()) {
        //On window/ANGLE, calling QtWebEngine::initialize from DllMain will result in a crash.
        if (!qt_gl_global_share_context()) {
            qWarning("Qt WebEngine seems to be initialized from a plugin. Please "
                     "set Qt::AA_ShareOpenGLContexts using QCoreApplication::setAttribute "
                     "before constructing QGuiApplication.");
        }
        return;
    }
    //QCoreApplication is not yet instantiated, ensuring the call will be deferred
    qAddPreRoutine(QtWebEngineCore::initialize);
#endif // QT_NO_OPENGL
}

Q_CONSTRUCTOR_FUNCTION(initialize)

QT_END_NAMESPACE
