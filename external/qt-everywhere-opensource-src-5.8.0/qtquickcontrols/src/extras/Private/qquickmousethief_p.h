/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Extras module of the Qt Toolkit.
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

#ifndef MOUSETHIEF_H
#define MOUSETHIEF_H

#include <QObject>
#include <QPointer>
#include <QtQuick/QQuickItem>

class QQuickMouseThief : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool receivedPressEvent READ receivedPressEvent
        WRITE setReceivedPressEvent NOTIFY receivedPressEventChanged)
public:
    explicit QQuickMouseThief(QObject *parent = 0);

    bool receivedPressEvent() const;
    void setReceivedPressEvent(bool receivedPressEvent);

    Q_INVOKABLE void grabMouse(QQuickItem *item);
    Q_INVOKABLE void ungrabMouse();
    Q_INVOKABLE void acceptCurrentEvent();
signals:
    /*!
        Coordinates are relative to the window of mItem.
    */
    void pressed(int mouseX, int mouseY);

    void released(int mouseX, int mouseY);

    void clicked(int mouseX, int mouseY);

    void touchUpdate(int mouseX, int mouseY);

    void receivedPressEventChanged();

    void handledEventChanged();
protected:
    bool eventFilter(QObject *obj, QEvent *event);
private slots:
    void itemWindowChanged(QQuickWindow *window);
private:
    void emitPressed(const QPointF &pos);
    void emitReleased(const QPointF &pos);
    void emitClicked(const QPointF &pos);

    QPointer<QQuickItem> mItem;
    bool mReceivedPressEvent;
    bool mAcceptCurrentEvent;
};

#endif // MOUSETHIEF_H
