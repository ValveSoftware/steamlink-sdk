/****************************************************************************
**
** Copyright (C) 2017 Pier Luigi Fiorini <pierluigi.fiorini@gmail.com>
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWaylandCompositor module of the Qt Toolkit.
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

#ifndef QWAYLANDOUTPUTMODE_H
#define QWAYLANDOUTPUTMODE_H

#include <QtWaylandCompositor/qtwaylandcompositorglobal.h>
#include <QtCore/QSize>

QT_BEGIN_NAMESPACE

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandOutputMode
{
public:
    explicit QWaylandOutputMode();
    QWaylandOutputMode(const QSize &size, int refreshRate);
    QWaylandOutputMode(const QWaylandOutputMode &other);
    ~QWaylandOutputMode();

    QWaylandOutputMode &operator=(const QWaylandOutputMode &other);
    bool operator==(const QWaylandOutputMode &other) const;
    bool operator!=(const QWaylandOutputMode &other) const;

    bool isValid() const;

    QSize size() const;
    int refreshRate() const;

private:
    class QWaylandOutputModePrivate *const d;
    friend class QWaylandOutput;

    void setWidth(int width);
    void setHeight(int height);
};
Q_DECLARE_TYPEINFO(QWaylandOutputMode, Q_MOVABLE_TYPE);

QT_END_NAMESPACE

#endif // QWAYLANDOUTPUTMODE_H
