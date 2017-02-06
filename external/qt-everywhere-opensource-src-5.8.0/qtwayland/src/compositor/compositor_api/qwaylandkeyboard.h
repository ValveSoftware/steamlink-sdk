/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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

#ifndef QWAYLANDKEYBOARD_H
#define QWAYLANDKEYBOARD_H

#include <QtCore/QObject>

#include <QtWaylandCompositor/QWaylandCompositorExtension>
#include <QtWaylandCompositor/QWaylandSurface>

QT_BEGIN_NAMESPACE

class QWaylandKeyboard;
class QWaylandKeyboardPrivate;
class QWaylandSeat;
class QWaylandKeymap;

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandKeyboard : public QWaylandObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QWaylandKeyboard)
    Q_PROPERTY(quint32 repeatRate READ repeatRate WRITE setRepeatRate NOTIFY repeatRateChanged)
    Q_PROPERTY(quint32 repeatDelay READ repeatDelay WRITE setRepeatDelay NOTIFY repeatDelayChanged)
public:
    QWaylandKeyboard(QWaylandSeat *seat, QObject *parent = nullptr);

    QWaylandSeat *seat() const;
    QWaylandCompositor *compositor() const;

    quint32 repeatRate() const;
    void setRepeatRate(quint32 rate);

    quint32 repeatDelay() const;
    void setRepeatDelay(quint32 delay);

    virtual void setFocus(QWaylandSurface *surface);

    virtual void sendKeyModifiers(QWaylandClient *client, uint32_t serial);
    virtual void sendKeyPressEvent(uint code);
    virtual void sendKeyReleaseEvent(uint code);

    QWaylandSurface *focus() const;
    QWaylandClient *focusClient() const;

    virtual void addClient(QWaylandClient *client, uint32_t id, uint32_t version);

Q_SIGNALS:
    void focusChanged(QWaylandSurface *surface);
    void repeatRateChanged(quint32 repeatRate);
    void repeatDelayChanged(quint32 repeatDelay);

private:
    void focusDestroyed(void *data);

private Q_SLOTS:
    void updateKeymap();
};

QT_END_NAMESPACE

#endif //QWAYLANDKEYBOARD_H
