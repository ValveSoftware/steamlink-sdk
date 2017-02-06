/****************************************************************************
**
** Copyright (C) 2014-2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
** Copyright (C) 2013 Klarälvdalens Datakonsult AB (KDAB).
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

#ifndef QWAYLANDOUTPUT_P_H
#define QWAYLANDOUTPUT_P_H

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

#include <QtWaylandCompositor/qtwaylandcompositorglobal.h>
#include <QtWaylandCompositor/QWaylandOutput>
#include <QtWaylandCompositor/QWaylandClient>
#include <QtWaylandCompositor/QWaylandSurface>

#include <QtWaylandCompositor/private/qwayland-server-wayland.h>

#include <QtCore/QRect>
#include <QtCore/QVector>

#include <QtCore/private/qobject_p.h>

QT_BEGIN_NAMESPACE

struct QWaylandSurfaceViewMapper
{
    QWaylandSurfaceViewMapper()
        : surface(0)
        , views()
        , has_entered(false)
    {}

    QWaylandSurfaceViewMapper(QWaylandSurface *s, QWaylandView *v)
        : surface(s)
        , views(1, v)
        , has_entered(false)
    {}

    QWaylandView *maybePrimaryView() const
    {
        for (int i = 0; i < views.size(); i++) {
            if (surface && surface->primaryView() == views.at(i))
                return views.at(i);
        }
        return Q_NULLPTR;
    }

    QWaylandSurface *surface;
    QVector<QWaylandView *> views;
    bool has_entered;
};

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandOutputPrivate : public QObjectPrivate, public QtWaylandServer::wl_output
{
public:
    QWaylandOutputPrivate();

    ~QWaylandOutputPrivate();
    static QWaylandOutputPrivate *get(QWaylandOutput *output) { return output->d_func(); }

    void addView(QWaylandView *view, QWaylandSurface *surface);
    void removeView(QWaylandView *view, QWaylandSurface *surface);

    void sendGeometry(const Resource *resource);
    void sendGeometryInfo();

    void sendMode(const Resource *resource, const QWaylandOutputMode &mode);
    void sendModesInfo();

protected:
    void output_bind_resource(Resource *resource) Q_DECL_OVERRIDE;

private:
    QWaylandCompositor *compositor;
    QWindow *window;
    QString manufacturer;
    QString model;
    QPoint position;
    QVector<QWaylandOutputMode> modes;
    int currentMode;
    int preferredMode;
    QRect availableGeometry;
    QVector<QWaylandSurfaceViewMapper> surfaceViews;
    QSize physicalSize;
    QWaylandOutput::Subpixel subpixel;
    QWaylandOutput::Transform transform;
    int scaleFactor;
    bool sizeFollowsWindow;
    bool initialized;

    Q_DECLARE_PUBLIC(QWaylandOutput)
    Q_DISABLE_COPY(QWaylandOutputPrivate)
};


QT_END_NAMESPACE

#endif  /*QWAYLANDOUTPUT_P_H*/
