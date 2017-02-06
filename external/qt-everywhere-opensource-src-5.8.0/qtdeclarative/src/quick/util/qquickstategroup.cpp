/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qquickstategroup_p.h"

#include "qquicktransition_p.h"

#include <private/qqmlbinding_p.h>
#include <private/qqmlglobal_p.h>

#include <QtCore/qstringlist.h>
#include <QtCore/qdebug.h>
#include <QtCore/qvector.h>

#include <private/qobject_p.h>
#include <qqmlinfo.h>

QT_BEGIN_NAMESPACE

DEFINE_BOOL_CONFIG_OPTION(stateChangeDebug, STATECHANGE_DEBUG);

class QQuickStateGroupPrivate : public QObjectPrivate
{
    Q_DECLARE_PUBLIC(QQuickStateGroup)
public:
    QQuickStateGroupPrivate()
    : nullState(0), componentComplete(true),
      ignoreTrans(false), applyingState(false), unnamedCount(0) {}

    QString currentState;
    QQuickState *nullState;

    static void append_state(QQmlListProperty<QQuickState> *list, QQuickState *state);
    static int count_state(QQmlListProperty<QQuickState> *list);
    static QQuickState *at_state(QQmlListProperty<QQuickState> *list, int index);
    static void clear_states(QQmlListProperty<QQuickState> *list);

    static void append_transition(QQmlListProperty<QQuickTransition> *list, QQuickTransition *state);
    static int count_transitions(QQmlListProperty<QQuickTransition> *list);
    static QQuickTransition *at_transition(QQmlListProperty<QQuickTransition> *list, int index);
    static void clear_transitions(QQmlListProperty<QQuickTransition> *list);

    QList<QQuickState *> states;
    QList<QQuickTransition *> transitions;

    bool componentComplete;
    bool ignoreTrans;
    bool applyingState;
    int unnamedCount;

    QQuickTransition *findTransition(const QString &from, const QString &to);
    void setCurrentStateInternal(const QString &state, bool = false);
    bool updateAutoState();
};

/*!
   \qmltype StateGroup
    \instantiates QQuickStateGroup
    \inqmlmodule QtQuick
   \ingroup qtquick-states
   \brief Provides built-in state support for non-Item types

   Item (and all derived types) provides built in support for states and transitions
   via its \l{Item::state}{state}, \l{Item::states}{states} and \l{Item::transitions}{transitions} properties. StateGroup provides an easy way to
   use this support in other (non-Item-derived) types.

   \qml
   MyCustomObject {
       StateGroup {
           id: myStateGroup
           states: State {
               name: "state1"
               // ...
           }
           transitions: Transition {
               // ...
           }
       }

       onSomethingHappened: myStateGroup.state = "state1";
   }
   \endqml

   \sa {Qt Quick States}{Qt Quick States}, {Animation and Transitions in Qt Quick}{Transitions}, {Qt QML}
*/

QQuickStateGroup::QQuickStateGroup(QObject *parent)
    : QObject(*(new QQuickStateGroupPrivate), parent)
{
}

QQuickStateGroup::~QQuickStateGroup()
{
    Q_D(const QQuickStateGroup);
    for (int i = 0; i < d->states.count(); ++i)
        d->states.at(i)->setStateGroup(0);
}

QList<QQuickState *> QQuickStateGroup::states() const
{
    Q_D(const QQuickStateGroup);
    return d->states;
}

/*!
  \qmlproperty list<State> QtQuick::StateGroup::states
  This property holds a list of states defined by the state group.

  \qml
  StateGroup {
      states: [
          State {
              // State definition...
          },
          State {
              // ...
          }
          // Other states...
      ]
  }
  \endqml

  \sa {Qt Quick States}{Qt Quick States}
*/
QQmlListProperty<QQuickState> QQuickStateGroup::statesProperty()
{
    Q_D(QQuickStateGroup);
    return QQmlListProperty<QQuickState>(this, &d->states, &QQuickStateGroupPrivate::append_state,
                                                       &QQuickStateGroupPrivate::count_state,
                                                       &QQuickStateGroupPrivate::at_state,
                                                       &QQuickStateGroupPrivate::clear_states);
}

void QQuickStateGroupPrivate::append_state(QQmlListProperty<QQuickState> *list, QQuickState *state)
{
    QQuickStateGroup *_this = static_cast<QQuickStateGroup *>(list->object);
    if (state) {
        _this->d_func()->states.append(state);
        state->setStateGroup(_this);
    }

}

int QQuickStateGroupPrivate::count_state(QQmlListProperty<QQuickState> *list)
{
    QQuickStateGroup *_this = static_cast<QQuickStateGroup *>(list->object);
    return _this->d_func()->states.count();
}

QQuickState *QQuickStateGroupPrivate::at_state(QQmlListProperty<QQuickState> *list, int index)
{
    QQuickStateGroup *_this = static_cast<QQuickStateGroup *>(list->object);
    return _this->d_func()->states.at(index);
}

void QQuickStateGroupPrivate::clear_states(QQmlListProperty<QQuickState> *list)
{
    QQuickStateGroup *_this = static_cast<QQuickStateGroup *>(list->object);
    _this->d_func()->setCurrentStateInternal(QString(), true);
    for (int i = 0; i < _this->d_func()->states.count(); ++i) {
        _this->d_func()->states.at(i)->setStateGroup(0);
    }
    _this->d_func()->states.clear();
}

/*!
  \qmlproperty list<Transition> QtQuick::StateGroup::transitions
  This property holds a list of transitions defined by the state group.

  \qml
  StateGroup {
      transitions: [
          Transition {
            // ...
          },
          Transition {
            // ...
          }
          // ...
      ]
  }
  \endqml

  \sa {Animation and Transitions in Qt Quick}{Transitions}
*/
QQmlListProperty<QQuickTransition> QQuickStateGroup::transitionsProperty()
{
    Q_D(QQuickStateGroup);
    return QQmlListProperty<QQuickTransition>(this, &d->transitions, &QQuickStateGroupPrivate::append_transition,
                                                       &QQuickStateGroupPrivate::count_transitions,
                                                       &QQuickStateGroupPrivate::at_transition,
                                                       &QQuickStateGroupPrivate::clear_transitions);
}

void QQuickStateGroupPrivate::append_transition(QQmlListProperty<QQuickTransition> *list, QQuickTransition *trans)
{
    QQuickStateGroup *_this = static_cast<QQuickStateGroup *>(list->object);
    if (trans)
        _this->d_func()->transitions.append(trans);
}

int QQuickStateGroupPrivate::count_transitions(QQmlListProperty<QQuickTransition> *list)
{
    QQuickStateGroup *_this = static_cast<QQuickStateGroup *>(list->object);
    return _this->d_func()->transitions.count();
}

QQuickTransition *QQuickStateGroupPrivate::at_transition(QQmlListProperty<QQuickTransition> *list, int index)
{
    QQuickStateGroup *_this = static_cast<QQuickStateGroup *>(list->object);
    return _this->d_func()->transitions.at(index);
}

void QQuickStateGroupPrivate::clear_transitions(QQmlListProperty<QQuickTransition> *list)
{
    QQuickStateGroup *_this = static_cast<QQuickStateGroup *>(list->object);
    _this->d_func()->transitions.clear();
}

/*!
  \qmlproperty string QtQuick::StateGroup::state

  This property holds the name of the current state of the state group.

  This property is often used in scripts to change between states. For
  example:

  \js
  function toggle() {
      if (button.state == 'On')
          button.state = 'Off';
      else
          button.state = 'On';
  }
  \endjs

  If the state group is in its base state (i.e. no explicit state has been
  set), \c state will be a blank string. Likewise, you can return a
  state group to its base state by setting its current state to \c ''.

  \sa {Qt Quick States}{Qt Quick States}
*/
QString QQuickStateGroup::state() const
{
    Q_D(const QQuickStateGroup);
    return d->currentState;
}

void QQuickStateGroup::setState(const QString &state)
{
    Q_D(QQuickStateGroup);
    if (d->currentState == state)
        return;

    d->setCurrentStateInternal(state);
}

void QQuickStateGroup::classBegin()
{
    Q_D(QQuickStateGroup);
    d->componentComplete = false;
}

void QQuickStateGroup::componentComplete()
{
    Q_D(QQuickStateGroup);
    d->componentComplete = true;

    for (int ii = 0; ii < d->states.count(); ++ii) {
        QQuickState *state = d->states.at(ii);
        if (!state->isNamed())
            state->setName(QLatin1String("anonymousState") + QString::number(++d->unnamedCount));
    }

    if (d->updateAutoState()) {
        return;
    } else if (!d->currentState.isEmpty()) {
        QString cs = d->currentState;
        d->currentState.clear();
        d->setCurrentStateInternal(cs, true);
    }
}

/*!
    Returns true if the state was changed, otherwise false.
*/
bool QQuickStateGroup::updateAutoState()
{
    Q_D(QQuickStateGroup);
    return d->updateAutoState();
}

bool QQuickStateGroupPrivate::updateAutoState()
{
    Q_Q(QQuickStateGroup);
    if (!componentComplete)
        return false;

    bool revert = false;
    for (int ii = 0; ii < states.count(); ++ii) {
        QQuickState *state = states.at(ii);
        if (state->isWhenKnown()) {
            if (state->isNamed()) {
                if (state->when() && state->when()->evaluate().toBool()) {
                    if (stateChangeDebug())
                        qWarning() << "Setting auto state due to:"
                                   << state->when()->expression();
                    if (currentState != state->name()) {
                        q->setState(state->name());
                        return true;
                    } else {
                        return false;
                    }
                } else if (state->name() == currentState) {
                    revert = true;
                }
            }
        }
    }
    if (revert) {
        bool rv = !currentState.isEmpty();
        q->setState(QString());
        return rv;
    } else {
        return false;
    }
}

QQuickTransition *QQuickStateGroupPrivate::findTransition(const QString &from, const QString &to)
{
    QQuickTransition *highest = 0;
    int score = 0;
    bool reversed = false;
    bool done = false;

    for (int ii = 0; !done && ii < transitions.count(); ++ii) {
        QQuickTransition *t = transitions.at(ii);
        if (!t->enabled())
            continue;
        for (int ii = 0; ii < 2; ++ii)
        {
            if (ii && (!t->reversible() ||
                      (t->fromState() == QLatin1String("*") &&
                       t->toState() == QLatin1String("*"))))
                break;
            const QString fromStateStr = t->fromState();
            const QString toStateStr = t->toState();

            QVector<QStringRef> fromState = fromStateStr.splitRef(QLatin1Char(','));
            for (int jj = 0; jj < fromState.count(); ++jj)
                fromState[jj] = fromState.at(jj).trimmed();
            QVector<QStringRef> toState = toStateStr.splitRef(QLatin1Char(','));
            for (int jj = 0; jj < toState.count(); ++jj)
                toState[jj] = toState.at(jj).trimmed();
            if (ii == 1)
                qSwap(fromState, toState);
            int tScore = 0;
            const QString asterisk = QStringLiteral("*");
            if (fromState.contains(QStringRef(&from)))
                tScore += 2;
            else if (fromState.contains(QStringRef(&asterisk)))
                tScore += 1;
            else
                continue;

            if (toState.contains(QStringRef(&to)))
                tScore += 2;
            else if (toState.contains(QStringRef(&asterisk)))
                tScore += 1;
            else
                continue;

            if (ii == 1)
                reversed = true;
            else
                reversed = false;

            if (tScore == 4) {
                highest = t;
                done = true;
                break;
            } else if (tScore > score) {
                score = tScore;
                highest = t;
            }
        }
    }

    if (highest)
        highest->setReversed(reversed);

    return highest;
}

void QQuickStateGroupPrivate::setCurrentStateInternal(const QString &state,
                                                   bool ignoreTrans)
{
    Q_Q(QQuickStateGroup);
    if (!componentComplete) {
        currentState = state;
        return;
    }

    if (applyingState) {
        qmlInfo(q) << "Can't apply a state change as part of a state definition.";
        return;
    }

    applyingState = true;

    QQuickTransition *transition = ignoreTrans ? 0 : findTransition(currentState, state);
    if (stateChangeDebug()) {
        qWarning() << this << "Changing state.  From" << currentState << ". To" << state;
        if (transition)
            qWarning() << "   using transition" << transition->fromState()
                       << transition->toState();
    }

    QQuickState *oldState = 0;
    if (!currentState.isEmpty()) {
        for (int ii = 0; ii < states.count(); ++ii) {
            if (states.at(ii)->name() == currentState) {
                oldState = states.at(ii);
                break;
            }
        }
    }

    currentState = state;
    emit q->stateChanged(currentState);

    QQuickState *newState = 0;
    for (int ii = 0; ii < states.count(); ++ii) {
        if (states.at(ii)->name() == currentState) {
            newState = states.at(ii);
            break;
        }
    }

    if (oldState == 0 || newState == 0) {
        if (!nullState) {
            nullState = new QQuickState;
            QQml_setParent_noEvent(nullState, q);
            nullState->setStateGroup(q);
        }
        if (!oldState) oldState = nullState;
        if (!newState) newState = nullState;
    }

    newState->apply(transition, oldState);
    applyingState = false;  //### consider removing this (don't allow state changes in transition)
}

QQuickState *QQuickStateGroup::findState(const QString &name) const
{
    Q_D(const QQuickStateGroup);
    for (int i = 0; i < d->states.count(); ++i) {
        QQuickState *state = d->states.at(i);
        if (state->name() == name)
            return state;
    }

    return 0;
}

void QQuickStateGroup::removeState(QQuickState *state)
{
    Q_D(QQuickStateGroup);
    d->states.removeOne(state);
}

void QQuickStateGroup::stateAboutToComplete()
{
    Q_D(QQuickStateGroup);
    d->applyingState = false;
}

QT_END_NAMESPACE


