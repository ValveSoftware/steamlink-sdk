/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquicktransitionmanager_p_p.h"

#include "qquicktransition_p.h"
#include "qquickstate_p_p.h"

#include <private/qqmlbinding_p.h>
#include <private/qqmlglobal_p.h>
#include <private/qqmlproperty_p.h>

#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

DEFINE_BOOL_CONFIG_OPTION(stateChangeDebug, STATECHANGE_DEBUG);

class QQuickTransitionManagerPrivate
{
public:
    QQuickTransitionManagerPrivate()
        : state(0), transitionInstance(0) {}

    void applyBindings();
    typedef QList<QQuickSimpleAction> SimpleActionList;
    QQuickState *state;
    QQuickTransitionInstance *transitionInstance;
    QQuickStateOperation::ActionList bindingsList;
    SimpleActionList completeList;
};

QQuickTransitionManager::QQuickTransitionManager()
: d(new QQuickTransitionManagerPrivate)
{
}

void QQuickTransitionManager::setState(QQuickState *s)
{
    d->state = s;
}

QQuickTransitionManager::~QQuickTransitionManager()
{
    delete d->transitionInstance;
    delete d; d = 0;
}

bool QQuickTransitionManager::isRunning() const
{
    return d->transitionInstance && d->transitionInstance->isRunning();
}

void QQuickTransitionManager::complete()
{
    d->applyBindings();

    for (int ii = 0; ii < d->completeList.count(); ++ii) {
        const QQmlProperty &prop = d->completeList.at(ii).property();
        prop.write(d->completeList.at(ii).value());
    }

    d->completeList.clear();

    if (d->state)
        static_cast<QQuickStatePrivate*>(QObjectPrivate::get(d->state))->complete();

    finished();
}

void QQuickTransitionManagerPrivate::applyBindings()
{
    foreach(const QQuickStateAction &action, bindingsList) {
        if (!action.toBinding.isNull()) {
            QQmlPropertyPrivate::setBinding(action.property, action.toBinding.data());
        } else if (action.event) {
            if (action.reverseEvent)
                action.event->reverse();
            else
                action.event->execute();
        }

    }

    bindingsList.clear();
}

void QQuickTransitionManager::finished()
{
}

void QQuickTransitionManager::transition(const QList<QQuickStateAction> &list,
                                      QQuickTransition *transition,
                                      QObject *defaultTarget)
{
    cancel();

    QQuickStateOperation::ActionList applyList = list;
    // Determine which actions are binding changes and disable any current bindings
    foreach(const QQuickStateAction &action, applyList) {
        if (action.toBinding)
            d->bindingsList << action;
        if (action.fromBinding)
            QQmlPropertyPrivate::setBinding(action.property, 0); // Disable current binding
        if (action.event && action.event->changesBindings()) {  //### assume isReversable()?
            d->bindingsList << action;
            action.event->clearBindings();
        }
    }

    // Animated transitions need both the start and the end value for
    // each property change.  In the presence of bindings, the end values
    // are non-trivial to calculate.  As a "best effort" attempt, we first
    // apply all the property and binding changes, then read all the actual
    // final values, then roll back the changes and proceed as normal.
    //
    // This doesn't catch everything, and it might be a little fragile in
    // some cases - but whatcha going to do?
    //
    // Note that we only fast forward if both a transition and bindings are
    // present, as it is unnecessary (and potentially expensive) otherwise.

    if (transition && !d->bindingsList.isEmpty()) {

        // Apply all the property and binding changes
        for (int ii = 0; ii < applyList.size(); ++ii) {
            const QQuickStateAction &action = applyList.at(ii);
            if (!action.toBinding.isNull()) {
                QQmlPropertyPrivate::setBinding(action.property, action.toBinding.data(), QQmlPropertyPrivate::BypassInterceptor | QQmlPropertyPrivate::DontRemoveBinding);
            } else if (!action.event) {
                QQmlPropertyPrivate::write(action.property, action.toValue, QQmlPropertyPrivate::BypassInterceptor | QQmlPropertyPrivate::DontRemoveBinding);
            } else if (action.event->isReversable()) {
                if (action.reverseEvent)
                    action.event->reverse(QQuickStateActionEvent::FastForward);
                else
                    action.event->execute(QQuickStateActionEvent::FastForward);
            }
        }

        // Read all the end values for binding changes
        for (int ii = 0; ii < applyList.size(); ++ii) {
            QQuickStateAction *action = &applyList[ii];
            if (action->event) {
                action->event->saveTargetValues();
                continue;
            }
            const QQmlProperty &prop = action->property;
            if (!action->toBinding.isNull() || !action->toValue.isValid()) {
                action->toValue = prop.read();
            }
        }

        // Revert back to the original values
        foreach(const QQuickStateAction &action, applyList) {
            if (action.event) {
                if (action.event->isReversable()) {
                    action.event->clearBindings();
                    action.event->rewind();
                    action.event->clearBindings();  //### shouldn't be needed
                }
                continue;
            }

            if (action.toBinding)
                QQmlPropertyPrivate::setBinding(action.property, 0); // Make sure this is disabled during the transition

            QQmlPropertyPrivate::write(action.property, action.fromValue, QQmlPropertyPrivate::BypassInterceptor | QQmlPropertyPrivate::DontRemoveBinding);
        }
    }

    if (transition) {
        QList<QQmlProperty> touched;
        QQuickTransitionInstance *oldInstance = d->transitionInstance;
        d->transitionInstance = transition->prepare(applyList, touched, this, defaultTarget);
        d->transitionInstance->start();
        if (oldInstance && oldInstance != d->transitionInstance)
            delete oldInstance;

        // Modify the action list to remove actions handled in the transition
        for (int ii = 0; ii < applyList.count(); ++ii) {
            const QQuickStateAction &action = applyList.at(ii);

            if (action.event) {

                if (action.actionDone) {
                    applyList.removeAt(ii);
                    --ii;
                }

            } else {

                if (touched.contains(action.property)) {
                    if (action.toValue != action.fromValue)
                        d->completeList <<
                            QQuickSimpleAction(action, QQuickSimpleAction::EndState);

                    applyList.removeAt(ii);
                    --ii;
                }

            }
        }
    }

    // Any actions remaining have not been handled by the transition and should
    // be applied immediately.  We skip applying bindings, as they are all
    // applied at the end in applyBindings() to avoid any nastiness mid
    // transition
    foreach(const QQuickStateAction &action, applyList) {
        if (action.event && !action.event->changesBindings()) {
            if (action.event->isReversable() && action.reverseEvent)
                action.event->reverse();
            else
                action.event->execute();
        } else if (!action.event && !action.toBinding) {
            action.property.write(action.toValue);
        }
    }
#ifndef QT_NO_DEBUG_STREAM
    if (stateChangeDebug()) {
        foreach(const QQuickStateAction &action, applyList) {
            if (action.event)
                qWarning() << "    No transition for event:" << action.event->type();
            else
                qWarning() << "    No transition for:" << action.property.object()
                           << action.property.name() << "From:" << action.fromValue
                           << "To:" << action.toValue;
        }
    }
#endif
    if (!transition)
        complete();
}

void QQuickTransitionManager::cancel()
{
    if (d->transitionInstance && d->transitionInstance->isRunning())
        d->transitionInstance->stop();

    for(int i = 0; i < d->bindingsList.count(); ++i) {
        QQuickStateAction action = d->bindingsList[i];
        if (!action.toBinding.isNull() && action.deletableToBinding) {
            QQmlPropertyPrivate::setBinding(action.property, 0);
            action.toBinding.data()->destroy();
            action.toBinding.clear();
            action.deletableToBinding = false;
        } else if (action.event) {
            //### what do we do here?
        }

    }
    d->bindingsList.clear();
    d->completeList.clear();
}

QT_END_NAMESPACE
