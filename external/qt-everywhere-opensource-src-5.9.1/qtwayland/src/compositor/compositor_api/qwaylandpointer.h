/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef QWAYLANDPOINTER_H
#define QWAYLANDPOINTER_H

#include <QtWaylandCompositor/QWaylandCompositorExtension>

struct wl_resource;

QT_BEGIN_NAMESPACE

class QWaylandPointer;
class QWaylandPointerPrivate;
class QWaylandSeat;
class QWaylandView;
class QWaylandOutput;
class QWaylandClient;

class Q_WAYLAND_COMPOSITOR_EXPORT QWaylandPointer : public QWaylandObject
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(QWaylandPointer)
    Q_PROPERTY(bool isButtonPressed READ isButtonPressed NOTIFY buttonPressedChanged)
public:
    QWaylandPointer(QWaylandSeat *seat, QObject *parent = nullptr);

    QWaylandSeat *seat() const;
    QWaylandCompositor *compositor() const;

    QWaylandOutput *output() const;
    void setOutput(QWaylandOutput *output);

    virtual uint sendMousePressEvent(Qt::MouseButton button);
    virtual uint sendMouseReleaseEvent(Qt::MouseButton button);
    virtual void sendMouseMoveEvent(QWaylandView *view, const QPointF &localPos, const QPointF &outputSpacePos);
    virtual void sendMouseWheelEvent(Qt::Orientation orientation, int delta);

    QWaylandView *mouseFocus() const;
    QPointF currentLocalPosition() const;
    QPointF currentSpacePosition() const;

    bool isButtonPressed() const;

    virtual void addClient(QWaylandClient *client, uint32_t id, uint32_t version);

    wl_resource *focusResource() const;

    static uint32_t toWaylandButton(Qt::MouseButton button);
    uint sendButton(struct wl_resource *resource, uint32_t time, Qt::MouseButton button, uint32_t state);
Q_SIGNALS:
    void outputChanged();
    void buttonPressedChanged();

private:
    void focusDestroyed(void *data);
    void pointerFocusChanged(QWaylandView *newFocus, QWaylandView *oldFocus);
};

QT_END_NAMESPACE

#endif  /*QWAYLANDPOINTER_H*/
