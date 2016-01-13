/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
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
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QPLATFORMINTEGRATION_WAYLAND_H
#define QPLATFORMINTEGRATION_WAYLAND_H

#include <qpa/qplatformintegration.h>

#include <QtWaylandClient/private/qwaylandclientexport_p.h>
QT_BEGIN_NAMESPACE

class QWaylandBuffer;
class QWaylandDisplay;
class QWaylandClientBufferIntegration;
class QWaylandServerBufferIntegration;
class QWaylandShellIntegration;

class Q_WAYLAND_CLIENT_EXPORT QWaylandIntegration : public QPlatformIntegration
{
public:
    QWaylandIntegration();
    ~QWaylandIntegration();

    bool hasCapability(QPlatformIntegration::Capability cap) const;
    QPlatformWindow *createPlatformWindow(QWindow *window) const;
#ifndef QT_NO_OPENGL
    QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const;
#endif
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const;

    QAbstractEventDispatcher *createEventDispatcher() const;
    void initialize();

    QPlatformFontDatabase *fontDatabase() const;

    QPlatformNativeInterface *nativeInterface() const;

    QPlatformClipboard *clipboard() const;

    QPlatformDrag *drag() const;

    QPlatformInputContext *inputContext() const;

    QVariant styleHint(StyleHint hint) const;

    QPlatformAccessibility *accessibility() const;

    QPlatformServices *services() const;

    QWaylandDisplay *display() const;

    QStringList themeNames() const;

    QPlatformTheme *createPlatformTheme(const QString &name) const;

    virtual QWaylandClientBufferIntegration *clientBufferIntegration() const;
    virtual QWaylandServerBufferIntegration *serverBufferIntegration() const;
    virtual QWaylandShellIntegration *shellIntegration() const;
protected:
    QWaylandClientBufferIntegration *mClientBufferIntegration;
    QWaylandServerBufferIntegration *mServerBufferIntegration;
    QWaylandShellIntegration *mShellIntegration;
private:
    void initializeClientBufferIntegration();
    void initializeServerBufferIntegration();
    void initializeShellIntegration();
    QPlatformFontDatabase *mFontDb;
    QPlatformClipboard *mClipboard;
    QPlatformDrag *mDrag;
    QWaylandDisplay *mDisplay;
    QPlatformNativeInterface *mNativeInterface;
    QScopedPointer<QPlatformInputContext> mInputContext;
    QPlatformAccessibility *mAccessibility;
    bool mClientBufferIntegrationInitialized;
    bool mServerBufferIntegrationInitialized;
    bool mShellIntegrationInitialized;

    friend class QWaylandDisplay;
};

QT_END_NAMESPACE

#endif
