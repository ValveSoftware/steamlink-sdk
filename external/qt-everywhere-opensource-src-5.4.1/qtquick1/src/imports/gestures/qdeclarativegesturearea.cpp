/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qdeclarativegesturearea_p.h"

#include <qdeclarativeexpression.h>
#include <qdeclarativecontext.h>
#include <qdeclarativeinfo.h>

#include <private/qdeclarativeproperty_p.h>
#include <private/qdeclarativeitem_p.h>

#include <QtCore/qdebug.h>
#include <QtCore/qstringlist.h>

#include <QtGui/qevent.h>

#include <private/qobject_p.h>

#ifndef QT_NO_GESTURES

QT_BEGIN_NAMESPACE

class QDeclarativeGestureAreaPrivate : public QDeclarativeItemPrivate
{
    Q_DECLARE_PUBLIC(QDeclarativeGestureArea)
public:
    QDeclarativeGestureAreaPrivate() : componentcomplete(false), gesture(0) {}

    typedef QMap<Qt::GestureType,QDeclarativeExpression*> Bindings;
    Bindings bindings;

    bool componentcomplete;

    QByteArray data;

    QGesture *gesture;

    bool gestureEvent(QGestureEvent *event);
};

/*!
    \qmltype GestureArea
    \instantiates QDeclarativeGestureArea
    \ingroup qml-basic-interaction-elements

    \brief The GestureArea item enables simple gesture handling.
    \inherits Item

    A GestureArea is like a MouseArea, but it has signals for gesture events.

    \warning Elements in the Qt.labs module are not guaranteed to remain compatible
    in future versions.

    \warning GestureArea is an experimental element whose development has
    been discontinued.  PinchArea is available in QtQuick 1.1 and handles
    two finger gesture input.

    \note This element is only functional on devices with touch input.

    \qml
    import Qt.labs.gestures 1.0

    GestureArea {
        anchors.fill: parent
     // onPan:        ... gesture.acceleration ...
     // onPinch:      ... gesture.rotationAngle ...
     // onSwipe:      ...
     // onTapAndHold: ...
     // onTap:        ...
     // onGesture:    ...
    }
    \endqml

    Each signal has a \e gesture parameter that has the
    properties of the gesture.

    \table
    \header \li Signal \li Type \li Property \li Description
    \row \li onTap \li point \li position \li the position of the tap
    \row \li onTapAndHold \li point \li position \li the position of the tap
    \row \li onPan \li real \li acceleration \li the acceleration of the pan
    \row \li onPan \li point \li delta \li the offset from the previous input position to the current input
    \row \li onPan \li point \li offset \li the total offset from the first input position to the current input position
    \row \li onPan \li point \li lastOffset \li the previous value of offset
    \row \li onPinch \li point \li centerPoint \li the midpoint between the two input points
    \row \li onPinch \li point \li lastCenterPoint \li the previous value of centerPoint
    \row \li onPinch \li point \li startCenterPoint \li the first value of centerPoint
    \row \li onPinch \li real \li rotationAngle \li the angle covered by the gesture motion
    \row \li onPinch \li real \li lastRotationAngle \li the previous value of rotationAngle
    \row \li onPinch \li real \li totalRotationAngle \li the complete angle covered by the gesture
    \row \li onPinch \li real \li scaleFactor \li the change in distance between the two input points
    \row \li onPinch \li real \li lastScaleFactor \li the previous value of scaleFactor
    \row \li onPinch \li real \li totalScaleFactor \li the complete scale factor of the gesture
    \row \li onSwipe \li real \li swipeAngle \li the angle of the swipe
    \endtable

    Custom gestures, handled by onGesture, will have custom properties.

    GestureArea is an invisible item: it is never painted.

    \sa MouseArea
*/

/*!
    \internal
    \class QDeclarativeGestureArea
    \brief The QDeclarativeGestureArea class provides simple gesture handling.

*/
QDeclarativeGestureArea::QDeclarativeGestureArea(QDeclarativeItem *parent) :
    QDeclarativeItem(*(new QDeclarativeGestureAreaPrivate), parent)
{
    setAcceptedMouseButtons(Qt::LeftButton);
    setAcceptTouchEvents(true);
}

QDeclarativeGestureArea::~QDeclarativeGestureArea()
{
}

QByteArray
QDeclarativeGestureAreaParser::compile(const QList<QDeclarativeCustomParserProperty> &props)
{
    QByteArray rv;
    QDataStream ds(&rv, QIODevice::WriteOnly);

    for(int ii = 0; ii < props.count(); ++ii)
    {
        QString propName = QString::fromUtf8(props.at(ii).name());
        Qt::GestureType type;

        if (propName == QLatin1String("onTap")) {
            type = Qt::TapGesture;
        } else if (propName == QLatin1String("onTapAndHold")) {
            type = Qt::TapAndHoldGesture;
        } else if (propName == QLatin1String("onPan")) {
            type = Qt::PanGesture;
        } else if (propName == QLatin1String("onPinch")) {
            type = Qt::PinchGesture;
        } else if (propName == QLatin1String("onSwipe")) {
            type = Qt::SwipeGesture;
        } else if (propName == QLatin1String("onGesture")) {
            type = Qt::CustomGesture;
        } else {
            error(props.at(ii), QDeclarativeGestureArea::tr("Cannot assign to non-existent property \"%1\"").arg(propName));
            return QByteArray();
        }

        QList<QVariant> values = props.at(ii).assignedValues();

        for (int i = 0; i < values.count(); ++i) {
            const QVariant &value = values.at(i);

            if (value.userType() == qMetaTypeId<QDeclarativeCustomParserNode>()) {
                error(props.at(ii), QDeclarativeGestureArea::tr("GestureArea: nested objects not allowed"));
                return QByteArray();
            } else if (value.userType() == qMetaTypeId<QDeclarativeCustomParserProperty>()) {
                error(props.at(ii), QDeclarativeGestureArea::tr("GestureArea: syntax error"));
                return QByteArray();
            } else {
                QDeclarativeParser::Variant v = qvariant_cast<QDeclarativeParser::Variant>(value);
                if (v.isScript()) {
                    ds << propName;
                    ds << int(type);
                    ds << v.asScript();
                } else {
                    error(props.at(ii), QDeclarativeGestureArea::tr("GestureArea: script expected"));
                    return QByteArray();
                }
            }
        }
    }

    return rv;
}

void QDeclarativeGestureAreaParser::setCustomData(QObject *object,
                                            const QByteArray &data)
{
    QDeclarativeGestureArea *ga = static_cast<QDeclarativeGestureArea*>(object);
    ga->d_func()->data = data;
}


void QDeclarativeGestureArea::connectSignals()
{
    Q_D(QDeclarativeGestureArea);
    if (!d->componentcomplete)
        return;

    QDataStream ds(d->data);
    while (!ds.atEnd()) {
        QString propName;
        ds >> propName;
        int gesturetype;
        ds >> gesturetype;
        QString script;
        ds >> script;
        QDeclarativeExpression *exp = new QDeclarativeExpression(qmlContext(this), this, script);
        d->bindings.insert(Qt::GestureType(gesturetype),exp);
        grabGesture(Qt::GestureType(gesturetype));
    }
}

void QDeclarativeGestureArea::componentComplete()
{
    QDeclarativeItem::componentComplete();
    Q_D(QDeclarativeGestureArea);
    d->componentcomplete=true;
    connectSignals();
}

QGesture *QDeclarativeGestureArea::gesture() const
{
    Q_D(const QDeclarativeGestureArea);
    return d->gesture;
}

bool QDeclarativeGestureArea::sceneEvent(QEvent *event)
{
    Q_D(QDeclarativeGestureArea);
    if (event->type() == QEvent::Gesture)
        return d->gestureEvent(static_cast<QGestureEvent*>(event));
    return QDeclarativeItem::sceneEvent(event);
}

bool QDeclarativeGestureAreaPrivate::gestureEvent(QGestureEvent *event)
{
    bool accept = true;
    for (Bindings::Iterator it = bindings.begin(); it != bindings.end(); ++it) {
        if ((gesture = event->gesture(it.key()))) {
            QDeclarativeExpression *expr = it.value();
            expr->evaluate();
            if (expr->hasError())
                qmlInfo(q_func()) << expr->error();
            event->setAccepted(true); // XXX only if value returns true?
        }
    }
    return accept;
}

QT_END_NAMESPACE

#endif // QT_NO_GESTURES
