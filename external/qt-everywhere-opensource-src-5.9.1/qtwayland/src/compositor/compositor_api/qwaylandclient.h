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

#ifndef QWAYLANDCLIENT_H
#define QWAYLANDCLIENT_H

#include <QtWaylandCompositor/qtwaylandcompositorglobal.h>

#include <QObject>

#include <signal.h>

struct wl_client;

QT_BEGIN_NAMESPACE

class QWaylandClientPrivate;
class QWaylandCompositor;

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandClient : public QObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QWaylandClient)

    Q_PROPERTY(QWaylandCompositor *compositor READ compositor CONSTANT)
    Q_PROPERTY(qint64 userId READ userId CONSTANT)
    Q_PROPERTY(qint64 groupId READ groupId CONSTANT)
    Q_PROPERTY(qint64 processId READ processId CONSTANT)
public:
    ~QWaylandClient();

    static QWaylandClient *fromWlClient(QWaylandCompositor *compositor, wl_client *wlClient);

    QWaylandCompositor *compositor() const;

    wl_client *client() const;

    qint64 userId() const;
    qint64 groupId() const;

    qint64 processId() const;

    Q_INVOKABLE void kill(int signal = SIGTERM);

public Q_SLOTS:
    void close();

private:
    explicit QWaylandClient(QWaylandCompositor *compositor, wl_client *client);
};

QT_END_NAMESPACE

#endif // QWAYLANDCLIENT_H
