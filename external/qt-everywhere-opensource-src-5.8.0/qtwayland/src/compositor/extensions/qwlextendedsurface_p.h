/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef WLEXTENDEDSURFACE_H
#define WLEXTENDEDSURFACE_H

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

#include <wayland-server.h>

#include <QtWaylandCompositor/private/qwayland-server-surface-extension.h>
#include <QtWaylandCompositor/qwaylandsurface.h>
#include <QtWaylandCompositor/qwaylandcompositorextension.h>

#include <QtCore/QVariant>
#include <QtCore/QLinkedList>
#include <QtGui/QWindow>

QT_BEGIN_NAMESPACE

class QWaylandCompositor;
class QWaylandSurface;

namespace QtWayland {

class ExtendedSurface;

class Q_WAYLAND_COMPOSITOR_EXPORT SurfaceExtensionGlobal : public QWaylandCompositorExtensionTemplate<SurfaceExtensionGlobal>, public QtWaylandServer::qt_surface_extension
{
    Q_OBJECT
public:
    SurfaceExtensionGlobal(QWaylandCompositor *compositor);

Q_SIGNALS:
    void extendedSurfaceReady(ExtendedSurface *extSurface, QWaylandSurface *surface);

private:
    void surface_extension_get_extended_surface(Resource *resource,
                                                uint32_t id,
                                                struct wl_resource *surface);

};

class Q_WAYLAND_COMPOSITOR_EXPORT ExtendedSurface : public QWaylandCompositorExtensionTemplate<ExtendedSurface>, public QtWaylandServer::qt_extended_surface
{
    Q_OBJECT
    Q_PROPERTY(Qt::ScreenOrientations contentOrientationMask READ contentOrientationMask NOTIFY contentOrientationMaskChanged)
    Q_PROPERTY(WindowFlags windowFlags READ windowFlags NOTIFY windowFlagsChanged)
    Q_FLAGS(WindowFlag WindowFlags)
public:
    enum WindowFlag {
        OverridesSystemGestures     = 0x0001,
        StaysOnTop                  = 0x0002,
        BypassWindowManager         = 0x0004
    };
    Q_DECLARE_FLAGS(WindowFlags, WindowFlag)

    ExtendedSurface(struct wl_client *client, uint32_t id, int version, QWaylandSurface *surface);
    ~ExtendedSurface();

    void sendGenericProperty(const QString &name, const QVariant &variant);

    void sendOnScreenVisibilityChange(bool onScreen);
    void setVisibility(QWindow::Visibility visibility);

    void setParentSurface(QWaylandSurface *s);

    Qt::ScreenOrientations contentOrientationMask() const;

    WindowFlags windowFlags() const { return m_windowFlags; }

    QVariantMap windowProperties() const;
    QVariant windowProperty(const QString &propertyName) const;
    void setWindowProperty(const QString &name, const QVariant &value);

Q_SIGNALS:
    void contentOrientationMaskChanged();
    void windowFlagsChanged();
    void windowPropertyChanged(const QString &name, const QVariant &value);
    void raiseRequested();
    void lowerRequested();

private:
    void setWindowPropertyImpl(const QString &name, const QVariant &value);

    QWaylandSurface *m_surface;

    Qt::ScreenOrientations m_contentOrientationMask;

    WindowFlags m_windowFlags;

    QByteArray m_authenticationToken;
    QVariantMap m_windowProperties;

    void extended_surface_update_generic_property(Resource *resource,
                                                  const QString &name,
                                                  struct wl_array *value) Q_DECL_OVERRIDE;

    void extended_surface_set_content_orientation_mask(Resource *resource,
                                                       int32_t orientation) Q_DECL_OVERRIDE;

    void extended_surface_set_window_flags(Resource *resource,
                                           int32_t flags) Q_DECL_OVERRIDE;

    void extended_surface_destroy_resource(Resource *) Q_DECL_OVERRIDE;
    void extended_surface_raise(Resource *) Q_DECL_OVERRIDE;
    void extended_surface_lower(Resource *) Q_DECL_OVERRIDE;
};

}

QT_END_NAMESPACE

#endif // WLEXTENDEDSURFACE_H
