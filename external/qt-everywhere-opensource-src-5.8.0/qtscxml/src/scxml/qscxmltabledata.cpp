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

#include "qscxmltabledata_p.h"
#include "qscxmlcompiler_p.h"
#include "qscxmlexecutablecontent_p.h"

QT_USE_NAMESPACE

/*!
    \class QScxmlTableData
    \since 5.8
    \inmodule QtScxml
    \brief The QScxmlTableData class is used by compiled state machines.

    QScxmlTableData is the interface to the compiled representation of SCXML
    state machines. It should only be used internally and by state machines
    compiled from SCXML documents.
 */

/*!
    \fn QScxmlTableData::string(QScxmlExecutableContent::StringId id) const
    Returns a QString for the given \a id.
 */

/*!
    \fn QScxmlTableData::instructions() const
    Returns a pointer to the instructions of executable content contained in
    the state machine.
 */

/*!
    \fn QScxmlTableData::evaluatorInfo(QScxmlExecutableContent::EvaluatorId evaluatorId) const
    Returns the QScxmlExecutableContent::EvaluatorInfo object for the given \a evaluatorId.
 */

/*!
    \fn QScxmlTableData::assignmentInfo(QScxmlExecutableContent::EvaluatorId assignmentId) const
    Returns the QScxmlExecutableContent::AssignmentInfo object for the given \a assignmentId.
 */

/*!
    \fn QScxmlTableData::foreachInfo(QScxmlExecutableContent::EvaluatorId foreachId) const
    Returns the QScxmlExecutableContent::ForeachInfo object for the given \a foreachId.
 */

/*!
    \fn QScxmlTableData::dataNames(int *count) const
    Retrieves the string IDs for the names of data items in the data model. The
    number of strings is saved into \a count and a pointer to an array of
    string IDs is returned.

    Returns a pointer to an array of string IDs.
 */

/*!
    \fn QScxmlTableData::initialSetup() const
    Initializes the table data. Returns the ID of the container with
    instructions to be executed when initializing the state machine.
 */

/*!
    \fn QScxmlTableData::name() const
    Returns the name of the state machine.
 */

/*!
    \fn QScxmlTableData::stateMachineTable() const
    Returns a pointer to the complete state table, expressed as an opaque
    sequence of integers.
 */

/*!
    \fn QScxmlTableData::serviceFactory(int id) const
    Returns the service factory that creates invokable services for the state
    with the ID \a id.
 */

using namespace QScxmlInternal;

namespace {
using namespace QScxmlExecutableContent;

class TableDataBuilder: public DocumentModel::NodeVisitor
{
public:
    TableDataBuilder(GeneratedTableData &tableData,
                     GeneratedTableData::MetaDataInfo &metaDataInfo,
                     GeneratedTableData::DataModelInfo &dataModelInfo,
                     GeneratedTableData::CreateFactoryId func)
        : createFactoryId(func)
        , m_tableData(tableData)
        , m_dataModelInfo(dataModelInfo)
        , m_stringTable(tableData.theStrings)
        , m_instructions(tableData.theInstructions)
        , m_evaluators(tableData.theEvaluators)
        , m_assignments(tableData.theAssignments)
        , m_foreaches(tableData.theForeaches)
        , m_dataIds(tableData.theDataNameIds)
        , m_stateNames(metaDataInfo.stateNames)

    {
        m_activeSequences.reserve(4);
        tableData.theInitialSetup = QScxmlExecutableContent::NoInstruction;
    }

    void buildTableData(DocumentModel::ScxmlDocument *doc)
    {
        m_isCppDataModel = doc->root->dataModel == DocumentModel::Scxml::CppDataModel;
        m_parents.reserve(32);
        m_allTransitions.resize(doc->allTransitions.size());
        m_docTransitionIndices.reserve(doc->allTransitions.size());
        for (auto *t : qAsConst(doc->allTransitions)) {
            m_docTransitionIndices.insert(t, m_docTransitionIndices.size());
        }
        m_docStatesIndices.reserve(doc->allStates.size());
        m_transitionsForState.resize(doc->allStates.size());
        m_allStates.resize(doc->allStates.size());
        for (DocumentModel::AbstractState *s : qAsConst(doc->allStates)) {
            m_docStatesIndices.insert(s, m_docStatesIndices.size());
        }

        doc->root->accept(this);
        m_stateTable.version = Q_QSCXMLC_OUTPUT_REVISION;
        generateStateMachineData();

        m_tableData.theInstructions.squeeze();
    }

    void generateStateMachineData()
    {
        const int tableSize = sizeof(StateTable) / sizeof(qint32);
        const int stateSize = qint32(sizeof(StateTable::State) / sizeof(qint32));
        const int transitionSize = qint32(sizeof(StateTable::Transition) / sizeof(qint32));

        m_stateTable.stateOffset = tableSize;
        m_stateTable.stateCount = m_allStates.size();
        m_stateTable.transitionOffset = m_stateTable.stateOffset +
                m_stateTable.stateCount * stateSize;
        m_stateTable.transitionCount = m_allTransitions.size();
        m_stateTable.arrayOffset = m_stateTable.transitionOffset +
                m_stateTable.transitionCount * transitionSize;
        m_stateTable.arraySize = m_arrays.size();

        const qint32 dataSize = qint32(tableSize)
                + (m_allStates.size() * stateSize)
                + (m_allTransitions.size() * transitionSize)
                + m_arrays.size()
                + 1;
        QVector<qint32> data(dataSize, -1);
        qint32 *ptr = data.data();

        memcpy(ptr, &m_stateTable, sizeof(m_stateTable));
        ptr += tableSize;

        Q_ASSERT(ptr == data.constData() + m_stateTable.stateOffset);
        memcpy(ptr, m_allStates.constData(),
               sizeof(StateTable::State) * size_t(m_allStates.size()));
        ptr += stateSize * size_t(m_allStates.size());

        Q_ASSERT(ptr == data.constData() + m_stateTable.transitionOffset);
        memcpy(ptr, m_allTransitions.constData(),
               sizeof(StateTable::Transition) * size_t(m_allTransitions.size()));
        ptr += transitionSize * size_t(m_allTransitions.size());

        Q_ASSERT(ptr == data.constData() + m_stateTable.arrayOffset);
        memcpy(ptr, m_arrays.constData(), sizeof(qint32) * size_t(m_arrays.size()));
        ptr += m_arrays.size();

        *ptr++ = StateTable::terminator;

        Q_ASSERT(ptr == data.constData() + dataSize);

        m_tableData.theStateMachineTable = data;
    }

protected: // visitor
    using NodeVisitor::visit;

    bool visit(DocumentModel::Scxml *node) Q_DECL_OVERRIDE Q_DECL_FINAL
    {
        setName(node->name);

        switch (node->dataModel) {
        case DocumentModel::Scxml::NullDataModel:
            m_stateTable.dataModel = StateTable::NullDataModel;
            break;
        case DocumentModel::Scxml::JSDataModel:
            m_stateTable.dataModel = StateTable::EcmaScriptDataModel;
            break;
        case DocumentModel::Scxml::CppDataModel:
            m_stateTable.dataModel = StateTable::CppDataModel;
            break;
        default:
            m_stateTable.dataModel = StateTable::InvalidDataModel;
            break;
        }

        switch (node->binding) {
        case DocumentModel::Scxml::EarlyBinding:
            m_stateTable.binding = StateTable::EarlyBinding;
            break;
        case DocumentModel::Scxml::LateBinding:
            m_stateTable.binding = StateTable::LateBinding;
            m_bindLate = true;
            break;
        default:
            Q_UNREACHABLE();
        }

        m_stateTable.name = addString(node->name);

        m_parents.append(-1);
        visit(node->children);

        m_dataElements.append(node->dataElements);
        if (node->script || !m_dataElements.isEmpty() || !node->initialSetup.isEmpty()) {
            setInitialSetup(startNewSequence());
            generate(m_dataElements);
            if (node->script) {
                node->script->accept(this);
            }
            visit(&node->initialSetup);
            endSequence();
        }

        QVector<DocumentModel::AbstractState *> childStates;
        for (DocumentModel::StateOrTransition *sot : qAsConst(node->children)) {
            if (DocumentModel::AbstractState *s = sot->asAbstractState()) {
                childStates.append(s);
            }
        }
        m_stateTable.childStates = addStates(childStates);
        if (node->initialTransition) {
            visit(node->initialTransition);
            const int transitionIndex = m_docTransitionIndices.value(node->initialTransition, -1);
            Q_ASSERT(transitionIndex != -1);
            m_stateTable.initialTransition = transitionIndex;
        }
        m_parents.removeLast();

        return false;
    }

    bool visit(DocumentModel::State *state) Q_DECL_OVERRIDE Q_DECL_FINAL
    {
        m_stateNames.add(state->id);
        const int stateIndex = m_docStatesIndices.value(state, -1);
        Q_ASSERT(stateIndex != -1);
        StateTable::State &newState = m_allStates[stateIndex];
        newState.name = addString(state->id);
        newState.parent = currentParent();

        switch (state->type) {
        case DocumentModel::State::Normal:
            newState.type = StateTable::State::Normal;
            break;
        case DocumentModel::State::Parallel:
            newState.type = StateTable::State::Parallel;
            break;
        case DocumentModel::State::Final:
            newState.type = StateTable::State::Final;
            newState.doneData = generate(state->doneData);
            break;
        default:
            Q_UNREACHABLE();
        }

        m_parents.append(stateIndex);

        if (!state->dataElements.isEmpty()) {
            if (m_bindLate) {
                newState.initInstructions = startNewSequence();
                generate(state->dataElements);
                endSequence();
            } else {
                m_dataElements.append(state->dataElements);
            }
        }

        newState.entryInstructions = generate(state->onEntry);
        newState.exitInstructions = generate(state->onExit);
        if (!state->invokes.isEmpty()) {
            QVector<int> factoryIds;
            for (DocumentModel::Invoke *invoke : qAsConst(state->invokes)) {
                auto ctxt = createContext(QStringLiteral("invoke"));
                QVector<QScxmlExecutableContent::StringId> namelist;
                for (const QString &name : qAsConst(invoke->namelist))
                    namelist += addString(name);
                QVector<QScxmlExecutableContent::ParameterInfo> params;
                for (DocumentModel::Param *param : qAsConst(invoke->params)) {
                    QScxmlExecutableContent::ParameterInfo p;
                    p.name = addString(param->name);
                    p.expr = createEvaluatorVariant(QStringLiteral("param"), QStringLiteral("expr"),
                                                    param->expr);
                    p.location = addString(param->location);
                    params.append(p);
                }
                QScxmlExecutableContent::ContainerId finalize =
                        QScxmlExecutableContent::NoInstruction;
                if (!invoke->finalize.isEmpty()) {
                    finalize = startNewSequence();
                    visit(&invoke->finalize);
                    endSequence();
                }
                auto srcexpr = createEvaluatorString(QStringLiteral("invoke"),
                                                     QStringLiteral("srcexpr"),
                                                     invoke->srcexpr);
                QScxmlExecutableContent::InvokeInfo invokeInfo;
                invokeInfo.id = addString(invoke->id);
                invokeInfo.prefix = addString(state->id + QStringLiteral(".session-"));
                invokeInfo.location = addString(invoke->idLocation);
                invokeInfo.context = ctxt;
                invokeInfo.expr = srcexpr;
                invokeInfo.finalize = finalize;
                invokeInfo.autoforward = invoke->autoforward;
                const int factoryId = createFactoryId(invokeInfo, namelist, params,
                                                      invoke->content);
                Q_ASSERT(factoryId >= 0);
                factoryIds.append(factoryId);
                m_stateTable.maxServiceId = std::max(m_stateTable.maxServiceId, factoryId);
            }
            newState.serviceFactoryIds = addArray(factoryIds);
        }

        visit(state->children);

        QVector<DocumentModel::AbstractState *> childStates;
        for (DocumentModel::StateOrTransition *sot : qAsConst(state->children)) {
            if (auto s = sot->asAbstractState()) {
                childStates.append(s);
            }
        }
        newState.childStates = addStates(childStates);
        newState.transitions = addArray(m_transitionsForState.at(stateIndex));
        if (state->initialTransition) {
            visit(state->initialTransition);
            newState.initialTransition = m_transitionsForState.at(stateIndex).last();
        }
        m_parents.removeLast();

        return false;
    }

    bool visit(DocumentModel::Transition *transition) Q_DECL_OVERRIDE Q_DECL_FINAL
    {
        const int transitionIndex = m_docTransitionIndices.value(transition, -1);
        Q_ASSERT(transitionIndex != -1);
        StateTable::Transition &newTransition = m_allTransitions[transitionIndex];
        const int parentIndex = currentParent();
        if (parentIndex != -1) {
            m_transitionsForState[parentIndex].append(transitionIndex);
        }
        newTransition.source = parentIndex;

        if (transition->condition) {
            newTransition.condition = createEvaluatorBool(QStringLiteral("transition"),
                                                          QStringLiteral("cond"),
                                                          *transition->condition.data());
        }

        switch (transition->type) {
        case DocumentModel::Transition::External:
            newTransition.type = StateTable::Transition::External;
            break;
        case DocumentModel::Transition::Internal:
            newTransition.type = StateTable::Transition::Internal;
            break;
        case DocumentModel::Transition::Synthetic:
            newTransition.type = StateTable::Transition::Synthetic;
            break;
        default:
            Q_UNREACHABLE();
        }

        if (!transition->instructionsOnTransition.isEmpty()) {
            m_currentTransition = transitionIndex;
            newTransition.transitionInstructions = startNewSequence();
            visit(&transition->instructionsOnTransition);
            endSequence();
            m_currentTransition = -1;
        }

        newTransition.targets = addStates(transition->targetStates);

        QVector<int> eventIds;
        for (const QString &event : qAsConst(transition->events))
            eventIds.push_back(addString(event));

        newTransition.events = addArray(eventIds);

        return false;
    }

    bool visit(DocumentModel::HistoryState *historyState) Q_DECL_OVERRIDE Q_DECL_FINAL
    {
        const int stateIndex = m_docStatesIndices.value(historyState, -1);
        Q_ASSERT(stateIndex != -1);
        StateTable::State &newState = m_allStates[stateIndex];
        newState.name = addString(historyState->id);
        newState.parent = currentParent();

        switch (historyState->type) {
        case DocumentModel::HistoryState::Shallow:
            newState.type = StateTable::State::ShallowHistory;
            break;
        case DocumentModel::HistoryState::Deep:
            newState.type = StateTable::State::DeepHistory;
            break;
        default:
            Q_UNREACHABLE();
        }

        m_parents.append(stateIndex);
        visit(historyState->children);
        m_parents.removeLast();
        newState.transitions = addArray(m_transitionsForState.at(stateIndex));
        return false;
    }

    bool visit(DocumentModel::Send *node) Q_DECL_OVERRIDE Q_DECL_FINAL
    {
        auto instr = m_instructions.add<Send>(Send::calculateExtraSize(node->params.size(),
                                                                       node->namelist.size()));
        instr->instructionLocation = createContext(QStringLiteral("send"));
        instr->event = addString(node->event);
        instr->eventexpr = createEvaluatorString(QStringLiteral("send"),
                                                 QStringLiteral("eventexpr"),
                                                 node->eventexpr);
        instr->type = addString(node->type);
        instr->typeexpr = createEvaluatorString(QStringLiteral("send"),
                                                QStringLiteral("typeexpr"),
                                                node->typeexpr);
        instr->target = addString(node->target);
        instr->targetexpr = createEvaluatorString(QStringLiteral("send"),
                                                  QStringLiteral("targetexpr"),
                                                  node->targetexpr);
        instr->id = addString(node->id);
        instr->idLocation = addString(node->idLocation);
        instr->delay = addString(node->delay);
        instr->delayexpr = createEvaluatorString(QStringLiteral("send"),
                                                 QStringLiteral("delayexpr"),
                                                 node->delayexpr);
        instr->content = addString(node->content);
        instr->contentexpr = createEvaluatorString(QStringLiteral("send"),
                                                   QStringLiteral("contentexpr"),
                                                   node->contentexpr);
        generate(&instr->namelist, node->namelist);
        generate(instr->params(), node->params);
        return false;
    }

    void visit(DocumentModel::Raise *node) Q_DECL_OVERRIDE Q_DECL_FINAL
    {
        auto instr = m_instructions.add<Raise>();
        instr->event = addString(node->event);
    }

    void visit(DocumentModel::Log *node) Q_DECL_OVERRIDE Q_DECL_FINAL
    {
        auto instr = m_instructions.add<Log>();
        instr->label = addString(node->label);
        instr->expr = createEvaluatorString(QStringLiteral("log"),
                                            QStringLiteral("expr"),
                                            node->expr);
    }

    void visit(DocumentModel::Script *node) Q_DECL_OVERRIDE Q_DECL_FINAL
    {
        auto instr = m_instructions.add<JavaScript>();
        instr->go = createEvaluatorVoid(QStringLiteral("script"),
                                        QStringLiteral("source"),
                                        node->content);
    }

    void visit(DocumentModel::Assign *node) Q_DECL_OVERRIDE Q_DECL_FINAL
    {
        auto instr = m_instructions.add<Assign>();
        auto ctxt = createContext(QStringLiteral("assign"), QStringLiteral("expr"), node->expr);
        instr->expression = addAssignment(node->location, node->expr, ctxt);
    }

    bool visit(DocumentModel::If *node) Q_DECL_OVERRIDE Q_DECL_FINAL
    {
        auto instr = m_instructions.add<If>(node->conditions.size());
        instr->conditions.count = node->conditions.size();
        auto it = instr->conditions.data();
        QString tag = QStringLiteral("if");
        for (int i = 0, ei = node->conditions.size(); i != ei; ++i) {
            *it++ = createEvaluatorBool(tag, QStringLiteral("cond"), node->conditions.at(i));
            if (i == 0) {
                tag = QStringLiteral("elif");
            }
        }
        auto outSequences = m_instructions.add<InstructionSequences>();
        generate(outSequences, node->blocks);
        return false;
    }

    bool visit(DocumentModel::Foreach *node) Q_DECL_OVERRIDE Q_DECL_FINAL
    {
        auto instr = m_instructions.add<Foreach>();
        auto ctxt = createContextString(QStringLiteral("foreach"));
        instr->doIt = addForeach(node->array, node->item, node->index, ctxt);
        startSequence(&instr->block);
        visit(&node->block);
        endSequence();
        return false;
    }

    void visit(DocumentModel::Cancel *node) Q_DECL_OVERRIDE Q_DECL_FINAL
    {
        auto instr = m_instructions.add<Cancel>();
        instr->sendid = addString(node->sendid);
        instr->sendidexpr = createEvaluatorString(QStringLiteral("cancel"),
                                                  QStringLiteral("sendidexpr"),
                                                  node->sendidexpr);
    }

protected:
    static int paramSize() { return sizeof(ParameterInfo) / sizeof(qint32); }

    ContainerId generate(const DocumentModel::DoneData *node)
    {
        auto id = m_instructions.newContainerId();
        DoneData *doneData;
        if (node) {
            doneData = m_instructions.add<DoneData>(node->params.size() * paramSize());
            doneData->contents = addString(node->contents);
            doneData->expr = createEvaluatorString(QStringLiteral("donedata"),
                                                   QStringLiteral("expr"),
                                                   node->expr);
            generate(&doneData->params, node->params);
        } else {
            doneData = m_instructions.add<DoneData>();
            doneData->contents = NoString;
            doneData->expr = NoEvaluator;
            doneData->params.count = 0;
        }
        doneData->location = createContext(QStringLiteral("final"));
        return id;
    }

    StringId createContext(const QString &instrName)
    {
        return addString(createContextString(instrName));
    }

    void generate(const QVector<DocumentModel::DataElement *> &dataElements)
    {
        for (DocumentModel::DataElement *el : dataElements) {
            auto ctxt = createContext(QStringLiteral("data"), QStringLiteral("expr"), el->expr);
            auto evaluator = addDataElement(el->id, el->expr, ctxt);
            if (evaluator != NoEvaluator) {
                auto instr = m_instructions.add<QScxmlExecutableContent::Initialize>();
                instr->expression = evaluator;
            }
        }
    }

    ContainerId generate(const DocumentModel::InstructionSequences &inSequences)
    {
        if (inSequences.isEmpty())
            return NoInstruction;

        auto id = m_instructions.newContainerId();
        auto outSequences = m_instructions.add<InstructionSequences>();
        generate(outSequences, inSequences);
        return id;
    }

    void generate(Array<ParameterInfo> *out, const QVector<DocumentModel::Param *> &in)
    {
        out->count = in.size();
        ParameterInfo *it = out->data();
        for (DocumentModel::Param *f : in) {
            it->name = addString(f->name);
            it->expr = createEvaluatorVariant(QStringLiteral("param"), QStringLiteral("expr"),
                                              f->expr);
            it->location = addString(f->location);
            ++it;
        }
    }

    void generate(InstructionSequences *outSequences,
                  const DocumentModel::InstructionSequences &inSequences)
    {
        int sequencesOffset = m_instructions.offset(outSequences);
        int sequenceCount = 0;
        int entryCount = 0;
        for (DocumentModel::InstructionSequence *sequence : inSequences) {
            ++sequenceCount;
            startNewSequence();
            visit(sequence);
            entryCount += endSequence()->size();
        }
        outSequences = m_instructions.at<InstructionSequences>(sequencesOffset);
        outSequences->sequenceCount = sequenceCount;
        outSequences->entryCount = entryCount;
    }

    void generate(Array<StringId> *out, const QStringList &in)
    {
        out->count = in.size();
        StringId *it = out->data();
        for (const QString &str : in) {
            *it++ = addString(str);
        }
    }

    ContainerId startNewSequence()
    {
        auto id = m_instructions.newContainerId();
        auto sequence = m_instructions.add<InstructionSequence>();
        startSequence(sequence);
        return id;
    }

    void startSequence(InstructionSequence *sequence)
    {
        SequenceInfo info;
        info.location = m_instructions.offset(sequence);
        info.entryCount = 0;
        m_activeSequences.push_back(info);
        m_instructions.setSequenceInfo(&m_activeSequences.last());
        sequence->instructionType = Instruction::Sequence;
        sequence->entryCount = -1; // checked in endSequence
    }

    InstructionSequence *endSequence()
    {
        SequenceInfo info = m_activeSequences.back();
        m_activeSequences.pop_back();
        m_instructions.setSequenceInfo(m_activeSequences.isEmpty() ? Q_NULLPTR :
                                                                     &m_activeSequences.last());

        auto sequence = m_instructions.at<InstructionSequence>(info.location);
        Q_ASSERT(sequence->entryCount == -1); // set in startSequence
        sequence->entryCount = info.entryCount;
        if (!m_activeSequences.isEmpty())
            m_activeSequences.last().entryCount += info.entryCount;
        return sequence;
    }

    EvaluatorId createEvaluatorString(const QString &instrName, const QString &attrName,
                                      const QString &expr)
    {
        if (!expr.isEmpty()) {
            if (isCppDataModel()) {
                auto id = m_evaluators.add(EvaluatorInfo(), false);
                m_dataModelInfo.stringEvaluators.insert(id, expr);
                return id;
            } else {
                return addEvaluator(expr, createContext(instrName, attrName, expr));
            }
        }

        return NoEvaluator;
    }

    EvaluatorId createEvaluatorBool(const QString &instrName, const QString &attrName,
                                    const QString &cond)
    {
        if (!cond.isEmpty()) {
            if (isCppDataModel()) {
                auto id = m_evaluators.add(EvaluatorInfo(), false);
                m_dataModelInfo.boolEvaluators.insert(id, cond);
                return id;
            } else {
                return addEvaluator(cond, createContext(instrName, attrName, cond));
            }
        }

        return NoEvaluator;
    }

    EvaluatorId createEvaluatorVariant(const QString &instrName, const QString &attrName,
                                       const QString &expr)
    {
        if (!expr.isEmpty()) {
            if (isCppDataModel()) {
                auto id = m_evaluators.add(EvaluatorInfo(), false);
                m_dataModelInfo.variantEvaluators.insert(id, expr);
                return id;
            } else {
                return addEvaluator(expr, createContext(instrName, attrName, expr));
            }
        }

        return NoEvaluator;
    }

    EvaluatorId createEvaluatorVoid(const QString &instrName, const QString &attrName,
                                    const QString &stuff)
    {
        if (!stuff.isEmpty()) {
            if (isCppDataModel()) {
                auto id = m_evaluators.add(EvaluatorInfo(), false);
                m_dataModelInfo.voidEvaluators.insert(id, stuff);
                return id;
            } else {
                return addEvaluator(stuff, createContext(instrName, attrName, stuff));
            }
        }

        return NoEvaluator;
    }

    GeneratedTableData *tableData(const QVector<int> &stateMachineTable);

    StringId addString(const QString &str)
    { return str.isEmpty() ? NoString : m_stringTable.add(str); }

    void setInitialSetup(ContainerId id)
    { m_tableData.theInitialSetup = id; }

    void setName(const QString &name)
    { m_tableData.theName = addString(name); }

    bool isCppDataModel() const
    { return m_isCppDataModel; }

    int addStates(const QVector<DocumentModel::AbstractState *> &states)
    {
        QVector<int> array;
        for (auto *s : states) {
            int si = m_docStatesIndices.value(s, -1);
            Q_ASSERT(si != -1);
            array.push_back(si);
        }

        return addArray(array);
    }

    int addArray(const QVector<int> &array)
    {
        if (array.isEmpty())
            return -1;

        const int res = m_arrays.size();
        m_arrays.push_back(array.size());
        m_arrays.append(array);
        return res;
    }

    int currentParent() const
    {
        return m_parents.last();
    }

    QString createContextString(const QString &instrName) const
    {
        if (m_currentTransition != -1) {
            QString state;
            int parent = m_allTransitions.at(m_currentTransition).source;
            if (parent != -1) {
                QString parentName = QStringLiteral("(none)");
                int name = m_allStates.at(parent).name;
                if (name != -1) {
                    parentName = m_stringTable.item(name);
                }
                state = QStringLiteral(" of state '%1'").arg(parentName);
            }
            return QStringLiteral("%1 instruction in transition %3").arg(instrName, state);
        } else {
            QString parentName = QStringLiteral("(none)");
            const int parent = currentParent();
            if (parent != -1) {
                const int name = m_allStates.at(parent).name;
                if (name != -1) {
                    parentName = m_stringTable.item(name);
                }
            }
            return QStringLiteral("%1 instruction in state %2").arg(instrName, parentName);
        }
    }

    QString createContext(const QString &instrName, const QString &attrName,
                          const QString &attrValue) const
    {
        const QString location = createContextString(instrName);
        return QStringLiteral("%1 with %2=\"%3\"").arg(location, attrName, attrValue);
    }

    EvaluatorId addEvaluator(const QString &expr, const QString &context)
    {
        EvaluatorInfo ei;
        ei.expr = addString(expr);
        ei.context = addString(context);
        return m_evaluators.add(ei);
    }

    EvaluatorId addAssignment(const QString &dest, const QString &expr, const QString &context)
    {
        AssignmentInfo ai;
        ai.dest = addString(dest);
        ai.expr = addString(expr);
        ai.context = addString(context);
        return m_assignments.add(ai);
    }

    EvaluatorId addForeach(const QString &array, const QString &item, const QString &index,
                           const QString &context)
    {
        ForeachInfo fi;
        fi.array = addString(array);
        fi.item = addString(item);
        fi.index = addString(index);
        fi.context = addString(context);
        return m_foreaches.add(fi);
    }

    EvaluatorId addDataElement(const QString &id, const QString &expr, const QString &context)
    {
        auto str = addString(id);
        if (!m_dataIds.contains(str))
            m_dataIds.append(str);
        if (expr.isEmpty())
            return NoEvaluator;

        return addAssignment(id, expr, context);
    }

private:
    template <class Container, typename T, typename U>
    class Table {
        Container &elements;
        QMap<T, int> indexForElement;

    public:
        Table(Container &storage)
            : elements(storage)
        {}

        U add(const T &s, bool uniqueOnly = true) {
            int pos = uniqueOnly ? indexForElement.value(s, -1) : -1;
            if (pos == -1) {
                pos = elements.size();
                elements.append(s);
                indexForElement.insert(s, pos);
            }
            return pos;
        }

        Container data() {
            return elements;
        }

        const T &item(U pos) const {
            return elements.at(pos);
        }
    };

    struct SequenceInfo {
        int location;
        qint32 entryCount; // the amount of qint32's that the instructions take up
    };

    class InstructionStorage {
    public:
        InstructionStorage(QVector<qint32> &storage)
            : m_instr(storage)
            , m_info(Q_NULLPTR)
        {}

        ContainerId newContainerId() const { return m_instr.size(); }

        template <typename T>
        T *add(int extra = 0)
        {
            const int pos = m_instr.size();
            const int size = sizeof(T) / sizeof(qint32) + extra;
            if (m_info)
                m_info->entryCount += size;
            m_instr.resize(pos + size);
            T *instr = at<T>(pos);
            Q_ASSERT(instr->instructionType == 0);
            instr->instructionType = T::kind();
            return instr;
        }

        int offset(Instruction *instr) const
        {
            return reinterpret_cast<qint32 *>(instr) - m_instr.data();
        }

        template <typename T>
        T *at(int offset)
        {
            return reinterpret_cast<T *>(&m_instr[offset]);
        }

        void setSequenceInfo(SequenceInfo *info)
        {
            m_info = info;
        }

    private:
        QVector<qint32> &m_instr;
        SequenceInfo *m_info;
    };

    QVector<SequenceInfo> m_activeSequences;

    GeneratedTableData::CreateFactoryId createFactoryId;
    GeneratedTableData &m_tableData;
    GeneratedTableData::DataModelInfo &m_dataModelInfo;
    Table<QStringList, QString, StringId> m_stringTable;
    InstructionStorage m_instructions;
    Table<QVector<EvaluatorInfo>, EvaluatorInfo, EvaluatorId> m_evaluators;
    Table<QVector<AssignmentInfo>, AssignmentInfo, EvaluatorId> m_assignments;
    Table<QVector<ForeachInfo>, ForeachInfo, EvaluatorId> m_foreaches;
    QVector<StringId> &m_dataIds;
    bool m_isCppDataModel = false;

    StateTable m_stateTable;
    QVector<int> m_parents;
    QVector<qint32> m_arrays;

    QVector<StateTable::Transition> m_allTransitions;
    QHash<DocumentModel::Transition *, int> m_docTransitionIndices;
    QVector<StateTable::State> m_allStates;
    QHash<DocumentModel::AbstractState *, int> m_docStatesIndices;
    QVector<QVector<int>> m_transitionsForState;

    int m_currentTransition = StateTable::InvalidIndex;
    bool m_bindLate = false;
    QVector<DocumentModel::DataElement *> m_dataElements;
    Table<QStringList, QString, int> m_stateNames;
};

} // anonymous namespace

/*!
    \fn QScxmlTableData::~QScxmlTableData()
    Destroys the SXCML table data.
 */
QScxmlTableData::~QScxmlTableData()
{}

void GeneratedTableData::build(DocumentModel::ScxmlDocument *doc,
                               GeneratedTableData *table,
                               MetaDataInfo *metaDataInfo,
                               DataModelInfo *dataModelInfo,
                               GeneratedTableData::CreateFactoryId func)
{
    TableDataBuilder builder(*table, *metaDataInfo, *dataModelInfo, func);
    builder.buildTableData(doc);
}

QString GeneratedTableData::toString(const int *stateMachineTable)
{
    QString result;
    QTextStream out(&result);

    const StateTable *st = reinterpret_cast<const StateTable *>(stateMachineTable);

    out << "{" << endl
        << "\t0x" << hex << st->version << dec << ", // version" << endl
        << "\t" << st->name << ", // name" << endl
        << "\t" << st->dataModel << ", // data-model" << endl
        << "\t" << st->childStates << ", // child states array offset" << endl
        << "\t" << st->initialTransition << ", // transition to initial states" << endl
        << "\t" << st->initialSetup << ", // initial setup" << endl
        << "\t" << st->binding << ", // binding" << endl
        << "\t" << st->maxServiceId << ", // maxServiceId" << endl
        << "\t" << st->stateOffset << ", " << st->stateCount
                                           << ", // state offset and count" << endl
        << "\t" << st->transitionOffset << ", " << st->transitionCount
                                                << ", // transition offset and count" << endl
        << "\t" << st->arrayOffset << ", " << st->arraySize << ", // array offset and size" << endl
        << endl;

    out << "\t// States:" << endl;
    for (int i = 0; i < st->stateCount; ++i) {
        const StateTable::State &s = st->state(i);
        out << "\t"
            << s.name << ", "
            << s.parent << ", "
            << s.type << ", "
            << s.initialTransition << ", "
            << s.initInstructions << ", "
            << s.entryInstructions << ", "
            << s.exitInstructions << ", "
            << s.doneData << ", "
            << s.childStates << ", "
            << s.transitions << ", "
            << s.serviceFactoryIds << ","
            << endl;
    }

    out << endl
        << "\t// Transitions:" << endl;
    for (int i = 0; i < st->transitionCount; ++i) {
        auto t = st->transition(i);
        out << "\t"
            << t.events << ", "
            << t.condition << ", "
            << t.type << ", "
            << t.source << ", "
            << t.targets << ", "
            << t.transitionInstructions << ", "
            << endl ;
    }

    out << endl
        << "\t// Arrays:" << endl;
    int nextStart = 0;
    while (nextStart < st->arraySize) {
        const StateTable::Array a = st->array(nextStart);
        out << "\t" << a.size() << ", ";
        for (int j = 0; j < a.size(); ++j) {
            out << a[j] << ", ";
        }
        out << endl;
        nextStart += a.size() + 1;
    }

    out << hex;
    out << endl
        << "\t0x" << StateTable::terminator << " // terminator" << endl
        << "}";

    return result;
}

QString GeneratedTableData::string(StringId id) const
{
    return id == NoString ? QString() : theStrings.at(id);
}

InstructionId *GeneratedTableData::instructions() const
{
    return const_cast<InstructionId *>(theInstructions.data());
}

EvaluatorInfo GeneratedTableData::evaluatorInfo(EvaluatorId evaluatorId) const
{
    return theEvaluators[evaluatorId];
}

AssignmentInfo GeneratedTableData::assignmentInfo(EvaluatorId assignmentId) const
{
    return theAssignments[assignmentId];
}

ForeachInfo GeneratedTableData::foreachInfo(EvaluatorId foreachId) const
{
    return theForeaches[foreachId];
}

StringId *GeneratedTableData::dataNames(int *count) const
{
    Q_ASSERT(count);
    *count = theDataNameIds.size();
    return const_cast<StringId *>(theDataNameIds.data());
}

ContainerId GeneratedTableData::initialSetup() const
{
    return theInitialSetup;
}

QString GeneratedTableData::name() const
{
    return string(theName);
}

const qint32 *GeneratedTableData::stateMachineTable() const
{
    return theStateMachineTable.constData();
}

QScxmlInvokableServiceFactory *GeneratedTableData::serviceFactory(int id) const
{
    Q_UNUSED(id);
    return nullptr;
}
