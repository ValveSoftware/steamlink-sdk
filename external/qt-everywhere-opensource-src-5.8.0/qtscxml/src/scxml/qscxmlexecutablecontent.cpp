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

#include "qscxmlglobals_p.h"
#include "qscxmlexecutablecontent_p.h"
#include "qscxmlcompiler_p.h"
#include "qscxmlevent_p.h"

QT_BEGIN_NAMESPACE

using namespace QScxmlExecutableContent;

/*!
    \namespace QScxmlExecutableContent
    \inmodule QtScxml
    \since 5.8
    \brief The QScxmlExecutableContent namespace contains various types used
    to interpret executable content in state machines.
 */

/*!
    \typedef QScxmlExecutableContent::ContainerId
    \inmodule QtScxml
    \since 5.8
    \brief ID for a container holding executable content.
 */

/*!
    \typedef QScxmlExecutableContent::EvaluatorId
    \inmodule QtScxml
    \since 5.8
    \brief ID for a unit of executable content.
 */

/*!
    \typedef QScxmlExecutableContent::InstructionId
    \inmodule QtScxml
    \since 5.8
    \brief ID for an instruction of executable content.
 */

/*!
    \typedef QScxmlExecutableContent::StringId
    \inmodule QtScxml
    \since 5.8
    \brief ID for a string contained in executable content.
 */

/*!
    \class QScxmlExecutableContent::EvaluatorInfo
    \brief The EvaluatorInfo class represents a unit of executable content.
    \since 5.8
    \inmodule QtScxml
 */

/*!
    \variable QScxmlExecutableContent::EvaluatorInfo::expr
    \brief The expression to be evaluated
 */

/*!
    \variable QScxmlExecutableContent::EvaluatorInfo::context
    \brief The context for evaluating the expression
 */

/*!
    \class QScxmlExecutableContent::AssignmentInfo
    \brief The AssingmentInfo class represents a data assignment.
    \since 5.8
    \inmodule QtScxml
 */

/*!
    \variable QScxmlExecutableContent::AssignmentInfo::expr
    \brief The expression to be evaluated
 */

/*!
    \variable QScxmlExecutableContent::AssignmentInfo::context
    \brief The context for evaluating the expression
 */

/*!
    \variable QScxmlExecutableContent::AssignmentInfo::dest
    \brief The name of the data item to assign to
 */

/*!
    \class QScxmlExecutableContent::ForeachInfo
    \brief The ForeachInfo class represents a foreach construct.
    \since 5.8
    \inmodule QtScxml
 */

/*!
    \variable QScxmlExecutableContent::ForeachInfo::array
    \brief The name of the array that is iterated over
 */

/*!
    \variable QScxmlExecutableContent::ForeachInfo::item
    \brief The name of the iteration variable
 */

/*!
    \variable QScxmlExecutableContent::ForeachInfo::index
    \brief The name of the index variable
 */

/*!
    \variable QScxmlExecutableContent::ForeachInfo::context
    \brief The context for evaluating the expression
 */

/*!
    \class QScxmlExecutableContent::ParameterInfo
    \brief The ParameterInfo class represents a parameter to a service
    invocation.
    \since 5.8
    \inmodule QtScxml
 */

/*!
    \variable QScxmlExecutableContent::ParameterInfo::name
    \brief The name of the parameter
 */

/*!
    \variable QScxmlExecutableContent::ParameterInfo::expr
    \brief The expression to be evaluated
 */

/*!
    \variable QScxmlExecutableContent::ParameterInfo::location
    \brief The data model name of the item to be passed as a parameter
 */

/*!
    \class QScxmlExecutableContent::InvokeInfo
    \brief The InvokeInfo class represents a service invocation.
    \since 5.8
    \inmodule QtScxml
 */

/*!
    \variable QScxmlExecutableContent::InvokeInfo::id
    \brief The ID specified by the \c id attribute in the \c <invoke> element.
 */

/*!
    \variable QScxmlExecutableContent::InvokeInfo::prefix
    \brief The unique prefix for this invocation in the context of the state
    from which it is called
 */

/*!
    \variable QScxmlExecutableContent::InvokeInfo::location
    \brief The data model location to write the invocation ID to
 */

/*!
    \variable QScxmlExecutableContent::InvokeInfo::context
    \brief The context to interpret the location in
 */

/*!
    \variable QScxmlExecutableContent::InvokeInfo::expr
    \brief The expression representing the srcexpr of the invoke element
 */

/*!
    \variable QScxmlExecutableContent::InvokeInfo::finalize
    \brief The ID of the container of executable content to be run on finalizing
    the invocation
 */

/*!
    \variable QScxmlExecutableContent::InvokeInfo::autoforward
    \brief Whether events should automatically be forwarded to the invoked
    service
 */


#ifndef BUILD_QSCXMLC
static int parseTime(const QString &t, bool *ok = 0)
{
    if (t.isEmpty()) {
        if (ok)
            *ok = false;
        return -1;
    }
    bool negative = false;
    int startPos = 0;
    if (t[0] == QLatin1Char('-')) {
        negative = true;
        ++startPos;
    } else if (t[0] == QLatin1Char('+')) {
        ++startPos;
    }
    int pos = startPos;
    for (int endPos = t.length(); pos < endPos; ++pos) {
        auto c = t[pos];
        if (c < QLatin1Char('0') || c > QLatin1Char('9'))
            break;
    }
    if (pos == startPos) {
        if (ok) *ok = false;
        return -1;
    }
    int value = t.midRef(startPos, pos - startPos).toInt(ok);
    if (ok && !*ok) return -1;
    if (t.length() == pos + 1 && t[pos] == QLatin1Char('s')) {
        value *= 1000;
    } else if (t.length() != pos + 2 || t[pos] != QLatin1Char('m') || t[pos + 1] != QLatin1Char('s')) {
        if (ok) *ok = false;
        return -1;
    }
    return negative ? -value : value;
}

QScxmlExecutionEngine::QScxmlExecutionEngine(QScxmlStateMachine *stateMachine)
    : stateMachine(stateMachine)
{
    Q_ASSERT(stateMachine);
}

bool QScxmlExecutionEngine::execute(ContainerId id, const QVariant &extraData)
{
    Q_ASSERT(stateMachine);

    if (id == NoInstruction)
        return true;

    const InstructionId *ip = stateMachine->tableData()->instructions() + id;
    this->extraData = extraData;
    bool result = true;
    step(ip, &result);
    this->extraData = QVariant();
    return result;
}

const InstructionId *QScxmlExecutionEngine::step(const InstructionId *ip, bool *ok)
{
    auto dataModel = stateMachine->dataModel();
    auto tableData = stateMachine->tableData();

    *ok = true;
    auto instr = reinterpret_cast<const Instruction *>(ip);
    switch (instr->instructionType) {
    case Instruction::Sequence: {
        qCDebug(qscxmlLog) << stateMachine << "Executing sequence step";
        const InstructionSequence *sequence = reinterpret_cast<const InstructionSequence *>(instr);
        ip = sequence->instructions();
        const InstructionId *end = ip + sequence->entryCount;
        while (ip < end) {
            ip = step(ip, ok);
            if (!(*ok)) {
                qCDebug(qscxmlLog) << stateMachine << "Finished sequence step UNsuccessfully";
                return end;
            }
        }
        qCDebug(qscxmlLog) << stateMachine << "Finished sequence step successfully";
        return ip;
    }

    case Instruction::Sequences: {
        qCDebug(qscxmlLog) << stateMachine << "Executing sequences step";
        const InstructionSequences *sequences
                = reinterpret_cast<const InstructionSequences *>(instr);
        ip += sequences->size();
        for (int i = 0; i != sequences->sequenceCount; ++i) {
            bool ignored;
            const InstructionId *sequence = sequences->at(i);
            step(sequence, &ignored);
        }
        qCDebug(qscxmlLog) << stateMachine << "Finished sequences step";
        return ip;
    }

    case Instruction::Send: {
        qCDebug(qscxmlLog) << stateMachine << "Executing send step";
        const Send *send = reinterpret_cast<const Send *>(instr);
        ip += send->size();

        QString delay = tableData->string(send->delay);
        if (send->delayexpr != NoEvaluator) {
            delay = stateMachine->dataModel()->evaluateToString(send->delayexpr, ok);
            if (!(*ok))
                return ip;
        }

        QScxmlEvent *event = QScxmlEventBuilder(stateMachine, *send).buildEvent();
        if (!event) {
            *ok = false;
            return ip;
        }

        if (!delay.isEmpty()) {
            int msecs = parseTime(delay);
            if (msecs >= 0) {
                event->setDelay(msecs);
            } else {
                qCDebug(qscxmlLog) << stateMachine << "failed to parse delay time" << delay;
                *ok = false;
                return ip;
            }
        }

        stateMachine->submitEvent(event);
        return ip;
    }

    case Instruction::JavaScript: {
        qCDebug(qscxmlLog) << stateMachine << "Executing script step";
        const JavaScript *javascript = reinterpret_cast<const JavaScript *>(instr);
        ip += javascript->size();
        dataModel->evaluateToVoid(javascript->go, ok);
        return ip;
    }

    case Instruction::If: {
        qCDebug(qscxmlLog) << stateMachine << "Executing if step";
        const If *_if = reinterpret_cast<const If *>(instr);
        ip += _if->size();
        auto blocks = _if->blocks();
        for (qint32 i = 0; i < _if->conditions.count; ++i) {
            bool conditionOk = true;
            if (dataModel->evaluateToBool(_if->conditions.at(i), &conditionOk) && conditionOk) {
                const InstructionId *block = blocks->at(i);
                step(block, ok);
                qCDebug(qscxmlLog) << stateMachine << "Finished if step";
                return ip;
            }
        }

        if (_if->conditions.count < blocks->sequenceCount)
            step(blocks->at(_if->conditions.count), ok);

        return ip;
    }

    case Instruction::Foreach: {
        class LoopBody: public QScxmlDataModel::ForeachLoopBody // If only we could put std::function in public API, we could use a lambda here. Alas....
        {
            QScxmlExecutionEngine *engine;
            const InstructionId *loopStart;

        public:
            LoopBody(QScxmlExecutionEngine *engine, const InstructionId *loopStart)
                : engine(engine)
                , loopStart(loopStart)
            {}

            void run(bool *ok) Q_DECL_OVERRIDE
            {
                engine->step(loopStart, ok);
            }
        };

        qCDebug(qscxmlLog) << stateMachine << "Executing foreach step";
        const Foreach *_foreach = reinterpret_cast<const Foreach *>(instr);
        const InstructionId *loopStart = _foreach->blockstart();
        ip += _foreach->size();
        LoopBody body(this, loopStart);
        dataModel->evaluateForeach(_foreach->doIt, ok, &body);
        return ip;
    }

    case Instruction::Raise: {
        qCDebug(qscxmlLog) << stateMachine << "Executing raise step";
        const Raise *raise = reinterpret_cast<const Raise *>(instr);
        ip += raise->size();
        auto name = tableData->string(raise->event);
        auto event = new QScxmlEvent;
        event->setName(name);
        event->setEventType(QScxmlEvent::InternalEvent);
        stateMachine->submitEvent(event);
        return ip;
    }

    case Instruction::Log: {
        qCDebug(qscxmlLog) << stateMachine << "Executing log step";
        const Log *log = reinterpret_cast<const Log *>(instr);
        ip += log->size();
        QString str = dataModel->evaluateToString(log->expr, ok);
        if (*ok) {
            const QString label = tableData->string(log->label);
            qCDebug(scxmlLog) << label << ":" << str;
            QMetaObject::invokeMethod(stateMachine,
                                      "log",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, label),
                                      Q_ARG(QString, str));
        }
        return ip;
    }

    case Instruction::Cancel: {
        qCDebug(qscxmlLog) << stateMachine << "Executing cancel step";
        const Cancel *cancel = reinterpret_cast<const Cancel *>(instr);
        ip += cancel->size();
        QString e = tableData->string(cancel->sendid);
        if (cancel->sendidexpr != NoEvaluator)
            e = dataModel->evaluateToString(cancel->sendidexpr, ok);
        if (*ok && !e.isEmpty())
            stateMachine->cancelDelayedEvent(e);
        return ip;
    }

    case Instruction::Assign: {
        qCDebug(qscxmlLog) << stateMachine << "Executing assign step";
        const Assign *assign = reinterpret_cast<const Assign *>(instr);
        ip += assign->size();
        dataModel->evaluateAssignment(assign->expression, ok);
        return ip;
    }

    case Instruction::Initialize: {
        qCDebug(qscxmlLog) << stateMachine << "Executing initialize step";
        const Initialize *init = reinterpret_cast<const Initialize *>(instr);
        ip += init->size();
        dataModel->evaluateInitialization(init->expression, ok);
        return ip;
    }

    case Instruction::DoneData: {
        qCDebug(qscxmlLog) << stateMachine << "Executing DoneData step";
        const DoneData *doneData = reinterpret_cast<const DoneData *>(instr);

        QString eventName = QStringLiteral("done.state.") + extraData.toString();
        QScxmlEventBuilder event(stateMachine, eventName, doneData);
        auto e = event();
        e->setEventType(QScxmlEvent::InternalEvent);
        qCDebug(qscxmlLog) << stateMachine << "submitting event" << eventName;
        stateMachine->submitEvent(e);
        return ip;
    }

    default:
        Q_UNREACHABLE();
        *ok = false;
        return ip;
    }
}
#endif // BUILD_QSCXMLC

QT_END_NAMESPACE
