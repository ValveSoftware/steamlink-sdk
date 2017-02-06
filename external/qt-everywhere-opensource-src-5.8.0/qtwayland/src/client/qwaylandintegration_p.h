/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#ifndef QPLATFORMINTEGRATION_WAYLAND_H
#define QPLATFORMINTEGRATION_WAYLAND_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtWaylandClient/qtwaylandclientglobal.h>
#include <qpa/qplatformintegration.h>
#include <QtCore/QScopedPointer>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

class QWaylandBuffer;
class QWaylandDisplay;
class QWaylandClientBufferIntegration;
class QWaylandServerBufferIntegration;
class QWaylandShellIntegration;
class QWaylandInputDeviceIntegration;
class QWaylandInputDevice;

class Q_WAYLAND_CLIENT_EXPORT QWaylandIntegration : public QPlatformIntegration
{
public:
    QWaylandIntegration();
    ~QWaylandIntegration();

    bool hasCapability(QPlatformIntegration::Capability cap) const Q_DECL_OVERRIDE;
    QPlatformWindow *createPlatformWindow(QWindow *window) const Q_DECL_OVERRIDE;
#if QT_CONFIG(opengl)
    QPlatformOpenGLContext *createPlatformOpenGLContext(QOpenGLContext *context) const Q_DECL_OVERRIDE;
#endif
    QPlatformBackingStore *createPlatformBackingStore(QWindow *window) const Q_DECL_OVERRIDE;

    QAbstractEventDispatcher *createEventDispatcher() const Q_DECL_OVERRIDE;
    void initialize() Q_DECL_OVERRIDE;

    QPlatformFontDatabase *fontDatabase() const Q_DECL_OVERRIDE;

    QPlatformNativeInterface *nativeInterface() const Q_DECL_OVERRIDE;
#if QT_CONFIG(draganddrop)
    QPlatformClipboard *clipboard() const Q_DECL_OVERRIDE;
    QPlatformDrag *drag() const Q_DECL_OVERRIDE;
#endif
    QPlatformInputContext *inputContext() const Q_DECL_OVERRIDE;

    QVariant styleHint(StyleHint hint) const Q_DECL_OVERRIDE;

#if QT_CONFIG(accessibility)
    QPlatformAccessibility *accessibility() const Q_DECL_OVERRIDE;
#endif

    QPlatformServices *services() const Q_DECL_OVERRIDE;

    QWaylandDisplay *display() const;

    QStringList themeNames() const Q_DECL_OVERRIDE;

    QPlatformTheme *createPlatformTheme(const QString &name) const Q_DECL_OVERRIDE;

    QWaylandInputDevice *createInputDevice(QWaylandDisplay *display, int version, uint32_t id);

    virtual QWaylandClientBufferIntegration *clientBufferIntegration() const;
    virtual QWaylandServerBufferIntegration *serverBufferIntegration() const;
    virtual QWaylandShellIntegration *shellIntegration() const;

private:
    // NOTE: mDisplay *must* be destructed after mDrag and mClientBufferIntegration.
    // Do not move this definition into the private section at the bottom.
    QScopedPointer<QWaylandDisplay> mDisplay;

protected:
    QScopedPointer<QWaylandClientBufferIntegration> mClientBufferIntegration;
    QScopedPointer<QWaylandServerBufferIntegration> mServerBufferIntegration;
    QScopedPointer<QWaylandShellIntegration> mShellIntegration;
    QScopedPointer<QWaylandInputDeviceIntegration> mInputDeviceIntegration;

private:
    void initializeClientBufferIntegration();
    void initializeServerBufferIntegration();
    void initializeShellIntegration();
    void initializeInputDeviceIntegration();
    QWaylandShellIntegration *createShellIntegration(const QString& interfaceName);

    QScopedPointer<QPlatformFontDatabase> mFontDb;
#if QT_CONFIG(draganddrop)
    QScopedPointer<QPlatformClipboard> mClipboard;
    QScopedPointer<QPlatformDrag> mDrag;
#endif
    QScopedPointer<QPlatformNativeInterface> mNativeInterface;
    QScopedPointer<QPlatformInputContext> mInputContext;
#if QT_CONFIG(accessibility)
    QScopedPointer<QPlatformAccessibility> mAccessibility;
#endif
    bool mClientBufferIntegrationInitialized;
    bool mServerBufferIntegrationInitialized;
    bool mShellIntegrationInitialized;

    friend class QWaylandDisplay;
};

}

QT_END_NAMESPACE

#endif
