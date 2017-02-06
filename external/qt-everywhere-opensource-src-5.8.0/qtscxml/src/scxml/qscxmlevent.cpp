/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtScxml module of the Qt Toolkit.
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

#include "qscxmlexecutablecontent_p.h"
#include "qscxmlevent_p.h"
#include "qscxmlstatemachine_p.h"

#include <QJsonDocument>
#include <QJsonObject>

QT_BEGIN_NAMESPACE

using namespace QScxmlExecutableContent;

QAtomicInt QScxmlEventBuilder::idCounter = QAtomicInt(0);

QScxmlEvent *QScxmlEventBuilder::buildEvent()
{
    auto dataModel = stateMachine ? stateMachine->dataModel() : Q_NULLPTR;
    auto tableData = stateMachine ? stateMachine->tableData() : Q_NULLPTR;

    QString eventName = event;
    bool ok = true;
    if (eventexpr != NoEvaluator) {
        eventName = dataModel->evaluateToString(eventexpr, &ok);
        ok = true; // ignore failure.
    }

    QVariant data;
    if ((!params || params->count == 0) && (!namelist || namelist->count == 0)) {
        if (contentExpr == NoEvaluator) {
            data = contents;
        } else {
            data = dataModel->evaluateToString(contentExpr, &ok);
        }
        if (!ok) {
            // expr evaluation failure results in the data property of the event being set to null. See e.g. test528.
            data = QVariant(QMetaType::VoidStar, 0);
        }
    } else {
        QVariantMap keyValues;
        if (evaluate(params, stateMachine, keyValues)) {
            if (namelist) {
                for (qint32 i = 0; i < namelist->count; ++i) {
                    QString name = tableData->string(namelist->const_data()[i]);
                    keyValues.insert(name, dataModel->scxmlProperty(name));
                }
            }
            data = keyValues;
        } else {
            // If the evaluation of the <param> tags fails, set _event.data to an empty string.
            // See test343.
            data = QVariant(QMetaType::VoidStar, 0);
        }
    }

    QString sendid = id;
    if (!idLocation.isEmpty()) {
        sendid = generateId();
        ok = stateMachine->dataModel()->setScxmlProperty(idLocation, sendid, tableData->string(instructionLocation));
        if (!ok)
            return Q_NULLPTR;
    }

    QString origin = target;
    if (targetexpr != NoEvaluator) {
        origin = dataModel->evaluateToString(targetexpr, &ok);
        if (!ok)
            return Q_NULLPTR;
    }
    if (origin.isEmpty()) {
        if (eventType == QScxmlEvent::ExternalEvent) {
            origin = QStringLiteral("#_internal");
        }
    } else if (origin == QStringLiteral("#_parent")) {
        // allow sending messages to the parent, independently of whether we're invoked or not.
    } else if (!origin.startsWith(QLatin1Char('#'))) {
        // [6.2.4] and test194.
        submitError(QStringLiteral("error.execution"),
                    QStringLiteral("Error in %1: %2 is not a legal target")
                    .arg(tableData->string(instructionLocation), origin),
                    sendid);
        return Q_NULLPTR;
    } else if (!stateMachine->isDispatchableTarget(origin)) {
        // [6.2.4] and test521.
        submitError(QStringLiteral("error.communication"),
                    QStringLiteral("Error in %1: cannot dispatch to target '%2'")
                    .arg(tableData->string(instructionLocation), origin),
                    sendid);
        return Q_NULLPTR;
    }

    QString origintype = type;
    if (origintype.isEmpty()) {
        // [6.2.5] and test198
        origintype = QStringLiteral("http://www.w3.org/TR/scxml/#SCXMLEventProcessor");
    }
    if (typeexpr != NoEvaluator) {
        origintype = dataModel->evaluateToString(typeexpr, &ok);
        if (!ok)
            return Q_NULLPTR;
    }
    if (!origintype.isEmpty()
            && origintype != QStringLiteral("http://www.w3.org/TR/scxml/#SCXMLEventProcessor")) {
        // [6.2.5] and test199
        submitError(QStringLiteral("error.execution"),
                    QStringLiteral("Error in %1: %2 is not a valid type")
                    .arg(tableData->string(instructionLocation), origintype),
                    sendid);
        return Q_NULLPTR;
    }

    QString invokeid;
    if (stateMachine && stateMachine->isInvoked()) {
        invokeid = stateMachine->sessionId();
    }

    QScxmlEvent *event = new QScxmlEvent;
    event->setName(eventName);
    event->setEventType(eventType);
    event->setData(data);
    event->setSendId(sendid);
    event->setOrigin(origin);
    event->setOriginType(origintype);
    event->setInvokeId(invokeid);
    return event;
}

QScxmlEvent *QScxmlEventBuilder::errorEvent(QScxmlStateMachine *stateMachine, const QString &name,
                                            const QString &message, const QString &sendid)
{
    QScxmlEventBuilder event;
    event.stateMachine = stateMachine;
    event.event = name;
    event.eventType = QScxmlEvent::PlatformEvent; // Errors are platform events. See e.g. test331.
    // _event.data == null, see test528
    event.id = sendid;
    auto error = event();
    error->setErrorMessage(message);
    return error;
}

bool QScxmlEventBuilder::evaluate(const ParameterInfo &param, QScxmlStateMachine *stateMachine,
                                  QVariantMap &keyValues)
{
    auto dataModel = stateMachine->dataModel();
    auto tableData = stateMachine->tableData();
    if (param.expr != NoEvaluator) {
        bool success = false;
        auto v = dataModel->evaluateToVariant(param.expr, &success);
        keyValues.insert(tableData->string(param.name), v);
        return success;
    }

    QString loc;
    if (param.location != QScxmlExecutableContent::NoString) {
        loc = tableData->string(param.location);
    }

    if (loc.isEmpty()) {
        return false;
    }

    if (dataModel->hasScxmlProperty(loc)) {
        keyValues.insert(tableData->string(param.name), dataModel->scxmlProperty(loc));
        return true;
    } else {
        submitError(QStringLiteral("error.execution"),
                    QStringLiteral("Error in <param>: %1 is not a valid location")
                    .arg(loc));
        return false;
    }
}

bool QScxmlEventBuilder::evaluate(const QScxmlExecutableContent::Array<ParameterInfo> *params,
                                  QScxmlStateMachine *stateMachine, QVariantMap &keyValues)
{
    if (!params)
        return true;

    auto paramPtr = params->const_data();
    for (qint32 i = 0; i != params->count; ++i, ++paramPtr) {
        if (!evaluate(*paramPtr, stateMachine, keyValues))
            return false;
    }

    return true;
}

void QScxmlEventBuilder::submitError(const QString &type, const QString &msg, const QString &sendid)
{
    QScxmlStateMachinePrivate::get(stateMachine)->submitError(type, msg, sendid);
}

/*!
 * \class QScxmlEvent
 * \brief The QScxmlEvent class is an event for a Qt SCXML state machine.
 * \since 5.7
 * \inmodule QtScxml
 *
 * SCXML \e events drive transitions. Most events are generated by using the
 * \c <raise> and \c <send> elements in the application. The state machine
 * automatically generates some mandatory events, such as errors.
 *
 * For more information, see
 * \l {SCXML Specification - 5.10.1 The Internal Structure of Events}.
 * For more information about how the Qt SCXML API differs from the
 * specification, see \l {SCXML Compliance}.
 *
 * \sa QScxmlStateMachine
 */

/*!
    \enum QScxmlEvent::EventType

    This enum type specifies the type of an SCXML event:

    \value  PlatformEvent
            An event generated internally by the state machine. For example,
            errors.
    \value  InternalEvent
            An event generated by a \c <raise> element.
    \value  ExternalEvent
            An event generated by a \c <send> element.
 */

/*!
 * Creates a new external SCXML event.
 */
QScxmlEvent::QScxmlEvent()
    : d(new QScxmlEventPrivate)
{ }

/*!
 * Destroys the SCXML event.
 */
QScxmlEvent::~QScxmlEvent()
{
    delete d;
}

/*!
    \property QScxmlEvent::scxmlType
    \brief The event type.

*/

/*!
 * Returns the event type.
 */
QString QScxmlEvent::scxmlType() const
{
    switch (d->eventType) {
    case PlatformEvent:
        return QLatin1String("platform");
    case InternalEvent:
        return QLatin1String("internal");
    case ExternalEvent:
        break;
    }
    return QLatin1String("external");
}

/*!
 * Clears the contents of the event.
 */
void QScxmlEvent::clear()
{
    *d = QScxmlEventPrivate();
}

/*!
 * Assigns \a other to this SCXML event and returns a reference to this SCXML
 * event.
 */
QScxmlEvent &QScxmlEvent::operator=(const QScxmlEvent &other)
{
    *d = *other.d;
    return *this;
}

/*!
 * Constructs a copy of \a other.
 */
QScxmlEvent::QScxmlEvent(const QScxmlEvent &other)
    : d(new QScxmlEventPrivate(*other.d))
{
}

/*!
    \property QScxmlEvent::name

    \brief the name of the event.

    If the event is generated inside the SCXML document, this property holds the
    value of the \e event attribute specified inside the \c <raise> or \c <send>
    element.

    If the event is created in the C++ code and submitted to the
    QScxmlStateMachine, the value of this property is matched against the value
    of the \e event attribute specified inside the \c <transition> element in
    the SCXML document.
*/

/*!
 * Returns the name of the event.
 */
QString QScxmlEvent::name() const
{
    return d->name;
}

/*!
 * Sets the name of the event to \a name.
 */
void QScxmlEvent::setName(const QString &name)
{
    d->name = name;
}

/*!
    \property QScxmlEvent::sendId

    \brief the ID of the event.

    The ID is used by the \c <cancel> element to identify the event to be
    canceled.

    \note The state machine generates a unique ID if the \e id attribute is not
    specified in the \c <send> element. The generated ID can be accessed through
    this property.
*/

/*!
 * Returns the ID of the event.
 */
QString QScxmlEvent::sendId() const
{
    return d->sendid;
}

/*!
 * Sets the ID \a sendid for this event.
 */
void QScxmlEvent::setSendId(const QString &sendid)
{
    d->sendid = sendid;
}

/*!
    \property QScxmlEvent::origin

    \brief the URI that points to the origin of an SCXML event.

    The origin is equivalent to the \e target attribute of the \c <send>
    element.
*/

/*!
 * Returns a URI that points to the origin of an SCXML event.
 */
QString QScxmlEvent::origin() const
{
    return d->origin;
}

/*!
 * Sets the origin of an SCXML event to \a origin.
 *
 * \sa QScxmlEvent::origin
 */
void QScxmlEvent::setOrigin(const QString &origin)
{
    d->origin = origin;
}

/*!
    \property QScxmlEvent::originType

    \brief the origin type of an SCXML event.

    The origin type is equivalent to the \e type attribute of the \c <send>
    element.
*/

/*!
 * Returns the origin type of an SCXML event.
 */
QString QScxmlEvent::originType() const
{
    return d->originType;
}

/*!
 * Sets the origin type of an SCXML event to \a origintype.
 *
 * \sa QScxmlEvent::originType
 */
void QScxmlEvent::setOriginType(const QString &origintype)
{
    d->originType = origintype;
}

/*!
    \property QScxmlEvent::invokeId

    \brief the ID of the invoked state machine if the event is generated by one.
*/

/*!
 * If this event is generated by an invoked state machine, returns the ID of
 * the \c <invoke> element. Otherwise, returns an empty value.
 */
QString QScxmlEvent::invokeId() const
{
    return d->invokeId;
}

/*!
 * Sets the ID of an invoked state machine to \a invokeid.
 * \sa QScxmlEvent::invokeId
 */
void QScxmlEvent::setInvokeId(const QString &invokeid)
{
    d->invokeId = invokeid;
}

/*!
    \property QScxmlEvent::delay

    \brief The delay in milliseconds after which the event is to be delivered
    after processing the \c <send> element.
*/

/*!
 * Returns the delay in milliseconds after which this event is to be delivered
 * after processing the \c <send> element.
 */
int QScxmlEvent::delay() const
{
    return d->delayInMiliSecs;
}

/*!
 * Sets the delay in milliseconds as the value of \a delayInMiliSecs.
 * \sa QScxmlEvent::delay
 */
void QScxmlEvent::setDelay(int delayInMiliSecs)
{
    d->delayInMiliSecs = delayInMiliSecs;
}
/*!
    \property QScxmlEvent::eventType

    \brief the type of the event.
*/

/*!
 * Returns the type of this event.
 * \sa QScxmlEvent::EventType
 */
QScxmlEvent::EventType QScxmlEvent::eventType() const
{
    return d->eventType;
}

/*!
 * Sets the event type to \a type.
 * \sa QScxmlEvent::eventType QScxmlEvent::EventType
 */
void QScxmlEvent::setEventType(const EventType &type)
{
    d->eventType = type;
}

/*!
    \property QScxmlEvent::data

    \brief the data included by the sender.

    When \c <param> elements are used in the \c <send> element, the data will
    contain a QVariantMap where the key is the \e name attribute, and the value
    is taken from the \e expr attribute or the \e location attribute.

    When a \c <content> element is used, the data will contain a single item
    with either the value of the \e expr attribute of the \c <content> element
    or the child data of the \c <content> element.
*/

/*!
 * Returns the data included by the sender.
 */
QVariant QScxmlEvent::data() const
{
    if (isErrorEvent())
        return QVariant();
    return d->data;
}

/*!
 * Sets the payload data to \a data.
 * \sa QScxmlEvent::data
 */
void QScxmlEvent::setData(const QVariant &data)
{
    if (!isErrorEvent())
        d->data = data;
}

/*!
    \property QScxmlEvent::errorEvent
    \brief Whether the event represents an error.
*/

/*!
 * Returns \c true when this is an error event, \c false otherwise.
 */
bool QScxmlEvent::isErrorEvent() const
{
    return eventType() == PlatformEvent && name().startsWith(QStringLiteral("error."));
}

/*!
    \property QScxmlEvent::errorMessage
    \brief An error message for an error event, or an empty QString.
*/

/*!
 * If this is an error event, returns the error message. Otherwise, returns an
 *         empty QString.
 */
QString QScxmlEvent::errorMessage() const
{
    if (!isErrorEvent())
        return QString();
    return d->data.toString();
}

/*!
 * If this is an error event, the \a message is set as the error message.
 */
void QScxmlEvent::setErrorMessage(const QString &message)
{
    if (isErrorEvent())
        d->data = message;
}

QByteArray QScxmlEventPrivate::debugString(QScxmlEvent *event)
{
    if (event == nullptr) {
        return "<null>";
    }

    QJsonObject o;
    if (!event->name().isNull())
        o[QStringLiteral("name")] = event->name();
    if (!event->scxmlType().isNull())
        o[QStringLiteral("type")] = event->scxmlType();
    if (!event->sendId().isNull())
        o[QStringLiteral("sendid")] = event->sendId();
    if (!event->origin().isNull())
        o[QStringLiteral("origin")] = event->origin();
    if (!event->originType().isNull())
        o[QStringLiteral("origintype")] = event->originType();
    if (!event->invokeId().isNull())
        o[QStringLiteral("invokeid")] = event->invokeId();
    if (!event->data().isNull())
        o[QStringLiteral("data")] = QJsonValue::fromVariant(event->data());

    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}

QT_END_NAMESPACE
