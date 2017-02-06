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

#ifndef QWAYLANDSCREEN_H
#define QWAYLANDSCREEN_H

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

#include <qpa/qplatformscreen.h>
#include <QtWaylandClient/qtwaylandclientglobal.h>

#include <QtWaylandClient/private/qwayland-wayland.h>

QT_BEGIN_NAMESPACE

namespace QtWaylandClient {

class QWaylandDisplay;
class QWaylandCursor;

class Q_WAYLAND_CLIENT_EXPORT QWaylandScreen : public QPlatformScreen, QtWayland::wl_output
{
public:
    QWaylandScreen(QWaylandDisplay *waylandDisplay, int version, uint32_t id);
    ~QWaylandScreen();

    void init();
    QWaylandDisplay *display() const;

    QRect geometry() const Q_DECL_OVERRIDE;
    int depth() const Q_DECL_OVERRIDE;
    QImage::Format format() const Q_DECL_OVERRIDE;

    QSizeF physicalSize() const Q_DECL_OVERRIDE;

    QDpi logicalDpi() const Q_DECL_OVERRIDE;
    QList<QPlatformScreen *> virtualSiblings() const Q_DECL_OVERRIDE;

    void setOrientationUpdateMask(Qt::ScreenOrientations mask) Q_DECL_OVERRIDE;

    Qt::ScreenOrientation orientation() const Q_DECL_OVERRIDE;
    int scale() const;
    qreal devicePixelRatio() const Q_DECL_OVERRIDE;
    qreal refreshRate() const Q_DECL_OVERRIDE;

    QString name() const Q_DECL_OVERRIDE { return mOutputName; }

    QPlatformCursor *cursor() const Q_DECL_OVERRIDE;
    QWaylandCursor *waylandCursor() const { return mWaylandCursor; };

    uint32_t outputId() const { return m_outputId; }
    ::wl_output *output() { return object(); }

    static QWaylandScreen *waylandScreenFromWindow(QWindow *window);

private:
    void output_mode(uint32_t flags, int width, int height, int refresh) Q_DECL_OVERRIDE;
    void output_geometry(int32_t x, int32_t y,
                         int32_t width, int32_t height,
                         int subpixel,
                         const QString &make,
                         const QString &model,
                         int32_t transform) Q_DECL_OVERRIDE;
    void output_scale(int32_t factor) Q_DECL_OVERRIDE;
    void output_done() Q_DECL_OVERRIDE;

    int m_outputId;
    QWaylandDisplay *mWaylandDisplay;
    QRect mGeometry;
    int mScale;
    int mDepth;
    int mRefreshRate;
    int mTransform;
    QImage::Format mFormat;
    QSize mPhysicalSize;
    QString mOutputName;
    Qt::ScreenOrientation m_orientation;

    QWaylandCursor *mWaylandCursor;
};

}

QT_END_NAMESPACE

#endif // QWAYLANDSCREEN_H
