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

#ifndef QWAYLANDSCREEN_H
#define QWAYLANDSCREEN_H

#include <qpa/qplatformscreen.h>
#include <QtWaylandClient/private/qwaylandclientexport_p.h>

#include <QtWaylandClient/private/qwayland-wayland.h>

QT_BEGIN_NAMESPACE

class QWaylandDisplay;
class QWaylandCursor;
class QWaylandExtendedOutput;

class Q_WAYLAND_CLIENT_EXPORT QWaylandScreen : public QPlatformScreen, QtWayland::wl_output
{
public:
    QWaylandScreen(QWaylandDisplay *waylandDisplay, int version, uint32_t id);
    ~QWaylandScreen();

    QWaylandDisplay *display() const;

    QRect geometry() const;
    int depth() const;
    QImage::Format format() const;

    QSizeF physicalSize() const Q_DECL_OVERRIDE;

    QDpi logicalDpi() const Q_DECL_OVERRIDE;

    void setOrientationUpdateMask(Qt::ScreenOrientations mask);

    Qt::ScreenOrientation orientation() const;
    qreal refreshRate() const;

    QString name() const { return mOutputName; }

    QPlatformCursor *cursor() const;
    QWaylandCursor *waylandCursor() const { return mWaylandCursor; };

    uint32_t outputId() const { return m_outputId; }
    ::wl_output *output() { return object(); }

    QWaylandExtendedOutput *extendedOutput() const;
    void createExtendedOutput();

    static QWaylandScreen *waylandScreenFromWindow(QWindow *window);

private:
    void output_mode(uint32_t flags, int width, int height, int refresh) Q_DECL_OVERRIDE;
    void output_geometry(int32_t x, int32_t y,
                         int32_t width, int32_t height,
                         int subpixel,
                         const QString &make,
                         const QString &model,
                         int32_t transform) Q_DECL_OVERRIDE;
    void output_done() Q_DECL_OVERRIDE;

    int m_outputId;
    QWaylandDisplay *mWaylandDisplay;
    QWaylandExtendedOutput *mExtendedOutput;
    QRect mGeometry;
    int mDepth;
    int mRefreshRate;
    int mTransform;
    QImage::Format mFormat;
    QSize mPhysicalSize;
    QString mOutputName;
    Qt::ScreenOrientation m_orientation;

    QWaylandCursor *mWaylandCursor;
};

QT_END_NAMESPACE

#endif // QWAYLANDSCREEN_H
