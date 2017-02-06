/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickdesignerwindowmanager_p.h"
#include "private/qquickwindow_p.h"
#ifndef QT_NO_OPENGL
# include <QtQuick/private/qsgdefaultrendercontext_p.h>
#endif
#include <QtQuick/QQuickWindow>

QT_BEGIN_NAMESPACE

QQuickDesignerWindowManager::QQuickDesignerWindowManager()
    : m_sgContext(QSGContext::createDefaultContext())
{
    m_renderContext.reset(m_sgContext.data()->createRenderContext());
}

void QQuickDesignerWindowManager::show(QQuickWindow *window)
{
    makeOpenGLContext(window);
}

void QQuickDesignerWindowManager::hide(QQuickWindow *)
{
}

void QQuickDesignerWindowManager::windowDestroyed(QQuickWindow *)
{
}

void QQuickDesignerWindowManager::makeOpenGLContext(QQuickWindow *window)
{
#ifndef QT_NO_OPENGL
    if (!m_openGlContext) {
        m_openGlContext.reset(new QOpenGLContext());
        m_openGlContext->setFormat(window->requestedFormat());
        m_openGlContext->create();
        if (!m_openGlContext->makeCurrent(window))
            qWarning("QQuickWindow: makeCurrent() failed...");
        m_renderContext->initialize(m_openGlContext.data());
    } else {
        m_openGlContext->makeCurrent(window);
    }
#else
    Q_UNUSED(window)
#endif
}

void QQuickDesignerWindowManager::exposureChanged(QQuickWindow *)
{
}

QImage QQuickDesignerWindowManager::grab(QQuickWindow *)
{
    return QImage();
}

void QQuickDesignerWindowManager::maybeUpdate(QQuickWindow *)
{
}

QSGContext *QQuickDesignerWindowManager::sceneGraphContext() const
{
    return m_sgContext.data();
}

void QQuickDesignerWindowManager::createOpenGLContext(QQuickWindow *window)
{
    window->create();
    window->update();
}

void QQuickDesignerWindowManager::update(QQuickWindow *window)
{
    makeOpenGLContext(window);
}

QT_END_NAMESPACE


