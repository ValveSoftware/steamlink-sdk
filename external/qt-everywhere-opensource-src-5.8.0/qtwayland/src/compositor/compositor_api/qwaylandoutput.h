/****************************************************************************
**
** Copyright (C) 2014-2016 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
** Copyright (C) 2013 Klarälvdalens Datakonsult AB (KDAB).
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

#ifndef QWAYLANDOUTPUT_H
#define QWAYLANDOUTPUT_H

#include <QtWaylandCompositor/qwaylandcompositorextension.h>
#include <QtWaylandCompositor/QWaylandOutputMode>
#include <QtCore/QObject>

#include <QObject>
#include <QRect>
#include <QSize>

struct wl_resource;

QT_BEGIN_NAMESPACE

class QWaylandOutputPrivate;
class QWaylandCompositor;
class QWindow;
class QWaylandSurface;
class QWaylandView;
class QWaylandClient;
class QWaylandOutputSpace;

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandOutput : public QWaylandObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QWaylandOutput)
    Q_PROPERTY(QWaylandCompositor *compositor READ compositor WRITE setCompositor NOTIFY compositorChanged)
    Q_PROPERTY(QWindow *window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(QString manufacturer READ manufacturer WRITE setManufacturer NOTIFY manufacturerChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QPoint position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QRect geometry READ geometry NOTIFY geometryChanged)
    Q_PROPERTY(QRect availableGeometry READ availableGeometry WRITE setAvailableGeometry NOTIFY availableGeometryChanged)
    Q_PROPERTY(QSize physicalSize READ physicalSize WRITE setPhysicalSize NOTIFY physicalSizeChanged)
    Q_PROPERTY(QWaylandOutput::Subpixel subpixel READ subpixel WRITE setSubpixel NOTIFY subpixelChanged)
    Q_PROPERTY(QWaylandOutput::Transform transform READ transform WRITE setTransform NOTIFY transformChanged)
    Q_PROPERTY(int scaleFactor READ scaleFactor WRITE setScaleFactor NOTIFY scaleFactorChanged)
    Q_PROPERTY(bool sizeFollowsWindow READ sizeFollowsWindow WRITE setSizeFollowsWindow NOTIFY sizeFollowsWindowChanged)
    Q_ENUMS(Subpixel Transform)

public:
    enum Subpixel {
      SubpixelUnknown = 0,
      SubpixelNone,
      SubpixelHorizontalRgb,
      SubpixelHorizontalBgr,
      SubpixelVerticalRgb,
      SubpixelVerticalBgr
    };
    Q_ENUM(Subpixel)

    enum Transform {
        TransformNormal = 0,
        Transform90,
        Transform180,
        Transform270,
        TransformFlipped,
        TransformFlipped90,
        TransformFlipped180,
        TransformFlipped270
    };
    Q_ENUM(Transform)

    QWaylandOutput();
    QWaylandOutput(QWaylandCompositor *compositor, QWindow *window);
    ~QWaylandOutput();

    static QWaylandOutput *fromResource(wl_resource *resource);
    struct ::wl_resource *resourceForClient(QWaylandClient *client) const;

    QWaylandCompositor *compositor() const;
    void setCompositor(QWaylandCompositor *compositor);

    QWindow *window() const;
    void setWindow(QWindow *window);

    QString manufacturer() const;
    void setManufacturer(const QString &manufacturer);

    QString model() const;
    void setModel(const QString &model);

    QPoint position() const;
    void setPosition(const QPoint &pt);

    QList<QWaylandOutputMode> modes() const;

    void addMode(const QWaylandOutputMode &mode, bool preferred = false);

    QWaylandOutputMode currentMode() const;
    void setCurrentMode(const QWaylandOutputMode &mode);

    QRect geometry() const;

    QRect availableGeometry() const;
    void setAvailableGeometry(const QRect &availableGeometry);

    QSize physicalSize() const;
    void setPhysicalSize(const QSize &size);

    Subpixel subpixel() const;
    void setSubpixel(const Subpixel &subpixel);

    Transform transform() const;
    void setTransform(const Transform &transform);

    int scaleFactor() const;
    void setScaleFactor(int scale);

    bool sizeFollowsWindow() const;
    void setSizeFollowsWindow(bool follow);

    bool physicalSizeFollowsSize() const;
    void setPhysicalSizeFollowsSize(bool follow);

    void frameStarted();
    void sendFrameCallbacks();

    void surfaceEnter(QWaylandSurface *surface);
    void surfaceLeave(QWaylandSurface *surface);

    virtual void update();

Q_SIGNALS:
    void compositorChanged();
    void windowChanged();
    void positionChanged();
    void geometryChanged();
    void modeAdded();
    void currentModeChanged();
    void availableGeometryChanged();
    void physicalSizeChanged();
    void scaleFactorChanged();
    void subpixelChanged();
    void transformChanged();
    void sizeFollowsWindowChanged();
    void physicalSizeFollowsSizeChanged();
    void manufacturerChanged();
    void modelChanged();
    void windowDestroyed();

private Q_SLOTS:
    void handleSetWidth(int newWidth);
    void handleSetHeight(int newHeight);
    void handleWindowDestroyed();

protected:
    bool event(QEvent *event) Q_DECL_OVERRIDE;

    virtual void initialize();
};

QT_END_NAMESPACE

#endif // QWAYLANDOUTPUT_H
