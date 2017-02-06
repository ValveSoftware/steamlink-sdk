/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtScxml module of the Qt Toolkit.
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

#include <QtTest>
#include <QtScxml/qscxmlstatemachine.h>
#include <QtScxml/private/qscxmlstatemachineinfo_p.h>

Q_DECLARE_METATYPE(QScxmlError);

class tst_StateMachineInfo: public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void checkInfo();
};

class Recorder: public QObject
{
    Q_OBJECT

public:
    void clear()
    {
        enterCount = 0;
        entered.clear();
        exitCount = 0;
        exited.clear();
        transitionTriggerCount = 0;
        transitions.clear();
        macroStepDone = false;
    }

public slots:
    void statesEntered(const QVector<QScxmlStateMachineInfo::StateId> &states)
    { entered = states; ++enterCount; }

    void statesExited(const QVector<QScxmlStateMachineInfo::StateId> &states)
    { exited = states; ++exitCount; }

    void transitionsTriggered(const QVector<QScxmlStateMachineInfo::TransitionId> &transitions)
    { this->transitions = transitions; ++transitionTriggerCount; }

    void reachedStableState()
    { macroStepDone = true; }

    bool finishMacroStep() const
    {
        for (int i = 0; i < 100; ++i) {
            if (!macroStepDone)
                QCoreApplication::processEvents();
        }

        return macroStepDone;
    }

public:
    int enterCount = 0;
    QVector<QScxmlStateMachineInfo::StateId> entered;
    int exitCount = 0;
    QVector<QScxmlStateMachineInfo::StateId> exited;
    int transitionTriggerCount = 0;
    QVector<QScxmlStateMachineInfo::TransitionId> transitions;
    bool macroStepDone = false;
};

void tst_StateMachineInfo::checkInfo()
{
    QScopedPointer<QScxmlStateMachine> stateMachine(
                QScxmlStateMachine::fromFile(QString(":/tst_statemachineinfo/statemachine.scxml")));
    QVERIFY(!stateMachine.isNull());
    QVERIFY(stateMachine->parseErrors().isEmpty());
    auto info = new QScxmlStateMachineInfo(stateMachine.data());

    const QString machineName = QLatin1String("InfoTest");
    QCOMPARE(stateMachine->name(), machineName);

    auto states = info->allStates();
    QCOMPARE(states.size(), 5);
    QCOMPARE(info->stateName(states.at(0)), QLatin1String(""));
    QCOMPARE(info->stateName(states.at(1)), QLatin1String("next"));
    QCOMPARE(info->stateName(states.at(2)), QLatin1String("a"));
    QCOMPARE(info->stateName(states.at(3)), QLatin1String("b"));
    QCOMPARE(info->stateName(states.at(4)), QLatin1String("theEnd"));

    QCOMPARE(info->stateParent(QScxmlStateMachineInfo::InvalidState),
             static_cast<int>(QScxmlStateMachineInfo::InvalidStateId));
    QCOMPARE(info->stateParent(states.at(0)), static_cast<int>(QScxmlStateMachineInfo::InvalidStateId));
    QCOMPARE(info->stateParent(states.at(1)), static_cast<int>(QScxmlStateMachineInfo::InvalidStateId));
    QCOMPARE(info->stateParent(states.at(2)), 1);
    QCOMPARE(info->stateParent(states.at(3)), 1);
    QCOMPARE(info->stateParent(states.at(4)), static_cast<int>(QScxmlStateMachineInfo::InvalidStateId));

    QCOMPARE(info->stateType(states.at(0)), QScxmlStateMachineInfo::NormalState);
    QCOMPARE(info->stateType(states.at(1)), QScxmlStateMachineInfo::ParallelState);
    QCOMPARE(info->stateType(states.at(2)), QScxmlStateMachineInfo::NormalState);
    QCOMPARE(info->stateType(states.at(3)), QScxmlStateMachineInfo::NormalState);
    QCOMPARE(info->stateType(states.at(4)), QScxmlStateMachineInfo::FinalState);

    QCOMPARE(info->stateChildren(QScxmlStateMachineInfo::InvalidStateId),
             QVector<int>() << 0 << 1 << 4);
    QCOMPARE(info->stateChildren(states.at(0)), QVector<int>());
    QCOMPARE(info->stateChildren(states.at(1)), QVector<int>() << 2 << 3);
    QCOMPARE(info->stateChildren(states.at(2)), QVector<int>());
    QCOMPARE(info->stateChildren(states.at(3)), QVector<int>());
    QCOMPARE(info->stateChildren(states.at(4)), QVector<int>());

    QCOMPARE(info->initialTransition(QScxmlStateMachineInfo::InvalidStateId), 4);
    QCOMPARE(info->initialTransition(states.at(0)), static_cast<int>(QScxmlStateMachineInfo::InvalidTransitionId));
    QCOMPARE(info->initialTransition(states.at(1)), 5);
    QCOMPARE(info->initialTransition(states.at(2)), static_cast<int>(QScxmlStateMachineInfo::InvalidTransitionId));
    QCOMPARE(info->initialTransition(states.at(3)), static_cast<int>(QScxmlStateMachineInfo::InvalidTransitionId));
    QCOMPARE(info->initialTransition(states.at(4)), static_cast<int>(QScxmlStateMachineInfo::InvalidTransitionId));

    auto transitions = info->allTransitions();
    QCOMPARE(transitions.size(), 6);

    // targetless transition on top level
    QCOMPARE(info->transitionType(transitions.at(0)), QScxmlStateMachineInfo::ExternalTransition);
    QCOMPARE(info->stateType(info->transitionSource(transitions.at(0))),
             QScxmlStateMachineInfo::InvalidState);
    QCOMPARE(info->transitionTargets(transitions.at(0)).size(), 0);
    QCOMPARE(info->transitionEvents(transitions.at(0)).size(), 0);

    // <anon>->next
    QCOMPARE(info->transitionType(transitions.at(1)), QScxmlStateMachineInfo::ExternalTransition);
    QCOMPARE(info->transitionSource(transitions.at(1)), states.at(0));
    QCOMPARE(info->transitionTargets(transitions.at(1)).size(), 1);
    QCOMPARE(info->transitionTargets(transitions.at(1)).at(0), states.at(1));
    QCOMPARE(info->transitionEvents(transitions.at(1)).size(), 1);
    QCOMPARE(info->transitionEvents(transitions.at(1)).at(0), QStringLiteral("step"));

    // a->theEnd
    QCOMPARE(info->transitionType(transitions.at(2)), QScxmlStateMachineInfo::ExternalTransition);
    QCOMPARE(info->transitionSource(transitions.at(2)), states.at(2));
    QCOMPARE(info->transitionTargets(transitions.at(2)).size(), 1);
    QCOMPARE(info->transitionTargets(transitions.at(2)).at(0), states.at(4));
    QCOMPARE(info->transitionEvents(transitions.at(2)).size(), 1);
    QCOMPARE(info->transitionEvents(transitions.at(2)).at(0), QStringLiteral("step"));

    // b->theEnd
    QCOMPARE(info->transitionType(transitions.at(3)), QScxmlStateMachineInfo::InternalTransition);
    QCOMPARE(info->transitionSource(transitions.at(3)), states.at(3));
    QCOMPARE(info->transitionTargets(transitions.at(3)).size(), 1);
    QCOMPARE(info->transitionTargets(transitions.at(3)).at(0), states.at(4));
    QCOMPARE(info->transitionEvents(transitions.at(3)).size(), 1);
    QCOMPARE(info->transitionEvents(transitions.at(3)).at(0), QStringLiteral("step"));

    // initial transition that activates the first (anonymous) state
    QCOMPARE(info->transitionType(transitions.at(4)), QScxmlStateMachineInfo::SyntheticTransition);
    QCOMPARE(info->stateType(info->transitionSource(transitions.at(4))),
             QScxmlStateMachineInfo::InvalidState);
    QCOMPARE(info->transitionTargets(transitions.at(4)).size(), 1);
    QCOMPARE(info->transitionTargets(transitions.at(4)).at(0), states.at(0));
    QCOMPARE(info->transitionEvents(transitions.at(4)).size(), 0);

    // "initial" transition in the next state that activates all sub-states
    QCOMPARE(info->transitionType(transitions.at(5)), QScxmlStateMachineInfo::SyntheticTransition);
    QCOMPARE(info->transitionSource(transitions.at(5)), states.at(1));
    QCOMPARE(info->transitionTargets(transitions.at(5)).size(), 2);
    QCOMPARE(info->transitionTargets(transitions.at(5)).at(0), states.at(2));
    QCOMPARE(info->transitionTargets(transitions.at(5)).at(1), states.at(3));
    QCOMPARE(info->transitionEvents(transitions.at(5)).size(), 0);

    Recorder recorder;
    QObject::connect(info, &QScxmlStateMachineInfo::statesEntered,
                     &recorder, &Recorder::statesEntered);
    QObject::connect(info, &QScxmlStateMachineInfo::statesExited,
                     &recorder, &Recorder::statesExited);
    QObject::connect(info, &QScxmlStateMachineInfo::transitionsTriggered,
                     &recorder, &Recorder::transitionsTriggered);
    QObject::connect(stateMachine.data(), &QScxmlStateMachine::reachedStableState,
                     &recorder, &Recorder::reachedStableState);

    // initial step into first anonymous state
    stateMachine->start();
    QVERIFY(recorder.finishMacroStep());
    QCOMPARE(recorder.enterCount, 1);
    QCOMPARE(recorder.entered, QVector<QScxmlStateMachineInfo::StateId>() << 0);
    QVERIFY(recorder.exited.isEmpty());

    recorder.clear();

    // step from anonymous state into the parallel state, which activates "a" and "b" (in THAT
    // order!)
    stateMachine->submitEvent("step");
    QVERIFY(recorder.finishMacroStep());
    QCOMPARE(recorder.enterCount, 1);
    QCOMPARE(recorder.entered, QVector<QScxmlStateMachineInfo::StateId>() << 1 << 2 << 3);
    QCOMPARE(recorder.exited, QVector<QScxmlStateMachineInfo::StateId>() << 0);
    QCOMPARE(recorder.transitionTriggerCount, 1);
    QCOMPARE(recorder.transitions, QVector<QScxmlStateMachineInfo::TransitionId>() << 1);

    recorder.clear();

    // step from the state "b" into "theEnd", which exits "b", "a", and "next" in exactly that
    // order
    stateMachine->submitEvent("step");
    QVERIFY(recorder.finishMacroStep());
    QCOMPARE(recorder.enterCount, 1);
    QCOMPARE(recorder.entered, QVector<QScxmlStateMachineInfo::StateId>() << 4);
    QCOMPARE(recorder.exited, QVector<QScxmlStateMachineInfo::StateId>() << 3 << 2 << 1);
    QCOMPARE(recorder.transitionTriggerCount, 1);
    QCOMPARE(recorder.transitions, QVector<QScxmlStateMachineInfo::TransitionId>() << 2);
}


QTEST_MAIN(tst_StateMachineInfo)

#include "tst_statemachineinfo.moc"


