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

#include "qscxmlstatemachine_p.h"
#include "qscxmlexecutablecontent_p.h"
#include "qscxmlevent_p.h"
#include "qscxmlinvokableservice.h"
#include "qscxmldatamodel_p.h"

#include <QAbstractState>
#include <QAbstractTransition>
#include <QFile>
#include <QHash>
#include <QJSEngine>
#include <QLoggingCategory>
#include <QString>
#include <QTimer>
#include <QThread>

#include <functional>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(qscxmlLog, "qt.scxml.statemachine")
Q_LOGGING_CATEGORY(scxmlLog, "scxml.statemachine")

/*!
 * \class QScxmlStateMachine
 * \brief The QScxmlStateMachine class provides an interface to the state machines
 * created from SCXML files.
 * \since 5.7
 * \inmodule QtScxml
 *
 * QScxmlStateMachine is an implementation of the
 * \l{SCXML Specification}{State Chart XML (SCXML)}.
 *
 * All states that are defined in the SCXML file
 * are accessible as properties of QScxmlStateMachine.
 * These properties are boolean values and indicate
 * whether the state is active or inactive.
 *
 * \note The QScxmlStateMachine needs a QEventLoop to work correctly. The event loop is used to
 *       implement the \c delay attribute for events and to schedule the processing of a state
 *       machine when events are received from nested (or parent) state machines.
 */

/*!
    \fn QScxmlStateMachine::connectToEvent(const QString &scxmlEventSpec,
                                           const QObject *receiver,
                                           PointerToMemberFunction method,
                                           Qt::ConnectionType type)

    Creates a connection of the given \a type from the event specified by
    \a scxmlEventSpec to \a method in the \a receiver object.

    The receiver's \a method must take a QScxmlEvent as a parameter.

    In contrast to event specifications in SCXML documents, spaces are not
    allowed in the \a scxmlEventSpec here. In order to connect to multiple
    events with different prefixes, connectToEvent() has to be called multiple
    times.

    Returns a handle to the connection, which can be used later to disconnect.
*/

/*!
    \fn QScxmlStateMachine::connectToEvent(const QString &scxmlEventSpec,
                                           Functor functor,
                                           Qt::ConnectionType type)

    Creates a connection of the given \a type from the event specified by
    \a scxmlEventSpec to \a functor.

    The \a functor must take a QScxmlEvent as a parameter.

    In contrast to event specifications in SCXML documents, spaces are not
    allowed in the \a scxmlEventSpec here. In order to connect to multiple
    events with different prefixes, connectToEvent() has to be called multiple
    times.

    Returns a handle to the connection, which can be used later to disconnect.
*/

/*!
    \fn QScxmlStateMachine::connectToEvent(const QString &scxmlEventSpec,
                                           const QObject *context,
                                           Functor functor,
                                           Qt::ConnectionType type)

    Creates a connection of the given \a type from the event specified by
    \a scxmlEventSpec to \a functor using \a context as context.

    The \a functor must take a QScxmlEvent as a parameter.

    In contrast to event specifications in SCXML documents, spaces are not
    allowed in the \a scxmlEventSpec here. In order to connect to multiple
    events with different prefixes, connectToEvent() has to be called multiple
    times.

    Returns a handle to the connection, which can be used later to disconnect.
*/

/*!
    \fn QScxmlStateMachine::connectToState(const QString &scxmlStateName,
                                           const QObject *receiver,
                                           PointerToMemberFunction method,
                                           Qt::ConnectionType type)

    Creates a connection of the given \a type from the state specified by
    \a scxmlStateName to \a method in the \a receiver object.

    The receiver's \a method must take a boolean argument that indicates
    whether the state connected became active or inactive.

    Returns a handle to the connection, which can be used later to disconnect.
*/


/*!
    \fn QScxmlStateMachine::connectToState(const QString &scxmlStateName,
                                           Functor functor,
                                           Qt::ConnectionType type)

    Creates a connection of the given \a type from the state specified by
    \a scxmlStateName to \a functor.

    The \a functor must take a boolean argument that indicates whether the
    state connected became active or inactive.

    Returns a handle to the connection, which can be used later to disconnect.
*/

/*!
    \fn QScxmlStateMachine::connectToState(const QString &scxmlStateName,
                                           const QObject *context,
                                           Functor functor,
                                           Qt::ConnectionType type)

    Creates a connection of the given \a type from the state specified by
    \a scxmlStateName to \a functor using \a context as context.

    The \a functor must take a boolean argument that indicates whether the
    state connected became active or inactive.

    Returns a handle to the connection, which can be used later to disconnect.
*/

/*!
    \fn QScxmlStateMachine::onEntry(const QObject *receiver,
                                    const char *method)

    Returns a functor that accepts a boolean argument and calls the given
    \a method on \a receiver using QMetaObject::invokeMethod() if that argument
    is \c true and \a receiver has not been deleted, yet.

    The given \a method must not accept any arguments. \a method is the plain
    method name, not enclosed in \c SIGNAL() or \c SLOT().

    This is useful to wrap handlers for connectToState() that should only
    be executed when the state is entered.
 */

/*!
    \fn QScxmlStateMachine::onExit(const QObject *receiver, const char *method)

    Returns a functor that accepts a boolean argument and calls the given
    \a method on \a receiver using QMetaObject::invokeMethod() if that argument
    is \c false and \a receiver has not been deleted, yet.

    The given \a method must not accept any arguments. \a method is the plain
    method name, not enclosed in SIGNAL(...) or SLOT(...).

    This is useful to wrap handlers for connectToState() that should only
    be executed when the state is left.
 */

/*!
    \fn QScxmlStateMachine::onEntry(Functor functor)

    Returns a functor that accepts a boolean argument and calls the given
    \a functor if that argument is \c true. The given \a functor must not
    accept any arguments.

    This is useful to wrap handlers for connectToState() that should only
    be executed when the state is entered.
 */

/*!
    \fn QScxmlStateMachine::onExit(Functor functor)

    Returns a functor that accepts a boolean argument and calls the given
    \a functor if that argument is \c false. The given \a functor must not
    accept any arguments.

    This is useful to wrap handlers for connectToState() that should only
    be executed when the state is left.
 */

/*!
    \fn QScxmlStateMachine::onEntry(const QObject *receiver,
                                    PointerToMemberFunction method)

    Returns a functor that accepts a boolean argument and calls the given
    \a method on \a receiver if that argument is \c true and the \a receiver
    has not been deleted, yet. The given \a method must not accept any
    arguments.

    This is useful to wrap handlers for connectToState() that should only
    be executed when the state is entered.
 */

/*!
    \fn QScxmlStateMachine::onExit(const QObject *receiver,
                                   PointerToMemberFunction method)

    Returns a functor that accepts a boolean argument and calls the given
    \a method on \a receiver if that argument is \c false and the \a receiver
    has not been deleted, yet. The given \a method must not accept any
    arguments.

    This is useful to wrap handlers for connectToState() that should only
    be executed when the state is left.
 */

namespace QScxmlInternal {

static int signalIndex(const QMetaObject *meta, const QByteArray &signalName)
{
    Q_ASSERT(meta);

    int signalIndex = meta->indexOfSignal(signalName.constData());

    // If signal doesn't exist, return negative value
    if (signalIndex < 0)
        return signalIndex;

    // signal belongs to class whose meta object was passed, not some derived class.
    Q_ASSERT(meta->methodOffset() <= signalIndex);

    // Duplicate of computeOffsets in qobject.cpp
    const QMetaObject *m = meta->d.superdata;
    while (m) {
        const QMetaObjectPrivate *d = QMetaObjectPrivate::get(m);
        signalIndex = signalIndex - d->methodCount + d->signalCount;
        m = m->d.superdata;
    }

    // Asserting about the signal not being cloned would be nice, too, but not practical.

    return signalIndex;
}

void EventLoopHook::queueProcessEvents()
{
    if (smp->m_isProcessingEvents)
        return;

    QMetaObject::invokeMethod(this, "doProcessEvents", Qt::QueuedConnection);
}

void EventLoopHook::doProcessEvents()
{
    smp->processEvents();
}

void EventLoopHook::timerEvent(QTimerEvent *timerEvent)
{
    const int timerId = timerEvent->timerId();
    for (auto it = smp->m_delayedEvents.begin(), eit = smp->m_delayedEvents.end(); it != eit; ++it) {
        if (it->first == timerId) {
            QScxmlEvent *scxmlEvent = it->second;
            smp->m_delayedEvents.erase(it);
            smp->routeEvent(scxmlEvent);
            return;
        }
    }
}

void ScxmlEventRouter::route(const QStringList &segments, QScxmlEvent *event)
{
    emit eventOccurred(*event);
    if (!segments.isEmpty()) {
        auto it = children.find(segments.first());
        if (it != children.end())
            it.value()->route(segments.mid(1), event);
    }
}

static QString nextSegment(const QStringList &segments)
{
    if (segments.isEmpty())
        return QString();

    const QString &segment = segments.first();
    return segment == QLatin1String("*") ? QString() : segment;
}

ScxmlEventRouter *ScxmlEventRouter::child(const QString &segment)
{
    ScxmlEventRouter *&child = children[segment];
    if (child == nullptr)
        child = new ScxmlEventRouter(this);
    return child;
}

void ScxmlEventRouter::disconnectNotify(const QMetaMethod &signal)
{
    Q_UNUSED(signal);

    // Defer the actual work, as this may be called from a destructor, or the signal may not
    // actually be disconnected, yet.
    QTimer::singleShot(0, this, [this] {
        if (!children.isEmpty() || receivers(SIGNAL(eventOccurred(QScxmlEvent))) > 0)
            return;

        ScxmlEventRouter *parentRouter = qobject_cast<ScxmlEventRouter *>(parent());
        if (!parentRouter) // root node
            return;

        QHash<QString, ScxmlEventRouter *>::Iterator it = parentRouter->children.begin(),
                end = parentRouter->children.end();
        for (; it != end; ++it) {
            if (it.value() == this) {
                parentRouter->children.erase(it);
                parentRouter->disconnectNotify(QMetaMethod());
                break;
            }
        }

        deleteLater(); // The parent might delete itself, triggering QObject delete cascades.
    });
}

QMetaObject::Connection ScxmlEventRouter::connectToEvent(const QStringList &segments,
                                                         const QObject *receiver,
                                                         const char *method,
                                                         Qt::ConnectionType type)
{
    QString segment = nextSegment(segments);
    return segment.isEmpty() ?
                connect(this, SIGNAL(eventOccurred(QScxmlEvent)), receiver, method, type) :
                child(segment)->connectToEvent(segments.mid(1), receiver, method, type);
}

QMetaObject::Connection ScxmlEventRouter::connectToEvent(const QStringList &segments,
                                                         const QObject *receiver, void **slot,
                                                         QtPrivate::QSlotObjectBase *method,
                                                         Qt::ConnectionType type)
{
    QString segment = nextSegment(segments);
    if (segment.isEmpty()) {
        const int *types = Q_NULLPTR;
        if (type == Qt::QueuedConnection || type == Qt::BlockingQueuedConnection)
            types = QtPrivate::ConnectionTypes<QtPrivate::List<QScxmlEvent> >::types();

        const QMetaObject *meta = metaObject();
        static const int eventOccurredIndex = signalIndex(meta, "eventOccurred(QScxmlEvent)");
        return QObjectPrivate::connectImpl(this, eventOccurredIndex, receiver, slot, method, type,
                                           types, meta);
    } else {
        return child(segment)->connectToEvent(segments.mid(1), receiver, slot, method, type);
    }
}

} // namespace QScxmlInternal

QAtomicInt QScxmlStateMachinePrivate::m_sessionIdCounter = QAtomicInt(0);

QScxmlStateMachinePrivate::QScxmlStateMachinePrivate(const QMetaObject *metaObject)
    : QObjectPrivate()
    , m_sessionId(QScxmlStateMachinePrivate::generateSessionId(QStringLiteral("session-")))
    , m_isInvoked(false)
    , m_isInitialized(false)
    , m_isProcessingEvents(false)
    , m_dataModel(Q_NULLPTR)
    , m_loader(&m_defaultLoader)
    , m_executionEngine(Q_NULLPTR)
    , m_tableData(Q_NULLPTR)
    , m_parentStateMachine(Q_NULLPTR)
    , m_eventLoopHook(this)
    , m_metaObject(metaObject)
    , m_infoSignalProxy(nullptr)
{}

QScxmlStateMachinePrivate::~QScxmlStateMachinePrivate()
{
    for (const InvokedService &invokedService : m_invokedServices)
        delete invokedService.service;
    qDeleteAll(m_cachedFactories);
    delete m_executionEngine;
}

QScxmlStateMachinePrivate::ParserData *QScxmlStateMachinePrivate::parserData()
{
    if (m_parserData.isNull())
        m_parserData.reset(new ParserData);
    return m_parserData.data();
}

void QScxmlStateMachinePrivate::addService(int invokingState)
{
    Q_Q(QScxmlStateMachine);

    const int arrayId = m_stateTable->state(invokingState).serviceFactoryIds;
    if (arrayId == StateTable::InvalidIndex)
        return;

    const auto &ids = m_stateTable->array(arrayId);
    for (int id : ids) {
        auto factory = serviceFactory(id);
        auto service = factory->invoke(q);
        if (service == nullptr)
            continue; // service failed to start
        const QString serviceName = service->name();
        m_invokedServices[size_t(id)] = { invokingState, service, serviceName };
        service->start();
    }
    emitInvokedServicesChanged();
}

void QScxmlStateMachinePrivate::removeService(int invokingState)
{
    const int arrayId = m_stateTable->state(invokingState).serviceFactoryIds;
    if (arrayId == StateTable::InvalidIndex)
        return;

    for (size_t i = 0, ei = m_invokedServices.size(); i != ei; ++i) {
        auto &it = m_invokedServices[i];
        QScxmlInvokableService *service = it.service;
        if (it.invokingState == invokingState && service != nullptr) {
            it.service = nullptr;
            delete service;
        }
    }
    emitInvokedServicesChanged();
}

QScxmlInvokableServiceFactory *QScxmlStateMachinePrivate::serviceFactory(int id)
{
    Q_ASSERT(id <= m_stateTable->maxServiceId && id >= 0);
    QScxmlInvokableServiceFactory *& factory = m_cachedFactories[size_t(id)];
    if (factory == nullptr)
        factory = m_tableData->serviceFactory(id);
    return factory;
}

bool QScxmlStateMachinePrivate::executeInitialSetup()
{
    return m_executionEngine->execute(m_tableData->initialSetup());
}

void QScxmlStateMachinePrivate::routeEvent(QScxmlEvent *event)
{
    Q_Q(QScxmlStateMachine);

    if (!event)
        return;

    QString origin = event->origin();
    if (origin == QStringLiteral("#_parent")) {
        if (auto psm = m_parentStateMachine) {
            qCDebug(qscxmlLog) << q << "routing event" << event->name() << "from" << q->name() << "to parent" << psm->name();
                                 QScxmlStateMachinePrivate::get(psm)->postEvent(event);
        } else {
            qCDebug(qscxmlLog) << this << "is not invoked, so it cannot route a message to #_parent";
            delete event;
        }
    } else if (origin.startsWith(QStringLiteral("#_")) && origin != QStringLiteral("#_internal")) {
        // route to children
        auto originId = origin.midRef(2);
        for (const auto &invokedService : m_invokedServices) {
            auto service = invokedService.service;
            if (service == nullptr)
                continue;
            if (service->id() == originId) {
                qCDebug(qscxmlLog) << q << "routing event" << event->name()
                                   << "from" << q->name()
                                   << "to child" << service->id();
                service->postEvent(new QScxmlEvent(*event));
            }
        }
        delete event;
    } else {
        postEvent(event);
    }
}

void QScxmlStateMachinePrivate::postEvent(QScxmlEvent *event)
{
    Q_Q(QScxmlStateMachine);

    if (!event->name().startsWith(QStringLiteral("done.invoke."))) {
        for (int id = 0, end = static_cast<int>(m_invokedServices.size()); id != end; ++id) {
            auto service = m_invokedServices[id].service;
            if (service == nullptr)
                continue;
            auto factory = serviceFactory(id);
            if (event->invokeId() == service->id()) {
                setEvent(event);

                const QScxmlExecutableContent::ContainerId finalize
                        = factory->invokeInfo().finalize;
                if (finalize != QScxmlExecutableContent::NoContainer) {
                    auto psm = service->parentStateMachine();
                    qCDebug(qscxmlLog) << psm << "running finalize on event";
                    auto smp = QScxmlStateMachinePrivate::get(psm);
                    smp->m_executionEngine->execute(finalize);
                }

                resetEvent();
            }
            if (factory->invokeInfo().autoforward) {
                qCDebug(qscxmlLog) << q << "auto-forwarding event" << event->name()
                                   << "from" << q->name()
                                   << "to child" << service->id();
                service->postEvent(new QScxmlEvent(*event));
            }
        }
    }

    if (event->eventType() == QScxmlEvent::ExternalEvent)
        m_router.route(event->name().split(QLatin1Char('.')), event);

    if (event->eventType() == QScxmlEvent::ExternalEvent) {
        qCDebug(qscxmlLog) << q << "posting external event" << event->name();
        m_externalQueue.enqueue(event);
    } else {
        qCDebug(qscxmlLog) << q << "posting internal event" << event->name();
        m_internalQueue.enqueue(event);
    }

    m_eventLoopHook.queueProcessEvents();
}

void QScxmlStateMachinePrivate::submitDelayedEvent(QScxmlEvent *event)
{
    Q_ASSERT(event);
    Q_ASSERT(event->delay() > 0);

    const int timerId = m_eventLoopHook.startTimer(event->delay());
    if (timerId == 0) {
        qWarning("QScxmlStateMachinePrivate::submitDelayedEvent: "
                 "failed to start timer for event '%s' (%p)",
                 qPrintable(event->name()), event);
        delete event;
        return;
    }
    m_delayedEvents.push_back(std::make_pair(timerId, event));

    qCDebug(qscxmlLog) << q_func()
                       << ": delayed event" << event->name()
                       << "(" << event << ") got id:" << timerId;
}

/*!
 * Submits an error event to the external event queue of this state machine.
 *
 * The type of the error is specified by \a type. The value of type has to begin
 * with the string \e error. For example \c {error.execution}. The message,
 * \a message, decribes the error and is passed to the event as the
 * \c errorMessage property. The \a sendId of the message causing the error is specified, if it has
 * one.
 */
void QScxmlStateMachinePrivate::submitError(const QString &type, const QString &message,
                                            const QString &sendId)
{
    Q_Q(QScxmlStateMachine);
    qCDebug(qscxmlLog) << q << "had error" << type << ":" << message;
    if (!type.startsWith(QStringLiteral("error.")))
        qCWarning(qscxmlLog) << q << "Message type of error message does not start with 'error.'!";
    q->submitEvent(QScxmlEventBuilder::errorEvent(q, type, message, sendId));
}

void QScxmlStateMachinePrivate::start()
{
    Q_Q(QScxmlStateMachine);

    if (m_stateTable->binding == StateTable::LateBinding)
        m_isFirstStateEntry.resize(m_stateTable->stateCount, true);

    bool running = isRunnable() && !isPaused();
    m_runningState = Starting;
    Q_ASSERT(m_stateTable->initialTransition != StateTable::InvalidIndex);

    if (!running)
        emit q->runningChanged(true);
}

void QScxmlStateMachinePrivate::pause()
{
    Q_Q(QScxmlStateMachine);

    if (isRunnable() && !isPaused()) {
        m_runningState = Paused;
        emit q->runningChanged(false);
    }
}

void QScxmlStateMachinePrivate::processEvents()
{
    if (m_isProcessingEvents || (!isRunnable() && !isPaused()))
        return;

    m_isProcessingEvents = true;

    Q_Q(QScxmlStateMachine);
    qCDebug(qscxmlLog) << q_func() << "starting macrostep";

    while (isRunnable() && !isPaused()) {
        if (m_runningState == Starting) {
            enterStates({m_stateTable->initialTransition});
            if (m_runningState == Starting)
                m_runningState = Running;
            continue;
        }

        OrderedSet enabledTransitions;
        std::vector<int> configurationInDocumentOrder = m_configuration.list();
        std::sort(configurationInDocumentOrder.begin(), configurationInDocumentOrder.end());
        selectTransitions(enabledTransitions, configurationInDocumentOrder, nullptr);
        if (!enabledTransitions.isEmpty()) {
            microstep(enabledTransitions);
        } else if (!m_internalQueue.isEmpty()) {
            auto event = m_internalQueue.dequeue();
            setEvent(event);
            selectTransitions(enabledTransitions, configurationInDocumentOrder, event);
            if (!enabledTransitions.isEmpty()) {
                microstep(enabledTransitions);
            }
            resetEvent();
            delete event;
        } else if (!m_externalQueue.isEmpty()) {
            auto event = m_externalQueue.dequeue();
            setEvent(event);
            selectTransitions(enabledTransitions, configurationInDocumentOrder, event);
            if (!enabledTransitions.isEmpty()) {
                microstep(enabledTransitions);
            }
            resetEvent();
            delete event;
        } else {
            // nothing to do, so:
            break;
        }
    }

    if (!m_statesToInvoke.empty()) {
        for (int stateId : m_statesToInvoke)
            addService(stateId);
        m_statesToInvoke.clear();
    }

    qCDebug(qscxmlLog) << q_func()
                       << "finished macrostep, runnable:" << isRunnable()
                       << "paused:" << isPaused();
    emit q->reachedStableState();
    if (!isRunnable() && !isPaused()) {
        exitInterpreter();
        emit q->finished();
    }

    m_isProcessingEvents = false;
}

void QScxmlStateMachinePrivate::setEvent(QScxmlEvent *event)
{
    Q_ASSERT(event);
    m_dataModel->setScxmlEvent(*event);
}

void QScxmlStateMachinePrivate::resetEvent()
{
    m_dataModel->setScxmlEvent(QScxmlEvent());
}

void QScxmlStateMachinePrivate::emitStateActive(int stateIndex, bool active)
{
    Q_Q(QScxmlStateMachine);
    void *args[] = { Q_NULLPTR, const_cast<void*>(reinterpret_cast<const void*>(&active)) };
    QMetaObject::activate(q, m_metaObject, stateIndex, args);
}

void QScxmlStateMachinePrivate::emitInvokedServicesChanged()
{
    Q_Q(QScxmlStateMachine);
    emit q->invokedServicesChanged(q->invokedServices());
}

void QScxmlStateMachinePrivate::attach(QScxmlStateMachineInfo *info)
{
    Q_Q(QScxmlStateMachine);

    if (!m_infoSignalProxy)
        m_infoSignalProxy = new QScxmlInternal::StateMachineInfoProxy(q);

    QObject::connect(m_infoSignalProxy, &QScxmlInternal::StateMachineInfoProxy::statesEntered,
                     info, &QScxmlStateMachineInfo::statesEntered);
    QObject::connect(m_infoSignalProxy, &QScxmlInternal::StateMachineInfoProxy::statesExited,
                     info, &QScxmlStateMachineInfo::statesExited);
    QObject::connect(m_infoSignalProxy,&QScxmlInternal::StateMachineInfoProxy::transitionsTriggered,
                     info, &QScxmlStateMachineInfo::transitionsTriggered);
}

QStringList QScxmlStateMachinePrivate::stateNames(const std::vector<int> &stateIndexes) const
{
    QStringList names;
    for (int idx : stateIndexes)
        names.append(m_tableData->string(m_stateTable->state(idx).name));
    return names;
}

std::vector<int> QScxmlStateMachinePrivate::historyStates(int stateIdx) const {
    const StateTable::Array kids = m_stateTable->array(m_stateTable->state(stateIdx).childStates);
    std::vector<int> res;
    if (!kids.isValid()) return res;
    for (int k : kids) {
        if (m_stateTable->state(k).isHistoryState())
            res.push_back(k);
    }
    return res;
}

void QScxmlStateMachinePrivate::exitInterpreter()
{
    qCDebug(qscxmlLog) << q_func() << "exiting SCXML processing";

    for (auto it : m_delayedEvents) {
        m_eventLoopHook.killTimer(it.first);
        delete it.second;
    }
    m_delayedEvents.clear();

    auto statesToExitSorted = m_configuration.list();
    std::sort(statesToExitSorted.begin(), statesToExitSorted.end(), std::greater<int>());
    for (int stateIndex : statesToExitSorted) {
        const auto &state = m_stateTable->state(stateIndex);
        if (state.exitInstructions != StateTable::InvalidIndex) {
            m_executionEngine->execute(state.exitInstructions);
        }
        removeService(stateIndex);
        if (state.type == StateTable::State::Final && state.parentIsScxmlElement()) {
            returnDoneEvent(state.doneData);
        }
    }
}

void QScxmlStateMachinePrivate::returnDoneEvent(QScxmlExecutableContent::ContainerId doneData)
{
    Q_Q(QScxmlStateMachine);

    m_executionEngine->execute(doneData, QVariant());
    if (m_isInvoked) {
        auto e = new QScxmlEvent;
        e->setName(QStringLiteral("done.invoke.") + q->sessionId());
        e->setInvokeId(q->sessionId());
        QScxmlStateMachinePrivate::get(m_parentStateMachine)->postEvent(e);
    }
}

bool QScxmlStateMachinePrivate::nameMatch(const StateTable::Array &patterns,
                                          QScxmlEvent *event) const
{
    const QString eventName = event->name();
    bool selected = false;
    for (int eventSelectorIter = 0; eventSelectorIter < patterns.size(); ++eventSelectorIter) {
        QString eventStr = m_tableData->string(patterns[eventSelectorIter]);
        if (eventStr == QStringLiteral("*")) {
            selected = true;
            break;
        }
        if (eventStr.endsWith(QStringLiteral(".*")))
            eventStr.chop(2);
        if (eventName.startsWith(eventStr)) {
            QChar nextC = QLatin1Char('.');
            if (eventName.size() > eventStr.size())
                nextC = eventName.at(eventStr.size());
            if (nextC == QLatin1Char('.') || nextC == QLatin1Char('(')) {
                selected = true;
                break;
            }
        }
    }
    return selected;
}

void QScxmlStateMachinePrivate::selectTransitions(OrderedSet &enabledTransitions,
                                                  const std::vector<int> &configInDocumentOrder,
                                                  QScxmlEvent *event) const
{
    if (event == nullptr) {
        qCDebug(qscxmlLog) << q_func() << "selectEventlessTransitions";
    } else {
        qCDebug(qscxmlLog) << q_func() << "selectTransitions with event"
                           << QScxmlEventPrivate::debugString(event).constData();
    }

    std::vector<int> states;
    states.reserve(16);
    for (int configStateIdx : configInDocumentOrder) {
        if (m_stateTable->state(configStateIdx).isAtomic()) {
            states.clear();
            states.push_back(configStateIdx);
            getProperAncestors(&states, configStateIdx, -1);
            for (int stateIdx : states) {
                bool finishedWithThisConfigState  = false;

                if (stateIdx == -1) {
                    // the state machine has no transitions (other than the initial one, which has
                    // already been taken at this point)
                    continue;
                }
                const auto &state = m_stateTable->state(stateIdx);
                const StateTable::Array transitions = m_stateTable->array(state.transitions);
                if (!transitions.isValid())
                    continue;
                std::vector<int> sortedTransitions(transitions.size(), -1);
                std::copy(transitions.begin(), transitions.end(), sortedTransitions.begin());
                for (int transitionIndex : sortedTransitions) {
                    const StateTable::Transition &t = m_stateTable->transition(transitionIndex);
                    bool enabled = false;
                    if (event == nullptr) {
                        if (t.events == -1) {
                            if (t.condition == -1) {
                                enabled = true;
                            } else {
                                bool ok = false;
                                enabled = m_dataModel->evaluateToBool(t.condition, &ok) && ok;
                            }
                        }
                    } else {
                        if (t.events != -1 && nameMatch(m_stateTable->array(t.events), event)) {
                            if (t.condition == -1) {
                                enabled = true;
                            } else {
                                bool ok = false;
                                enabled = m_dataModel->evaluateToBool(t.condition, &ok) && ok;
                            }
                        }
                    }
                    if (enabled) {
                        enabledTransitions.add(transitionIndex);
                        finishedWithThisConfigState = true;
                        break; // stop iterating over transitions
                    }
                }

                if (finishedWithThisConfigState)
                    break; // stop iterating over ancestors
            }
        }
    }
    if (!enabledTransitions.isEmpty())
        removeConflictingTransitions(&enabledTransitions);
}

void QScxmlStateMachinePrivate::removeConflictingTransitions(OrderedSet *enabledTransitions) const
{
    Q_ASSERT(enabledTransitions);

    auto sortedTransitions = enabledTransitions->takeList();
    std::sort(sortedTransitions.begin(), sortedTransitions.end(), [this](int t1, int t2) -> bool {
        auto descendantDepth = [this](int state, int ancestor)->int {
            int depth = 0;
            for (int it = state; it != -1; it = m_stateTable->state(it).parent) {
                if (it == ancestor)
                    break;
                ++depth;
            }
            return depth;
        };

        const auto &s1 = m_stateTable->transition(t1).source;
        const auto &s2 = m_stateTable->transition(t2).source;
        if (s1 == s2) {
            return t1 < t2;
        } else if (isDescendant(s1, s2)) {
            return true;
        } else if (isDescendant(s2, s1)) {
            return false;
        } else {
            const int lcca = findLCCA({ s1, s2 });
            const int s1Depth = descendantDepth(s1, lcca);
            const int s2Depth = descendantDepth(s2, lcca);
            if (s1Depth == s2Depth)
                return s1 < s2;
            else
                return s1Depth > s2Depth;
        }
    });

    OrderedSet filteredTransitions;
    for (int t1 : sortedTransitions) {
        OrderedSet transitionsToRemove;
        bool t1Preempted = false;
        OrderedSet exitSetT1;
        computeExitSet({t1}, exitSetT1);
        const int source1 = m_stateTable->transition(t1).source;
        for (int t2 : filteredTransitions) {
            OrderedSet exitSetT2;
            computeExitSet({t2}, exitSetT2);
            if (exitSetT1.intersectsWith(exitSetT2)) {
                const int source2 = m_stateTable->transition(t2).source;
                if (isDescendant(source1, source2)) {
                    transitionsToRemove.add(t2);
                } else {
                    t1Preempted = true;
                    break;
                }
            }
        }
        if (!t1Preempted) {
            for (int t3 : transitionsToRemove) {
                filteredTransitions.remove(t3);
            }
            filteredTransitions.add(t1);
        }
    }
    *enabledTransitions = filteredTransitions;
}

void QScxmlStateMachinePrivate::getProperAncestors(std::vector<int> *ancestors, int state1,
                                                   int state2) const
{
    Q_ASSERT(ancestors);

    if (state1 == -1) {
        return;
    }

    int parent = state1;
    do {
        parent = m_stateTable->state(parent).parent;
        if (parent == state2) {
            break;
        }
        ancestors->push_back(parent);
    } while (parent != -1);
}

void QScxmlStateMachinePrivate::microstep(const OrderedSet &enabledTransitions)
{
    if (qscxmlLog().isDebugEnabled()) {
        qCDebug(qscxmlLog) << q_func()
                           << "starting microstep, configuration:"
                           << stateNames(m_configuration.list());
        qCDebug(qscxmlLog) << q_func() << "enabled transitions:";
        for (int t : enabledTransitions) {
            const auto &transition = m_stateTable->transition(t);
            QString from = QStringLiteral("(none)");
            if (transition.source != StateTable::InvalidIndex)
                from = m_tableData->string(m_stateTable->state(transition.source).name);
            QStringList to;
            if (transition.targets == StateTable::InvalidIndex) {
                to.append(QStringLiteral("(none)"));
            } else {
                for (int t : m_stateTable->array(transition.targets))
                    to.append(m_tableData->string(m_stateTable->state(t).name));
            }
            qCDebug(qscxmlLog) << q_func() << "\t" << t << ":" << from << "->"
                               << to.join(QLatin1Char(','));
        }
    }

    exitStates(enabledTransitions);
    executeTransitionContent(enabledTransitions);
    enterStates(enabledTransitions);

    qCDebug(qscxmlLog) << q_func() << "finished microstep, configuration:"
                       << stateNames(m_configuration.list());
}

void QScxmlStateMachinePrivate::exitStates(const OrderedSet &enabledTransitions)
{
    OrderedSet statesToExit;
    computeExitSet(enabledTransitions, statesToExit);
    auto statesToExitSorted = statesToExit.takeList();
    std::sort(statesToExitSorted.begin(), statesToExitSorted.end(), std::greater<int>());
    qCDebug(qscxmlLog) << q_func() << "exiting states" << stateNames(statesToExitSorted);
    for (int s : statesToExitSorted) {
        const auto &state = m_stateTable->state(s);
        if (state.serviceFactoryIds != StateTable::InvalidIndex)
            m_statesToInvoke.remove(s);
    }
    for (int s : statesToExitSorted) {
        for (int h : historyStates(s)) {
            const auto &hState = m_stateTable->state(h);
            QVector<int> history;

            for (int s0 : m_configuration) {
                const auto &s0State = m_stateTable->state(s0);
                if (hState.type == StateTable::State::DeepHistory) {
                    if (s0State.isAtomic() && isDescendant(s0, s))
                        history.append(s0);
                } else {
                    if (s0State.parent == s)
                        history.append(s0);
                }
            }

            m_historyValue[h] = history;
        }
    }
    for (int s : statesToExitSorted) {
        const auto &state = m_stateTable->state(s);
        if (state.exitInstructions != StateTable::InvalidIndex)
            m_executionEngine->execute(state.exitInstructions);
        m_configuration.remove(s);
        emitStateActive(s, false);
        removeService(s);
    }

    if (m_infoSignalProxy) {
        emit m_infoSignalProxy->statesExited(
                QVector<QScxmlStateMachineInfo::StateId>::fromStdVector(statesToExitSorted));
    }
}

void QScxmlStateMachinePrivate::computeExitSet(const OrderedSet &enabledTransitions,
                                               OrderedSet &statesToExit) const
{
    for (int t : enabledTransitions) {
        const auto &transition = m_stateTable->transition(t);
        if (transition.targets == StateTable::InvalidIndex) {
            // nothing to do here: there is no exit set
        } else {
            const int domain = getTransitionDomain(t);
            for (int s : m_configuration) {
                if (isDescendant(s, domain))
                    statesToExit.add(s);
            }
        }
    }
}

void QScxmlStateMachinePrivate::executeTransitionContent(const OrderedSet &enabledTransitions)
{
    for (int t : enabledTransitions) {
        const auto &transition = m_stateTable->transition(t);
        if (transition.transitionInstructions != StateTable::InvalidIndex)
            m_executionEngine->execute(transition.transitionInstructions);
    }

    if (m_infoSignalProxy) {
        emit m_infoSignalProxy->transitionsTriggered(
                QVector<QScxmlStateMachineInfo::TransitionId>::fromStdVector(
                    enabledTransitions.list()));
    }
}

void QScxmlStateMachinePrivate::enterStates(const OrderedSet &enabledTransitions)
{
    Q_Q(QScxmlStateMachine);

    OrderedSet statesToEnter, statesForDefaultEntry;
    HistoryContent defaultHistoryContent;
    computeEntrySet(enabledTransitions, &statesToEnter, &statesForDefaultEntry,
                    &defaultHistoryContent);
    auto sortedStates = statesToEnter.takeList();
    std::sort(sortedStates.begin(), sortedStates.end());
    qCDebug(qscxmlLog) << q_func() << "entering states" << stateNames(sortedStates);
    for (int s : sortedStates) {
        const auto &state = m_stateTable->state(s);
        m_configuration.add(s);
        if (state.serviceFactoryIds != StateTable::InvalidIndex)
            m_statesToInvoke.insert(s);
        if (m_stateTable->binding == StateTable::LateBinding && m_isFirstStateEntry[s]) {
            if (state.initInstructions != StateTable::InvalidIndex)
                m_executionEngine->execute(state.initInstructions);
            m_isFirstStateEntry[s] = false;
        }
        if (state.entryInstructions != StateTable::InvalidIndex)
            m_executionEngine->execute(state.entryInstructions);
        if (statesForDefaultEntry.contains(s)) {
            const auto &initialTransition = m_stateTable->transition(state.initialTransition);
            if (initialTransition.transitionInstructions != StateTable::InvalidIndex)
                m_executionEngine->execute(initialTransition.transitionInstructions);
        }
        const int dhc = defaultHistoryContent.value(s);
        if (dhc != StateTable::InvalidIndex)
            m_executionEngine->execute(dhc);
        if (state.type == StateTable::State::Final) {
            if (state.parentIsScxmlElement()) {
                bool running = isRunnable() && !isPaused();
                m_runningState = Finished;
                if (running)
                    emit q->runningChanged(false);
            } else {
                const auto &parent = m_stateTable->state(state.parent);
                m_executionEngine->execute(state.doneData, m_tableData->string(parent.name));
                if (parent.parent != StateTable::InvalidIndex) {
                    const auto &grandParent = m_stateTable->state(parent.parent);
                    if (grandParent.isParallel()) {
                        if (allInFinalStates(getChildStates(grandParent))) {
                            auto e = new QScxmlEvent;
                            e->setEventType(QScxmlEvent::InternalEvent);
                            e->setName(QStringLiteral("done.state.")
                                       + m_tableData->string(grandParent.name));
                            q->submitEvent(e);
                        }
                    }
                }
            }
        }
    }
    for (int s : sortedStates)
        emitStateActive(s, true);
    if (m_infoSignalProxy) {
        emit m_infoSignalProxy->statesEntered(
                QVector<QScxmlStateMachineInfo::StateId>::fromStdVector(sortedStates));
    }
}

void QScxmlStateMachinePrivate::computeEntrySet(const OrderedSet &enabledTransitions,
                                                OrderedSet *statesToEnter,
                                                OrderedSet *statesForDefaultEntry,
                                                HistoryContent *defaultHistoryContent) const
{
    Q_ASSERT(statesToEnter);
    Q_ASSERT(statesForDefaultEntry);
    Q_ASSERT(defaultHistoryContent);

    for (int t : enabledTransitions) {
        const auto &transition = m_stateTable->transition(t);
        if (transition.targets == StateTable::InvalidIndex)
            // targetless transition, so nothing to do
            continue;
        for (int s : m_stateTable->array(transition.targets))
            addDescendantStatesToEnter(s, statesToEnter, statesForDefaultEntry,
                                       defaultHistoryContent);
        auto ancestor = getTransitionDomain(t);
        OrderedSet targets;
        getEffectiveTargetStates(&targets, t);
        for (auto s : targets)
            addAncestorStatesToEnter(s, ancestor, statesToEnter, statesForDefaultEntry,
                                     defaultHistoryContent);
    }
}

void QScxmlStateMachinePrivate::addDescendantStatesToEnter(
        int stateIndex, OrderedSet *statesToEnter, OrderedSet *statesForDefaultEntry,
        HistoryContent *defaultHistoryContent) const
{
    Q_ASSERT(statesToEnter);
    Q_ASSERT(statesForDefaultEntry);
    Q_ASSERT(defaultHistoryContent);

    const auto &state = m_stateTable->state(stateIndex);
    if (state.isHistoryState()) {
        HistoryValues::const_iterator historyValueIter = m_historyValue.find(stateIndex);
        if (historyValueIter != m_historyValue.end()) {
            auto historyValue = historyValueIter.value();
            for (int s : historyValue)
                addDescendantStatesToEnter(s, statesToEnter, statesForDefaultEntry,
                                           defaultHistoryContent);
            for (int s : historyValue)
                addAncestorStatesToEnter(s, state.parent, statesToEnter, statesForDefaultEntry,
                                         defaultHistoryContent);
        } else {
            const auto transitionIdx = m_stateTable->array(state.transitions)[0];
            const auto &defaultHistoryTransition = m_stateTable->transition(transitionIdx);
            defaultHistoryContent->operator[](state.parent) =
                    defaultHistoryTransition.transitionInstructions;
            StateTable::Array targetStates = m_stateTable->array(defaultHistoryTransition.targets);
            for (int s : targetStates)
                addDescendantStatesToEnter(s, statesToEnter, statesForDefaultEntry,
                                           defaultHistoryContent);
            for (int s : targetStates)
                addAncestorStatesToEnter(s, state.parent, statesToEnter, statesForDefaultEntry,
                                         defaultHistoryContent);
        }
    } else {
        statesToEnter->add(stateIndex);
        if (state.isCompound()) {
            statesForDefaultEntry->add(stateIndex);
            if (state.initialTransition != StateTable::InvalidIndex) {
                auto initialTransition = m_stateTable->transition(state.initialTransition);
                auto initialTransitionTargets = m_stateTable->array(initialTransition.targets);
                for (int targetStateIndex : initialTransitionTargets)
                    addDescendantStatesToEnter(targetStateIndex, statesToEnter,
                                               statesForDefaultEntry, defaultHistoryContent);
                for (int targetStateIndex : initialTransitionTargets)
                    addAncestorStatesToEnter(targetStateIndex, stateIndex, statesToEnter,
                                             statesForDefaultEntry, defaultHistoryContent);
            }
        } else {
            if (state.isParallel()) {
                for (int child : getChildStates(state)) {
                    if (!hasDescendant(*statesToEnter, child))
                        addDescendantStatesToEnter(child, statesToEnter, statesForDefaultEntry,
                                                   defaultHistoryContent);
                }
            }
        }
    }
}

void QScxmlStateMachinePrivate::addAncestorStatesToEnter(
        int stateIndex, int ancestorIndex, OrderedSet *statesToEnter,
        OrderedSet *statesForDefaultEntry, HistoryContent *defaultHistoryContent) const
{
    Q_ASSERT(statesToEnter);
    Q_ASSERT(statesForDefaultEntry);
    Q_ASSERT(defaultHistoryContent);

    std::vector<int> ancestors;
    getProperAncestors(&ancestors, stateIndex, ancestorIndex);
    for (int anc : ancestors) {
        if (anc == -1) {
            // we can't enter the state machine itself, so:
            continue;
        }
        statesToEnter->add(anc);
        const auto &ancState = m_stateTable->state(anc);
        if (ancState.isParallel()) {
            for (int child : getChildStates(ancState)) {
                if (!hasDescendant(*statesToEnter, child))
                    addDescendantStatesToEnter(child, statesToEnter, statesForDefaultEntry,
                                               defaultHistoryContent);
            }
        }
    }
}

std::vector<int> QScxmlStateMachinePrivate::getChildStates(
        const QScxmlExecutableContent::StateTable::State &state) const
{
    std::vector<int> childStates;
    auto kids = m_stateTable->array(state.childStates);
    if (kids.isValid()) {
        childStates.reserve(kids.size());
        for (int kiddo : kids) {
            switch (m_stateTable->state(kiddo).type) {
            case StateTable::State::Normal:
            case StateTable::State::Final:
            case StateTable::State::Parallel:
                childStates.push_back(kiddo);
                break;
            default:
                break;
            }
        }
    }
    return childStates;
}

bool QScxmlStateMachinePrivate::hasDescendant(const OrderedSet &statesToEnter, int childIdx) const
{
    for (int s : statesToEnter) {
        if (isDescendant(s, childIdx))
            return true;
    }
    return false;
}

bool QScxmlStateMachinePrivate::allDescendants(const OrderedSet &statesToEnter, int childdx) const
{
    for (int s : statesToEnter) {
        if (!isDescendant(s, childdx))
            return false;
    }
    return true;
}

bool QScxmlStateMachinePrivate::isDescendant(int state1, int state2) const
{
    int parent = state1;
    do {
        parent = m_stateTable->state(parent).parent;
        if (parent == state2)
            return true;
    } while (parent != -1);
    return false;
}

bool QScxmlStateMachinePrivate::allInFinalStates(const std::vector<int> &states) const
{
    if (states.empty())
        return false;

    for (int idx : states) {
        if (!isInFinalState(idx))
            return false;
    }

    return true;
}

bool QScxmlStateMachinePrivate::someInFinalStates(const std::vector<int> &states) const
{
    for (int stateIndex : states) {
        const auto &state = m_stateTable->state(stateIndex);
        if (state.type == StateTable::State::Final && m_configuration.contains(stateIndex))
            return true;
    }
    return false;
}

bool QScxmlStateMachinePrivate::isInFinalState(int stateIndex) const
{
    const auto &state = m_stateTable->state(stateIndex);
    if (state.isCompound())
        return someInFinalStates(getChildStates(state)) && m_configuration.contains(stateIndex);
    else if (state.isParallel())
        return allInFinalStates(getChildStates(state));
    else
        return false;
}

int QScxmlStateMachinePrivate::getTransitionDomain(int transitionIndex) const
{
    const auto &transition = m_stateTable->transition(transitionIndex);
    if (transition.source == -1)
        //oooh, we have the initial transition of the state machine.
        return -1;

    OrderedSet tstates;
    getEffectiveTargetStates(&tstates, transitionIndex);
    if (tstates.isEmpty()) {
        return StateTable::InvalidIndex;
    } else {
        const auto &sourceState = m_stateTable->state(transition.source);
        if (transition.type == StateTable::Transition::Internal
                && sourceState.isCompound()
                && allDescendants(tstates, transition.source)) {
            return transition.source;
        } else {
            tstates.add(transition.source);
            return findLCCA(std::move(tstates));
        }
    }
}

int QScxmlStateMachinePrivate::findLCCA(OrderedSet &&states) const
{
    std::vector<int> ancestors;
    const int head = *states.begin();
    OrderedSet tail(std::move(states));
    tail.removeHead();

    getProperAncestors(&ancestors, head, StateTable::InvalidIndex);
    for (int anc : ancestors) {
        if (anc != -1) { // the state machine itself is always compound
            const auto &ancState = m_stateTable->state(anc);
            if (!ancState.isCompound())
                continue;
        }

        if (allDescendants(tail, anc))
            return anc;
    }

    return StateTable::InvalidIndex;
}

void QScxmlStateMachinePrivate::getEffectiveTargetStates(OrderedSet *targets,
                                                         int transitionIndex) const
{
    Q_ASSERT(targets);

    const auto &transition = m_stateTable->transition(transitionIndex);
    for (int s : m_stateTable->array(transition.targets)) {
        const auto &state = m_stateTable->state(s);
        if (state.isHistoryState()) {
            HistoryValues::const_iterator historyValueIter = m_historyValue.find(s);
            if (historyValueIter != m_historyValue.end()) {
                for (int historyState : historyValueIter.value()) {
                    targets->add(historyState);
                }
            } else {
                getEffectiveTargetStates(targets, m_stateTable->array(state.transitions)[0]);
            }
        } else {
            targets->add(s);
        }
    }
}

/*!
 * Creates a state machine from the SCXML file specified by \a fileName.
 *
 * This method will always return a state machine. If errors occur while reading the SCXML file,
 * the state machine cannot be started. The errors can be retrieved by calling the parseErrors()
 * method.
 *
 * \sa parseErrors()
 */
QScxmlStateMachine *QScxmlStateMachine::fromFile(const QString &fileName)
{
    QFile scxmlFile(fileName);
    if (!scxmlFile.open(QIODevice::ReadOnly)) {
        auto stateMachine = new QScxmlStateMachine(&QScxmlStateMachine::staticMetaObject);
        QScxmlError err(scxmlFile.fileName(), 0, 0, QStringLiteral("cannot open for reading"));
        QScxmlStateMachinePrivate::get(stateMachine)->parserData()->m_errors.append(err);
        return stateMachine;
    }

    QScxmlStateMachine *stateMachine = fromData(&scxmlFile, fileName);
    scxmlFile.close();
    return stateMachine;
}

/*!
 * Creates a state machine by reading from the QIODevice specified by \a data.
 *
 * This method will always return a state machine. If errors occur while reading the SCXML file,
 * \a fileName, the state machine cannot be started. The errors can be retrieved by calling the
 * parseErrors() method.
 *
 * \sa parseErrors()
 */
QScxmlStateMachine *QScxmlStateMachine::fromData(QIODevice *data, const QString &fileName)
{
    QXmlStreamReader xmlReader(data);
    QScxmlCompiler compiler(&xmlReader);
    compiler.setFileName(fileName);
    return compiler.compile();
}

QVector<QScxmlError> QScxmlStateMachine::parseErrors() const
{
    Q_D(const QScxmlStateMachine);
    return d->m_parserData ? d->m_parserData->m_errors : QVector<QScxmlError>();
}

QScxmlStateMachine::QScxmlStateMachine(const QMetaObject *metaObject, QObject *parent)
    : QObject(*new QScxmlStateMachinePrivate(metaObject), parent)
{
    Q_D(QScxmlStateMachine);
    d->m_executionEngine = new QScxmlExecutionEngine(this);
}

QScxmlStateMachine::QScxmlStateMachine(QScxmlStateMachinePrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
    Q_D(QScxmlStateMachine);
    d->m_executionEngine = new QScxmlExecutionEngine(this);
}

/*!
    \property QScxmlStateMachine::running

    \brief the running state of this state machine

    \sa start()
 */

/*!
    \property QScxmlStateMachine::dataModel

    \brief The data model to be used for this state machine.

    SCXML data models are described in
    \l {SCXML Specification - 5 Data Model and Data Manipulation}. For more
    information about supported data models, see \l {SCXML Compliance}.

    Changing the data model when the state machine has been \c initialized is
    not specified in the SCXML standard and leads to undefined behavior.

    \sa QScxmlDataModel, QScxmlNullDataModel, QScxmlEcmaScriptDataModel,
        QScxmlCppDataModel
*/

/*!
    \property QScxmlStateMachine::initialized

    \brief Whether the state machine has been initialized.

    It is \c true if the state machine has been initialized, \c false otherwise.

    \sa QScxmlStateMachine::init(), QScxmlDataModel
*/

/*!
    \property QScxmlStateMachine::initialValues

    \brief The initial values to be used for setting up the data model.

    \sa QScxmlStateMachine::init(), QScxmlDataModel
*/

/*!
    \property QScxmlStateMachine::sessionId

    \brief The session ID of the current state machine.

    The session ID is used for message routing between parent and child state machines. If a state
    machine is started by an \c <invoke> element, any event it sends will have the \c invokeid field
    set to the session ID. The state machine will use the origin of an event (which is set by the
    \e target or \e targetexpr attribute in a \c <send> element) to dispatch messages to the correct
    child state machine.

    \sa QScxmlEvent::invokeId()
 */

/*!
    \property QScxmlStateMachine::name

    \brief The name of the state machine as set by the \e name attribute of the \c <scxml> tag.
 */

/*!
    \property QScxmlStateMachine::invoked

    \brief Whether the state machine was invoked from an outer state machine.

    \c true when the state machine was started as a service with the \c <invoke> element,
    \c false otherwise.
 */

/*!
    \property QScxmlStateMachine::parseErrors

    \brief The list of parse errors that occurred while creating a state machine from an SCXML file.
 */

/*!
    \property QScxmlStateMachine::loader

    \brief The loader that is currently used to resolve and load URIs for the state machine.
 */

/*!
    \property QScxmlStateMachine::tableData

    \brief The table data that is used when generating C++ from an SCXML file.

    The class implementing
    the state machine will use this property to assign the generated table
    data. The state machine does not assume ownership of the table data.
 */

QString QScxmlStateMachine::sessionId() const
{
    Q_D(const QScxmlStateMachine);

    return d->m_sessionId;
}

QString QScxmlStateMachinePrivate::generateSessionId(const QString &prefix)
{
    int id = ++QScxmlStateMachinePrivate::m_sessionIdCounter;
    return prefix + QString::number(id);
}

bool QScxmlStateMachine::isInvoked() const
{
    Q_D(const QScxmlStateMachine);
    return d->m_isInvoked;
}

bool QScxmlStateMachine::isInitialized() const
{
    Q_D(const QScxmlStateMachine);
    return d->m_isInitialized;
}

/*!
 * Sets the data model for this state machine to \a model. There is a 1:1
 * relation between state machines and models. After setting the model once you
 * cannot change it anymore. Any further attempts to set the model using this
 * method will be ignored.
 */
void QScxmlStateMachine::setDataModel(QScxmlDataModel *model)
{
    Q_D(QScxmlStateMachine);

    if (d->m_dataModel == Q_NULLPTR && model != Q_NULLPTR) {
        d->m_dataModel = model;
        if (model)
            model->setStateMachine(this);
        emit dataModelChanged(model);
    }
}

/*!
 * Returns the data model used by the state machine.
 */
QScxmlDataModel *QScxmlStateMachine::dataModel() const
{
    Q_D(const QScxmlStateMachine);

    return d->m_dataModel;
}

void QScxmlStateMachine::setLoader(QScxmlCompiler::Loader *loader)
{
    Q_D(QScxmlStateMachine);

    if (loader != d->m_loader) {
        d->m_loader = loader;
        emit loaderChanged(loader);
    }
}

QScxmlCompiler::Loader *QScxmlStateMachine::loader() const
{
    Q_D(const QScxmlStateMachine);

    return d->m_loader;
}

QScxmlTableData *QScxmlStateMachine::tableData() const
{
    Q_D(const QScxmlStateMachine);

    return d->m_tableData;
}

void QScxmlStateMachine::setTableData(QScxmlTableData *tableData)
{
    Q_D(QScxmlStateMachine);

    if (d->m_tableData == tableData)
        return;

    d->m_tableData = tableData;
    if (tableData) {
        d->m_stateTable = reinterpret_cast<const QScxmlExecutableContent::StateTable *>(
                    tableData->stateMachineTable());
        if (objectName().isEmpty()) {
            setObjectName(tableData->name());
        }
        if (d->m_stateTable->maxServiceId != QScxmlExecutableContent::StateTable::InvalidIndex) {
            const size_t serviceCount = size_t(d->m_stateTable->maxServiceId + 1);
            d->m_invokedServices.resize(serviceCount, { -1, nullptr, QString() });
            d->m_cachedFactories.resize(serviceCount, nullptr);
        }

        if (d->m_stateTable->version != Q_QSCXMLC_OUTPUT_REVISION) {
           qFatal("Cannot mix incompatible state table (version 0x%x) with this library "
                  "(version 0x%x)", d->m_stateTable->version, Q_QSCXMLC_OUTPUT_REVISION);
        }
        Q_ASSERT(tableData->stateMachineTable()[d->m_stateTable->arrayOffset +
                                                d->m_stateTable->arraySize]
                == QScxmlExecutableContent::StateTable::terminator);
    }

    emit tableDataChanged(tableData);
}

/*!
 * Retrieves a list of state names of all states.
 *
 * When \a compress is \c true (the default), the states that contain child states
 * will be filtered out and only the \e {leaf states} will be returned.
 * When it is \c false, the full list of all states will be returned.
 *
 * The returned list does not contain the states of possible nested state machines.
 *
 * \note The order of the state names in the list is the order in which the states occurred in
 *       the SCXML document.
 */
QStringList QScxmlStateMachine::stateNames(bool compress) const
{
    Q_D(const QScxmlStateMachine);

    QStringList names;
    for (int i = 0; i < d->m_stateTable->stateCount; ++i) {
        const auto &state = d->m_stateTable->state(i);
        if (!compress || state.isAtomic())
            names.append(d->m_tableData->string(state.name));
    }
    return names;
}

/*!
 * Retrieves a list of state names of all active states.
 *
 * When a state is active, all its parent states are active by definition. When \a compress
 * is \c true (the default), the parent states will be filtered out and only the \e {leaf states}
 * will be returned. When it is \c false, the full list of active states will be returned.
 */
QStringList QScxmlStateMachine::activeStateNames(bool compress) const
{
    Q_D(const QScxmlStateMachine);

    QStringList result;
    for (int stateIdx : d->m_configuration) {
        const auto &state = d->m_stateTable->state(stateIdx);
        if (state.isAtomic() || !compress)
            result.append(d->m_tableData->string(state.name));
    }
    return result;
}

/*!
 * Returns \c true if the state specified by \a scxmlStateName is active, \c false otherwise.
 */
bool QScxmlStateMachine::isActive(const QString &scxmlStateName) const
{
    Q_D(const QScxmlStateMachine);

    for (int stateIndex : d->m_configuration) {
        const auto &state = d->m_stateTable->state(stateIndex);
        if (d->m_tableData->string(state.name) == scxmlStateName)
            return true;
    }

    return false;
}

QMetaObject::Connection QScxmlStateMachine::connectToStateImpl(const QString &scxmlStateName,
                                                               const QObject *receiver, void **slot,
                                                               QtPrivate::QSlotObjectBase *slotObj,
                                                               Qt::ConnectionType type)
{
    const int *types = Q_NULLPTR;
    if (type == Qt::QueuedConnection || type == Qt::BlockingQueuedConnection)
        types = QtPrivate::ConnectionTypes<QtPrivate::List<bool> >::types();

    Q_D(QScxmlStateMachine);
    int signalIndex = QScxmlInternal::signalIndex(d->m_metaObject,
                                                  scxmlStateName.toUtf8() + "Changed(bool)");
    return signalIndex < 0 ? QMetaObject::Connection()
                           : QObjectPrivate::connectImpl(this, signalIndex, receiver, slot, slotObj,
                                                         type, types, d->m_metaObject);
}

/*!
    Creates a connection of the given \a type from the state identified by \a scxmlStateName
    to the \a method in the \a receiver object. The receiver's \a method
    may take a boolean argument that indicates whether the state connected
    became active or inactive. For example:

    \code
    void mySlot(bool active);
    \endcode

    Returns a handle to the connection, which can be used later to disconnect.
 */
QMetaObject::Connection QScxmlStateMachine::connectToState(const QString &scxmlStateName,
                                                           const QObject *receiver,
                                                           const char *method,
                                                           Qt::ConnectionType type)
{
    QByteArray signalName = QByteArray::number(QSIGNAL_CODE) + scxmlStateName.toUtf8()
            + "Changed(bool)";
    return QObject::connect(this, signalName.constData(), receiver, method, type);
}

/*!
    Creates a connection of the specified \a type from the event specified by
    \a scxmlEventSpec to the \a method in the \a receiver object. The receiver's
    \a method may take a QScxmlEvent as a parameter. For example:

    \code
    void mySlot(const QScxmlEvent &event);
    \endcode

    In contrast to event specifications in SCXML documents, spaces are not
    allowed in the \a scxmlEventSpec here. In order to connect to multiple
    events with different prefixes, connectToEvent() has to be called multiple
    times.

    Returns a handle to the connection, which can be used later to disconnect.
*/
QMetaObject::Connection QScxmlStateMachine::connectToEvent(const QString &scxmlEventSpec,
                                                           const QObject *receiver,
                                                           const char *method,
                                                           Qt::ConnectionType type)
{
    Q_D(QScxmlStateMachine);
    return d->m_router.connectToEvent(scxmlEventSpec.split(QLatin1Char('.')), receiver, method,
                                      type);
}

QMetaObject::Connection QScxmlStateMachine::connectToEventImpl(const QString &scxmlEventSpec,
                                                               const QObject *receiver, void **slot,
                                                               QtPrivate::QSlotObjectBase *slotObj,
                                                               Qt::ConnectionType type)
{
    Q_D(QScxmlStateMachine);
    return d->m_router.connectToEvent(scxmlEventSpec.split(QLatin1Char('.')), receiver, slot,
                                      slotObj, type);
}

/*!
 * Initializes the state machine.
 *
 * State machine initialization consists of calling QScxmlDataModel::setup(), setting the initial
 * values for \c <data> elements, and executing any \c <script> tags of the \c <scxml> tag. The
 * initial data values are taken from the \c initialValues property.
 *
 * Returns \c false if parse errors occur or if any of the initialization steps fail.
 * Returns \c true otherwise.
 */
bool QScxmlStateMachine::init()
{
    Q_D(QScxmlStateMachine);

    if (d->m_isInitialized)
        return false;

    if (!parseErrors().isEmpty())
        return false;

    if (!dataModel() || !dataModel()->setup(d->m_initialValues))
        return false;

    if (!d->executeInitialSetup())
        return false;

    d->m_isInitialized = true;
    emit initializedChanged(true);
    return true;
}

/*!
 * Returns \c true if the state machine is running, \c false otherwise.
 *
 * \sa setRunning(), runningChanged()
 */
bool QScxmlStateMachine::isRunning() const
{
    Q_D(const QScxmlStateMachine);

    return d->isRunnable() && !d->isPaused();
}

/*!
 * Starts the state machine if \a running is \c true, or stops it otherwise.
 *
 * \sa start(), stop(), isRunning(), runningChanged()
 */
void QScxmlStateMachine::setRunning(bool running)
{
    if (running)
        start();
    else
        stop();
}

QVariantMap QScxmlStateMachine::initialValues()
{
    Q_D(const QScxmlStateMachine);
    return d->m_initialValues;
}

void QScxmlStateMachine::setInitialValues(const QVariantMap &initialValues)
{
    Q_D(QScxmlStateMachine);
    if (initialValues != d->m_initialValues) {
        d->m_initialValues = initialValues;
        emit initialValuesChanged(initialValues);
    }
}

QString QScxmlStateMachine::name() const
{
    return tableData()->name();
}

/*!
 * Submits the SCXML event \a event to the internal or external event queue depending on the
 * priority of the event.
 *
 * When a delay is set, the event will be queued for delivery after the timeout has passed.
 */
void QScxmlStateMachine::submitEvent(QScxmlEvent *event)
{
    Q_D(QScxmlStateMachine);

    if (!event)
        return;

    if (event->delay() > 0) {
        qCDebug(qscxmlLog) << this << "submitting event" << event->name()
                           << "with delay" << event->delay() << "ms:"
                           << QScxmlEventPrivate::debugString(event).constData();

        Q_ASSERT(event->eventType() == QScxmlEvent::ExternalEvent);
        d->submitDelayedEvent(event);
    } else {
        qCDebug(qscxmlLog) << this << "submitting event" << event->name()
                           << ":" << QScxmlEventPrivate::debugString(event).constData();

        d->routeEvent(event);
    }
}

/*!
 * A utility method to create and submit an external event with the specified
 * \a eventName as the name.
 */
void QScxmlStateMachine::submitEvent(const QString &eventName)
{
    QScxmlEvent *e = new QScxmlEvent;
    e->setName(eventName);
    e->setEventType(QScxmlEvent::ExternalEvent);
    submitEvent(e);
}

/*!
 * A utility method to create and submit an external event with the specified
 * \a eventName as the name and \a data as the payload data.
 */
void QScxmlStateMachine::submitEvent(const QString &eventName, const QVariant &data)
{
    QVariant incomingData = data;
    if (incomingData.canConvert<QJSValue>())
        incomingData = incomingData.value<QJSValue>().toVariant();

    QScxmlEvent *e = new QScxmlEvent;
    e->setName(eventName);
    e->setEventType(QScxmlEvent::ExternalEvent);
    e->setData(incomingData);
    submitEvent(e);
}

/*!
 * Cancels a delayed event with the specified \a sendId.
 */
void QScxmlStateMachine::cancelDelayedEvent(const QString &sendId)
{
    Q_D(QScxmlStateMachine);

    for (auto it = d->m_delayedEvents.begin(), eit = d->m_delayedEvents.end(); it != eit; ++it) {
        if (it->second->sendId() == sendId) {
            qCDebug(qscxmlLog) << this
                               << "canceling event" << sendId
                               << "with timer id" << it->first;
            d->m_eventLoopHook.killTimer(it->first);
            delete it->second;
            d->m_delayedEvents.erase(it);
            return;
        }
    }
}

/*!
 * Returns \c true if a message to \a target can be dispatched by this state machine.
 *
 * Valid targets are:
 * \list
 * \li  \c #_parent for the parent state machine if the current state machine is started by
 *      \c <invoke>
 * \li  \c #_internal for the current state machine
 * \li  \c #_scxml_sessionid, where \c sessionid is the session ID of the current state machine
 * \li  \c #_servicename, where \c servicename is the ID or name of a service started with
 *      \c <invoke> by this state machine
 * \endlist
 */
bool QScxmlStateMachine::isDispatchableTarget(const QString &target) const
{
    Q_D(const QScxmlStateMachine);

    if (isInvoked() && target == QStringLiteral("#_parent"))
        return true; // parent state machine, if we're <invoke>d.
    if (target == QStringLiteral("#_internal")
            || target == QStringLiteral("#_scxml_%1").arg(sessionId()))
        return true; // that's the current state machine

    if (target.startsWith(QStringLiteral("#_"))) {
        QStringRef targetId = target.midRef(2);
        for (auto invokedService : d->m_invokedServices) {
            if (invokedService.service->id() == targetId)
                return true;
        }
    }

    return false;
}

/*!
    \property QScxmlStateMachine::invokedServices
    \brief A list of SCXML services that were invoked from the main
    state machine (possibly recursively).
*/

QVector<QScxmlInvokableService *> QScxmlStateMachine::invokedServices() const
{
    Q_D(const QScxmlStateMachine);

    QVector<QScxmlInvokableService *> result;
    for (int i = 0, ei = int(d->m_invokedServices.size()); i != ei; ++i) {
        if (auto service = d->m_invokedServices[size_t(i)].service)
            result.append(service);
    }
    return result;
}

/*!
  \fn QScxmlStateMachine::runningChanged(bool running)

  This signal is emitted when the \c running property is changed with \a running as argument.
*/

/*!
  \fn QScxmlStateMachine::log(const QString &label, const QString &msg)

  This signal is emitted if a \c <log> tag is used in the SCXML. \a label is the value of the
  \e label attribute of the \c <log> tag. \a msg is the value of the evaluated \e expr attribute
  of the \c <log> tag. If there is no \e expr attribute, a null string will be returned.
*/

/*!
  \fn QScxmlStateMachine::reachedStableState()

  This signal is emitted when the event queue is empty at the end of a macro step or when a final
  state is reached.
*/

/*!
  \fn QScxmlStateMachine::finished()

  This signal is emitted when the state machine reaches a top-level final state.

  \sa running
*/


/*!
  Starts this state machine. The machine will reset its configuration and
  transition to the initial state. When a final top-level state
  is entered, the machine will emit the finished() signal.

  \note A state machine will not run without a running event loop, such as
  the main application event loop started with QCoreApplication::exec() or
  QApplication::exec().

  \sa runningChanged(), setRunning(), stop(), finished()
*/
void QScxmlStateMachine::start()
{
    Q_D(QScxmlStateMachine);

    if (!parseErrors().isEmpty())
        return;

    // Failure to initialize doesn't prevent start(). See w3c-ecma/test487 in the scion test suite.
    if (!isInitialized() && !init())
        qCDebug(qscxmlLog) << this << "cannot be initialized on start(). Starting anyway ...";

    d->start();
    d->m_eventLoopHook.queueProcessEvents();
}

/*!
  Stops this state machine. The machine will not execute any further state
  transitions. Its \c running property is set to \c false.

  \sa runningChanged(), start(), setRunning()
 */
void QScxmlStateMachine::stop()
{
    Q_D(QScxmlStateMachine);
    d->pause();
}

/*!
  Returns \c true if the state with the ID \a stateIndex is active.

  This method is part of the interface to the compiled representation of SCXML
  state machines. It should only be used internally and by state machines
  compiled from SCXML documents.
 */
bool QScxmlStateMachine::isActive(int stateIndex) const
{
    Q_D(const QScxmlStateMachine);
    return d->m_configuration.contains(stateIndex);
}

QT_END_NAMESPACE
