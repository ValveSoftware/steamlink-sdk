/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QDECLARATIVEPINCHGENERATOR_H
#define QDECLARATIVEPINCHGENERATOR_H

#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickWindow>
#include <QtGui/QMouseEvent>
#include <QtGui/QTouchEvent>
#include <QtGui/QKeyEvent>
#include <QtCore/QElapsedTimer>
#include <QtCore/QList>
#include <QtCore/QPoint>
#include <QDebug>

#define SWIPES_REQUIRED 2

QT_BEGIN_NAMESPACE

typedef struct {
    QList<QPoint> touchPoints;
    QList<int> durations;
} Swipe;

class QDeclarativePinchGenerator : public QQuickItem
{
    Q_OBJECT

    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(qreal replaySpeedFactor READ replaySpeedFactor WRITE setReplaySpeedFactor NOTIFY replaySpeedFactorChanged)
    Q_PROPERTY(QQuickItem* target READ target WRITE setTarget NOTIFY targetChanged)
    Q_INTERFACES(QQmlParserStatus)

public:
    QDeclarativePinchGenerator();
    ~QDeclarativePinchGenerator();

    QString state() const;
    QQuickItem* target() const;
    void setTarget(QQuickItem* target);
    qreal replaySpeedFactor() const;
    void setReplaySpeedFactor(qreal factor);
    bool enabled() const;
    void setEnabled(bool enabled);

    Q_INVOKABLE void replay();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void stop();
    Q_INVOKABLE int startDragDistance();

    // programmatic interface, useful for autotests
    Q_INVOKABLE void pinch(QPoint point1From,
                           QPoint point1To,
                           QPoint point2From,
                           QPoint point2To,
                           int interval1 = 20,
                           int interval2 = 20,
                           int samples1 = 10,
                           int samples2 = 10);

    Q_INVOKABLE void pinchPress(QPoint point1From, QPoint point2From);
    Q_INVOKABLE void pinchMoveTo(QPoint point1To, QPoint point2To);
    Q_INVOKABLE void pinchRelease(QPoint point1To, QPoint point2To);

signals:
    void stateChanged();
    void targetChanged();
    void replaySpeedFactorChanged();
    void enabledChanged();

public:
    enum GeneratorState {
        Invalid    = 0,
        Idle       = 1,
        Recording  = 2,
        Replaying  = 3
    };

    // from QQmlParserStatus
    virtual void componentComplete();
    // from QQuickItem
    void itemChange(ItemChange change, const ItemChangeData & data);

protected:
    void mousePressEvent(QMouseEvent *event);
    void mouseReleaseEvent(QMouseEvent *event);
    void mouseDoubleClickEvent(QMouseEvent *event);
    void mouseMoveEvent(QMouseEvent *event);
    void keyPressEvent(QKeyEvent *event);
    void timerEvent(QTimerEvent *event);

private:
    void setState(GeneratorState state);

private:
    QQuickItem* target_;
    GeneratorState state_;
    QQuickWindow* window_;
    QList<Swipe*> swipes_;
    Swipe* activeSwipe_;
    QElapsedTimer swipeTimer_;
    int replayTimer_;
    int replayBookmark_;
    int masterSwipe_;
    qreal replaySpeedFactor_;
    bool enabled_;
    QTouchDevice* device_;
};

QT_END_NAMESPACE

#endif
