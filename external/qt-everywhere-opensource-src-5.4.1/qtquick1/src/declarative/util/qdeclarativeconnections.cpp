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

#include "private/qdeclarativeconnections_p.h"

#include <qdeclarativeexpression.h>
#include <private/qdeclarativeproperty_p.h>
#include <private/qdeclarativeboundsignal_p.h>
#include <qdeclarativecontext.h>
#include <private/qdeclarativecontext_p.h>
#include <qdeclarativeinfo.h>

#include <QtCore/qdebug.h>
#include <QtCore/qstringlist.h>

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

class QDeclarativeConnectionsPrivate : public QObjectPrivate
{
public:
    QDeclarativeConnectionsPrivate() : target(0), targetSet(false), ignoreUnknownSignals(false), componentcomplete(true) {}

    QList<QDeclarativeBoundSignal*> boundsignals;
    QObject *target;

    bool targetSet;
    bool ignoreUnknownSignals;
    bool componentcomplete;

    QByteArray data;
};

/*!
    \qmltype Connections
    \instantiates QDeclarativeConnections
    \ingroup qml-utility-elements
    \since 4.7
    \brief A Connections element describes generalized connections to signals.

    A Connections object creates a connection to a QML signal.

    When connecting to signals in QML, the usual way is to create an
    "on<Signal>" handler that reacts when a signal is received, like this:

    \qml
    MouseArea {
        onClicked: { foo(parameters) }
    }
    \endqml

    However, it is not possible to connect to a signal in this way in some
    cases, such as when:

    \list
        \li Multiple connections to the same signal are required
        \li Creating connections outside the scope of the signal sender
        \li Connecting to targets not defined in QML
    \endlist

    When any of these are needed, the Connections element can be used instead.

    For example, the above code can be changed to use a Connections object,
    like this:

    \qml
    MouseArea {
        Connections {
            onClicked: foo(parameters)
        }
    }
    \endqml

    More generally, the Connections object can be a child of some object other than
    the sender of the signal:

    \qml
    MouseArea {
        id: area
    }
    // ...
    \endqml
    \qml
    Connections {
        target: area
        onClicked: foo(parameters)
    }
    \endqml

    \sa QtDeclarative
*/
QDeclarativeConnections::QDeclarativeConnections(QObject *parent) :
    QObject(*(new QDeclarativeConnectionsPrivate), parent)
{
}

QDeclarativeConnections::~QDeclarativeConnections()
{
}

/*!
    \qmlproperty Object Connections::target
    This property holds the object that sends the signal.

    If this property is not set, the \c target defaults to the parent of the Connection.

    If set to null, no connection is made and any signal handlers are ignored
    until the target is not null.
*/
QObject *QDeclarativeConnections::target() const
{
    Q_D(const QDeclarativeConnections);
    return d->targetSet ? d->target : parent();
}

void QDeclarativeConnections::setTarget(QObject *obj)
{
    Q_D(QDeclarativeConnections);
    d->targetSet = true; // even if setting to 0, it is *set*
    if (d->target == obj)
        return;
    foreach (QDeclarativeBoundSignal *s, d->boundsignals) {
        // It is possible that target is being changed due to one of our signal
        // handlers -> use deleteLater().
        if (s->isEvaluating())
            s->deleteLater();
        else
            delete s;
    }
    d->boundsignals.clear();
    d->target = obj;
    connectSignals();
    emit targetChanged();
}

/*!
    \qmlproperty bool Connections::ignoreUnknownSignals

    Normally, a connection to a non-existent signal produces runtime errors.

    If this property is set to \c true, such errors are ignored.
    This is useful if you intend to connect to different types of objects, handling
    a different set of signals for each object.
*/
bool QDeclarativeConnections::ignoreUnknownSignals() const
{
    Q_D(const QDeclarativeConnections);
    return d->ignoreUnknownSignals;
}

void QDeclarativeConnections::setIgnoreUnknownSignals(bool ignore)
{
    Q_D(QDeclarativeConnections);
    d->ignoreUnknownSignals = ignore;
}



QByteArray
QDeclarativeConnectionsParser::compile(const QList<QDeclarativeCustomParserProperty> &props)
{
    QByteArray rv;
    QDataStream ds(&rv, QIODevice::WriteOnly);

    for(int ii = 0; ii < props.count(); ++ii)
    {
        QString propName = QString::fromUtf8(props.at(ii).name());
        if (!propName.startsWith(QLatin1String("on")) || !propName.at(2).isUpper()) {
            error(props.at(ii), QDeclarativeConnections::tr("Cannot assign to non-existent property \"%1\"").arg(propName));
            return QByteArray();
        }

        QList<QVariant> values = props.at(ii).assignedValues();

        for (int i = 0; i < values.count(); ++i) {
            const QVariant &value = values.at(i);

            if (value.userType() == qMetaTypeId<QDeclarativeCustomParserNode>()) {
                error(props.at(ii), QDeclarativeConnections::tr("Connections: nested objects not allowed"));
                return QByteArray();
            } else if (value.userType() == qMetaTypeId<QDeclarativeCustomParserProperty>()) {
                error(props.at(ii), QDeclarativeConnections::tr("Connections: syntax error"));
                return QByteArray();
            } else {
                QDeclarativeParser::Variant v = qvariant_cast<QDeclarativeParser::Variant>(value);
                if (v.isScript()) {
                    ds << propName;
                    ds << v.asScript();
                } else {
                    error(props.at(ii), QDeclarativeConnections::tr("Connections: script expected"));
                    return QByteArray();
                }
            }
        }
    }

    return rv;
}

void QDeclarativeConnectionsParser::setCustomData(QObject *object,
                                            const QByteArray &data)
{
    QDeclarativeConnectionsPrivate *p =
        static_cast<QDeclarativeConnectionsPrivate *>(QObjectPrivate::get(object));
    p->data = data;
}


void QDeclarativeConnections::connectSignals()
{
    Q_D(QDeclarativeConnections);
    if (!d->componentcomplete || (d->targetSet && !target()))
        return;

    QDataStream ds(d->data);
    while (!ds.atEnd()) {
        QString propName;
        ds >> propName;
        QString script;
        ds >> script;
        QDeclarativeProperty prop(target(), propName);
        if (prop.isValid() && (prop.type() & QDeclarativeProperty::SignalProperty)) {
            QDeclarativeBoundSignal *signal =
                new QDeclarativeBoundSignal(target(), prop.method(), this);
            QDeclarativeExpression *expression = new QDeclarativeExpression(qmlContext(this), 0, script);
            QDeclarativeData *ddata = QDeclarativeData::get(this);
            if (ddata && ddata->outerContext && !ddata->outerContext->url.isEmpty())
                expression->setSourceLocation(ddata->outerContext->url.toString(), ddata->lineNumber);
            signal->setExpression(expression);
            d->boundsignals += signal;
        } else {
            if (!d->ignoreUnknownSignals)
                qmlInfo(this) << tr("Cannot assign to non-existent property \"%1\"").arg(propName);
        }
    }
}

void QDeclarativeConnections::classBegin()
{
    Q_D(QDeclarativeConnections);
    d->componentcomplete=false;
}

void QDeclarativeConnections::componentComplete()
{
    Q_D(QDeclarativeConnections);
    d->componentcomplete=true;
    connectSignals();
}

QT_END_NAMESPACE
