/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include "qqmlconnections_p.h"

#include <private/qqmlexpression_p.h>
#include <private/qqmlproperty_p.h>
#include <private/qqmlboundsignal_p.h>
#include <qqmlcontext.h>
#include <private/qqmlcontext_p.h>
#include <private/qqmlcompiler_p.h>
#include <qqmlinfo.h>

#include <QtCore/qdebug.h>
#include <QtCore/qstringlist.h>

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

class QQmlConnectionsPrivate : public QObjectPrivate
{
public:
    QQmlConnectionsPrivate() : target(0), targetSet(false), ignoreUnknownSignals(false), componentcomplete(true) {}

    QList<QQmlBoundSignal*> boundsignals;
    QObject *target;

    bool targetSet;
    bool ignoreUnknownSignals;
    bool componentcomplete;

    QQmlRefPointer<QQmlCompiledData> cdata;
    QList<const QV4::CompiledData::Binding *> bindings;
};

/*!
    \qmltype Connections
    \instantiates QQmlConnections
    \inqmlmodule QtQml
    \ingroup qtquick-interceptors
    \brief Describes generalized connections to signals

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

    When any of these are needed, the Connections type can be used instead.

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

    \sa {Qt QML}
*/
QQmlConnections::QQmlConnections(QObject *parent) :
    QObject(*(new QQmlConnectionsPrivate), parent)
{
}

QQmlConnections::~QQmlConnections()
{
}

/*!
    \qmlproperty Object QtQml::Connections::target
    This property holds the object that sends the signal.

    If this property is not set, the \c target defaults to the parent of the Connection.

    If set to null, no connection is made and any signal handlers are ignored
    until the target is not null.
*/
QObject *QQmlConnections::target() const
{
    Q_D(const QQmlConnections);
    return d->targetSet ? d->target : parent();
}

class QQmlBoundSignalDeleter : public QObject
{
public:
    QQmlBoundSignalDeleter(QQmlBoundSignal *signal) : m_signal(signal) { m_signal->removeFromObject(); }
    ~QQmlBoundSignalDeleter() { delete m_signal; }

private:
    QQmlBoundSignal *m_signal;
};

void QQmlConnections::setTarget(QObject *obj)
{
    Q_D(QQmlConnections);
    d->targetSet = true; // even if setting to 0, it is *set*
    if (d->target == obj)
        return;
    foreach (QQmlBoundSignal *s, d->boundsignals) {
        // It is possible that target is being changed due to one of our signal
        // handlers -> use deleteLater().
        if (s->isEvaluating())
            (new QQmlBoundSignalDeleter(s))->deleteLater();
        else
            delete s;
    }
    d->boundsignals.clear();
    d->target = obj;
    connectSignals();
    emit targetChanged();
}

/*!
    \qmlproperty bool QtQml::Connections::ignoreUnknownSignals

    Normally, a connection to a non-existent signal produces runtime errors.

    If this property is set to \c true, such errors are ignored.
    This is useful if you intend to connect to different types of objects, handling
    a different set of signals for each object.
*/
bool QQmlConnections::ignoreUnknownSignals() const
{
    Q_D(const QQmlConnections);
    return d->ignoreUnknownSignals;
}

void QQmlConnections::setIgnoreUnknownSignals(bool ignore)
{
    Q_D(QQmlConnections);
    d->ignoreUnknownSignals = ignore;
}

void QQmlConnectionsParser::verifyBindings(const QV4::CompiledData::Unit *qmlUnit, const QList<const QV4::CompiledData::Binding *> &props)
{
    for (int ii = 0; ii < props.count(); ++ii) {
        const QV4::CompiledData::Binding *binding = props.at(ii);
        QString propName = qmlUnit->stringAt(binding->propertyNameIndex);

        if (!propName.startsWith(QLatin1String("on")) || !propName.at(2).isUpper()) {
            error(props.at(ii), QQmlConnections::tr("Cannot assign to non-existent property \"%1\"").arg(propName));
            return;
        }


        if (binding->type >= QV4::CompiledData::Binding::Type_Object) {
            const QV4::CompiledData::Object *target = qmlUnit->objectAt(binding->value.objectIndex);
            if (!qmlUnit->stringAt(target->inheritedTypeNameIndex).isEmpty())
                error(binding, QQmlConnections::tr("Connections: nested objects not allowed"));
            else
                error(binding, QQmlConnections::tr("Connections: syntax error"));
            return;
        } if (binding->type != QV4::CompiledData::Binding::Type_Script) {
            error(binding, QQmlConnections::tr("Connections: script expected"));
            return;
        }
    }
}

void QQmlConnectionsParser::applyBindings(QObject *object, QQmlCompiledData *cdata, const QList<const QV4::CompiledData::Binding *> &bindings)
{
    QQmlConnectionsPrivate *p =
        static_cast<QQmlConnectionsPrivate *>(QObjectPrivate::get(object));
    p->cdata = cdata;
    p->bindings = bindings;
}

void QQmlConnections::connectSignals()
{
    Q_D(QQmlConnections);
    if (!d->componentcomplete || (d->targetSet && !target()))
        return;

    if (d->bindings.isEmpty())
        return;
    QObject *target = this->target();
    QQmlData *ddata = QQmlData::get(this);
    QQmlContextData *ctxtdata = ddata ? ddata->outerContext : 0;

    const QV4::CompiledData::Unit *qmlUnit = d->cdata->compilationUnit->data;
    foreach (const QV4::CompiledData::Binding *binding, d->bindings) {
        Q_ASSERT(binding->type == QV4::CompiledData::Binding::Type_Script);
        QString propName = qmlUnit->stringAt(binding->propertyNameIndex);

        QQmlProperty prop(target, propName);
        if (prop.isValid() && (prop.type() & QQmlProperty::SignalProperty)) {
            int signalIndex = QQmlPropertyPrivate::get(prop)->signalIndex();
            QQmlBoundSignal *signal =
                new QQmlBoundSignal(target, signalIndex, this, qmlEngine(this));

            QQmlBoundSignalExpression *expression = ctxtdata ?
                new QQmlBoundSignalExpression(target, signalIndex,
                                              ctxtdata, this, d->cdata->compilationUnit->runtimeFunctions[binding->value.compiledScriptIndex]) : 0;
            signal->takeExpression(expression);
            d->boundsignals += signal;
        } else {
            if (!d->ignoreUnknownSignals)
                qmlInfo(this) << tr("Cannot assign to non-existent property \"%1\"").arg(propName);
        }
    }
}

void QQmlConnections::classBegin()
{
    Q_D(QQmlConnections);
    d->componentcomplete=false;
}

void QQmlConnections::componentComplete()
{
    Q_D(QQmlConnections);
    d->componentcomplete=true;
    connectSignals();
}

QT_END_NAMESPACE
