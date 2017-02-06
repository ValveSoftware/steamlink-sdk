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

#include "qtwebenginecoreglobal_p.h"

#include <QGuiApplication>
#ifndef QT_NO_OPENGL
# include <QOpenGLContext>
#endif
#include <QThread>

#ifndef QT_NO_OPENGL
QT_BEGIN_NAMESPACE
Q_GUI_EXPORT void qt_gl_set_global_share_context(QOpenGLContext *context);
Q_GUI_EXPORT QOpenGLContext *qt_gl_global_share_context();
QT_END_NAMESPACE
#endif

namespace QtWebEngineCore {
#ifndef QT_NO_OPENGL
static QOpenGLContext *shareContext;

static void deleteShareContext()
{
    delete shareContext;
    shareContext = 0;
}

#endif
// ### Qt 6: unify this logic and Qt::AA_ShareOpenGLContexts.
// QtWebEngine::initialize was introduced first and meant to be called
// after the QGuiApplication creation, when AA_ShareOpenGLContexts fills
// the same need but the flag has to be set earlier.

QWEBENGINE_PRIVATE_EXPORT void initialize()
{
#ifndef QT_NO_OPENGL
#ifdef Q_OS_WIN32
    qputenv("QT_D3DCREATE_MULTITHREADED", "1");
#endif

    // No need to override the shared context if QApplication already set one (e.g with Qt::AA_ShareOpenGLContexts).
    if (qt_gl_global_share_context())
        return;

    QCoreApplication *app = QCoreApplication::instance();
    if (!app) {
        qFatal("QtWebEngine::initialize() must be called after the construction of the application object.");
        return;
    }

#if (QT_VERSION >= QT_VERSION_CHECK(5, 6, 1))
    // Bail out silently if the user did not construct a QGuiApplication.
    if (!qobject_cast<QGuiApplication *>(app))
        return;
#endif

    if (app->thread() != QThread::currentThread()) {
        qFatal("QtWebEngine::initialize() must be called from the Qt gui thread.");
        return;
    }

    if (shareContext)
        return;

    shareContext = new QOpenGLContext;
    shareContext->create();
    qAddPostRoutine(deleteShareContext);
    qt_gl_set_global_share_context(shareContext);

    // Classes like QOpenGLWidget check for the attribute
    app->setAttribute(Qt::AA_ShareOpenGLContexts);
#endif // QT_NO_OPENGL
}
} // namespace QtWebEngineCore
