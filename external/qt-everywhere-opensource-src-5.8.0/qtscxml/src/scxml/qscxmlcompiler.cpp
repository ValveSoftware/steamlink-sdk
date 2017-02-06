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

#include "qscxmlcompiler_p.h"
#include "qscxmlexecutablecontent_p.h"

#include <QXmlStreamReader>
#include <QLoggingCategory>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QVector>
#include <QString>

#ifndef BUILD_QSCXMLC
#include "qscxmlecmascriptdatamodel.h"
#include "qscxmlinvokableservice_p.h"
#include "qscxmldatamodel_p.h"
#include "qscxmlstatemachine_p.h"
#include "qscxmlstatemachine.h"
#include "qscxmltabledata_p.h"

#include <QState>
#include <QHistoryState>
#include <QEventTransition>
#include <QSignalTransition>
#include <QJSValue>
#include <private/qabstracttransition_p.h>
#include <private/qmetaobjectbuilder_p.h>
#endif // BUILD_QSCXMLC

#include <functional>

namespace {
enum {
    DebugHelper_NameTransitions = 0
};
} // anonymous namespace

QT_BEGIN_NAMESPACE

static QString scxmlNamespace = QStringLiteral("http://www.w3.org/2005/07/scxml");
static QString qtScxmlNamespace = QStringLiteral("http://theqtcompany.com/scxml/2015/06/");

namespace {

class ScxmlVerifier: public DocumentModel::NodeVisitor
{
public:
    ScxmlVerifier(std::function<void (const DocumentModel::XmlLocation &, const QString &)> errorHandler)
        : m_errorHandler(errorHandler)
        , m_doc(Q_NULLPTR)
        , m_hasErrors(false)
    {}

    bool verify(DocumentModel::ScxmlDocument *doc)
    {
        if (doc->isVerified)
            return true;

        doc->isVerified = true;
        m_doc = doc;
        for (DocumentModel::AbstractState *state : qAsConst(doc->allStates)) {
            if (state->id.isEmpty()) {
                continue;
#ifndef QT_NO_DEBUG
            } else if (m_stateById.contains(state->id)) {
                Q_ASSERT(!"Should be unreachable: the compiler should check for this case!");
#endif // QT_NO_DEBUG
            } else {
                m_stateById[state->id] = state;
            }
        }

        if (doc->root)
            doc->root->accept(this);
        return !m_hasErrors;
    }

private:
    bool visit(DocumentModel::Scxml *scxml) Q_DECL_OVERRIDE
    {
        if (!scxml->name.isEmpty() && !isValidToken(scxml->name, XmlNmtoken)) {
            error(scxml->xmlLocation,
                  QStringLiteral("scxml name '%1' is not a valid XML Nmtoken").arg(scxml->name));
        }

        if (scxml->initial.isEmpty()) {
            if (auto firstChild = firstAbstractState(scxml)) {
                scxml->initialTransition = createInitialTransition({firstChild});
            }
        } else {
            QVector<DocumentModel::AbstractState *> initialStates;
            for (const QString &initial : qAsConst(scxml->initial)) {
                if (DocumentModel::AbstractState *s = m_stateById.value(initial))
                    initialStates.append(s);
                else
                    error(scxml->xmlLocation, QStringLiteral("initial state '%1' not found for <scxml> element").arg(initial));
            }
            scxml->initialTransition = createInitialTransition(initialStates);
        }

        m_parentNodes.append(scxml);

        return true;
    }

    void endVisit(DocumentModel::Scxml *) Q_DECL_OVERRIDE
    {
        m_parentNodes.removeLast();
    }

    bool visit(DocumentModel::State *state) Q_DECL_OVERRIDE
    {
        if (!state->id.isEmpty() && !isValidToken(state->id, XmlNCName)) {
            error(state->xmlLocation, QStringLiteral("'%1' is not a valid XML ID").arg(state->id));
        }

        if (state->initialTransition == nullptr) {
            if (state->initial.isEmpty()) {
                if (state->type == DocumentModel::State::Parallel) {
                    auto allChildren = allAbstractStates(state);
                    state->initialTransition = createInitialTransition(allChildren);
                } else {
                    if (auto firstChild = firstAbstractState(state)) {
                        state->initialTransition = createInitialTransition({firstChild});
                    }
                }
            } else {
                Q_ASSERT(state->type == DocumentModel::State::Normal);
                QVector<DocumentModel::AbstractState *> initialStates;
                for (const QString &initialState : qAsConst(state->initial)) {
                    if (DocumentModel::AbstractState *s = m_stateById.value(initialState)) {
                        initialStates.append(s);
                    } else {
                        error(state->xmlLocation,
                              QStringLiteral("undefined initial state '%1' for state '%2'")
                              .arg(initialState, state->id));
                    }
                }
                state->initialTransition = createInitialTransition(initialStates);
            }
        } else {
            if (state->initial.isEmpty()) {
                visit(state->initialTransition);
            } else {
                error(state->xmlLocation,
                      QStringLiteral("initial transition and initial attribute for state '%1'")
                      .arg(state->id));
            }
        }

        switch (state->type) {
        case DocumentModel::State::Normal:
            break;
        case DocumentModel::State::Parallel:
            if (!state->initial.isEmpty()) {
                error(state->xmlLocation,
                      QStringLiteral("parallel states cannot have an initial state"));
            }
            break;
        case DocumentModel::State::Final:
            break;
        default:
            Q_UNREACHABLE();
        }

        m_parentNodes.append(state);
        return true;
    }

    void endVisit(DocumentModel::State *) Q_DECL_OVERRIDE
    {
        m_parentNodes.removeLast();
    }

    bool visit(DocumentModel::Transition *transition) Q_DECL_OVERRIDE
    {
        Q_ASSERT(transition->targetStates.isEmpty());

        if (int size = transition->targets.size())
            transition->targetStates.reserve(size);
        for (const QString &target : qAsConst(transition->targets)) {
            if (DocumentModel::AbstractState *s = m_stateById.value(target)) {
                if (transition->targetStates.contains(s)) {
                    error(transition->xmlLocation, QStringLiteral("duplicate target '%1'").arg(target));
                } else {
                    transition->targetStates.append(s);
                }
            } else if (!target.isEmpty()) {
                error(transition->xmlLocation, QStringLiteral("unknown state '%1' in target").arg(target));
            }
        }
        for (const QString &event : qAsConst(transition->events))
            checkEvent(event, transition->xmlLocation, AllowWildCards);

        m_parentNodes.append(transition);
        return true;
    }

    void endVisit(DocumentModel::Transition *) Q_DECL_OVERRIDE
    {
        m_parentNodes.removeLast();
    }

    bool visit(DocumentModel::HistoryState *state) Q_DECL_OVERRIDE
    {
        bool seenTransition = false;
        for (DocumentModel::StateOrTransition *sot : qAsConst(state->children)) {
            if (DocumentModel::State *s = sot->asState()) {
                error(s->xmlLocation, QStringLiteral("history state cannot have substates"));
            } else if (DocumentModel::Transition *t = sot->asTransition()) {
                if (seenTransition) {
                    error(t->xmlLocation, QStringLiteral("history state can only have one transition"));
                } else {
                    seenTransition = true;
                    m_parentNodes.append(state);
                    t->accept(this);
                    m_parentNodes.removeLast();
                }
            }
        }

        return false;
    }

    bool visit(DocumentModel::Send *node) Q_DECL_OVERRIDE
    {
        checkEvent(node->event, node->xmlLocation, ForbidWildCards);
        checkExpr(node->xmlLocation, QStringLiteral("send"), QStringLiteral("eventexpr"), node->eventexpr);
        return true;
    }

    void visit(DocumentModel::Cancel *node) Q_DECL_OVERRIDE
    {
        checkExpr(node->xmlLocation, QStringLiteral("cancel"), QStringLiteral("sendidexpr"), node->sendidexpr);
    }

    bool visit(DocumentModel::DoneData *node) Q_DECL_OVERRIDE
    {
        checkExpr(node->xmlLocation, QStringLiteral("donedata"), QStringLiteral("expr"), node->expr);
        return false;
    }

    bool visit(DocumentModel::Invoke *node) Q_DECL_OVERRIDE
    {
        if (!node->srcexpr.isEmpty())
            return false;

        if (node->content.isNull()) {
            error(node->xmlLocation, QStringLiteral("no valid content found in <invoke> tag"));
        } else {
            ScxmlVerifier subVerifier(m_errorHandler);
            m_hasErrors = !subVerifier.verify(node->content.data());
        }
        return false;
    }

private:
    enum TokenType {
        XmlNCName,
        XmlNmtoken,
    };

    static bool isValidToken(const QString &id, TokenType tokenType)
    {
        Q_ASSERT(!id.isEmpty());
        int i = 0;
        if (tokenType == XmlNCName) {
            const QChar c = id.at(i++);
            if (!isLetter(c) && c != QLatin1Char('_'))
                return false;
        }
        for (int ei = id.length(); i != ei; ++i) {
            const QChar c = id.at(i);
            if (isLetter(c) || c.isDigit() || c == QLatin1Char('.') || c == QLatin1Char('-')
                    || c == QLatin1Char('_') || isNameTail(c))
                continue;
            else if (tokenType == XmlNmtoken && c == QLatin1Char(':'))
                continue;
            else
                return false;
        }

        return true;
    }

    static bool isLetter(QChar c)
    {
        switch (c.category()) {
        case QChar::Letter_Lowercase:
        case QChar::Letter_Uppercase:
        case QChar::Letter_Other:
        case QChar::Letter_Titlecase:
        case QChar::Number_Letter:
            return true;
        default:
            return false;
        }
    }

    static bool isNameTail(QChar c)
    {
        switch (c.category()) {
        case QChar::Mark_SpacingCombining:
        case QChar::Mark_Enclosing:
        case QChar::Mark_NonSpacing:
        case QChar::Letter_Modifier:
        case QChar::Number_DecimalDigit:
            return true;
        default:
            return false;
        }
    }

    enum WildCardMode {
        ForbidWildCards,
        AllowWildCards
    };

    void checkEvent(const QString &event, const DocumentModel::XmlLocation &loc,
                    WildCardMode wildCardMode)
    {
        if (event.isEmpty())
            return;

        if (!isValidEvent(event, wildCardMode)) {
            error(loc, QStringLiteral("'%1' is not a valid event").arg(event));
        }
    }

    static bool isValidEvent(const QString &event, WildCardMode wildCardMode)
    {
        if (event.isEmpty())
            return false;

        if (wildCardMode == AllowWildCards && event == QLatin1String(".*"))
            return true;

        const QStringList parts = event.split(QLatin1Char('.'));

        for (const QString &part : parts) {
            if (part.isEmpty())
                return false;

            if (wildCardMode == AllowWildCards && part.length() == 1
                    && part.at(0) == QLatin1Char('*')) {
                continue;
            }

            for (int i = 0, ei = part.length(); i != ei; ++i) {
                const QChar c = part.at(i);
                if (!isLetter(c) && !c.isDigit() && c != QLatin1Char('-') && c != QLatin1Char('_')
                        && c != QLatin1Char(':')) {
                    return false;
                }
            }
        }

        return true;
    }

    static const QVector<DocumentModel::StateOrTransition *> &allChildrenOfContainer(
            DocumentModel::StateContainer *container)
    {
        if (auto state = container->asState())
            return state->children;
        else if (auto scxml = container->asScxml())
            return scxml->children;
        else
            Q_UNREACHABLE();
    }

    static DocumentModel::AbstractState *firstAbstractState(DocumentModel::StateContainer *container)
    {
        const auto &allChildren = allChildrenOfContainer(container);

        QVector<DocumentModel::AbstractState *> childStates;
        for (DocumentModel::StateOrTransition *child : qAsConst(allChildren)) {
            if (DocumentModel::State *s = child->asState())
                return s;
            else if (DocumentModel::HistoryState *h = child->asHistoryState())
                return h;
        }
        return nullptr;
    }

    static QVector<DocumentModel::AbstractState *> allAbstractStates(
            DocumentModel::StateContainer *container)
    {
        const auto &allChildren = allChildrenOfContainer(container);

        QVector<DocumentModel::AbstractState *> childStates;
        for (DocumentModel::StateOrTransition *child : qAsConst(allChildren)) {
            if (DocumentModel::State *s = child->asState())
                childStates.append(s);
            else if (DocumentModel::HistoryState *h = child->asHistoryState())
                childStates.append(h);
        }
        return childStates;
    }

    DocumentModel::Transition *createInitialTransition(
            const QVector<DocumentModel::AbstractState *> &states)
    {
        auto *newTransition = m_doc->newTransition(nullptr, DocumentModel::XmlLocation(-1, -1));
        newTransition->type = DocumentModel::Transition::Synthetic;
        for (auto *s : states) {
            newTransition->targets.append(s->id);
        }

        newTransition->targetStates = states;
        return newTransition;
    }

    void checkExpr(const DocumentModel::XmlLocation &loc, const QString &tag, const QString &attrName, const QString &attrValue)
    {
        if (m_doc->root->dataModel == DocumentModel::Scxml::NullDataModel && !attrValue.isEmpty()) {
            error(loc, QStringLiteral(
                      "%1 in <%2> cannot be used with data model 'null'").arg(attrName, tag));
        }
    }

    void error(const DocumentModel::XmlLocation &location, const QString &message)
    {
        m_hasErrors = true;
        if (m_errorHandler)
            m_errorHandler(location, message);
    }

private:
    std::function<void (const DocumentModel::XmlLocation &, const QString &)> m_errorHandler;
    DocumentModel::ScxmlDocument *m_doc;
    bool m_hasErrors;
    QHash<QString, DocumentModel::AbstractState *> m_stateById;
    QVector<DocumentModel::Node *> m_parentNodes;
};

#ifndef BUILD_QSCXMLC
class InvokeDynamicScxmlFactory: public QScxmlInvokableServiceFactory
{
    Q_OBJECT
public:
    InvokeDynamicScxmlFactory(const QScxmlExecutableContent::InvokeInfo &invokeInfo,
                              const QVector<QScxmlExecutableContent::StringId> &namelist,
                              const QVector<QScxmlExecutableContent::ParameterInfo> &params)
        : QScxmlInvokableServiceFactory(invokeInfo, namelist, params)
    {}

    void setContent(const QSharedPointer<DocumentModel::ScxmlDocument> &content)
    { m_content = content; }

    QScxmlInvokableService *invoke(QScxmlStateMachine *child) Q_DECL_OVERRIDE;

private:
    QSharedPointer<DocumentModel::ScxmlDocument> m_content;
};

class DynamicStateMachinePrivate : public QScxmlStateMachinePrivate
{
public:
    DynamicStateMachinePrivate() :
        QScxmlStateMachinePrivate(&QScxmlStateMachine::staticMetaObject) {}
};

class DynamicStateMachine: public QScxmlStateMachine, public QScxmlInternal::GeneratedTableData
{
    Q_DECLARE_PRIVATE(DynamicStateMachine)
    // Manually expanded from Q_OBJECT macro:
public:
    Q_OBJECT_CHECK

    const QMetaObject *metaObject() const Q_DECL_OVERRIDE
    { return d_func()->m_metaObject; }

    int qt_metacall(QMetaObject::Call _c, int _id, void **_a) Q_DECL_OVERRIDE
    {
        Q_D(DynamicStateMachine);
        _id = QScxmlStateMachine::qt_metacall(_c, _id, _a);
        if (_id < 0)
            return _id;
        int ownMethodCount = d->m_metaObject->methodCount() - d->m_metaObject->methodOffset();
        if (_c == QMetaObject::InvokeMetaMethod) {
            if (_id < ownMethodCount)
                qt_static_metacall(this, _c, _id, _a);
            _id -= ownMethodCount;
        } else if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
                   || _c == QMetaObject::ResetProperty || _c == QMetaObject::RegisterPropertyMetaType) {
            qt_static_metacall(this, _c, _id, _a);
            _id -= d->m_metaObject->propertyCount();
        }
        return _id;
    }

private:
    static void qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
    {
        if (_c == QMetaObject::RegisterPropertyMetaType) {
            *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType<bool>();
        } else if (_c == QMetaObject::ReadProperty) {
            DynamicStateMachine *_t = static_cast<DynamicStateMachine *>(_o);
            void *_v = _a[0];
            if (_id >= 0 && _id < _t->m_propertyCount) {
                // getter for the state
                *reinterpret_cast<bool*>(_v) = _t->isActive(_id);
            }
        }
    }
    // end of Q_OBJECT macro

private:
    DynamicStateMachine()
        : QScxmlStateMachine(*new DynamicStateMachinePrivate)
        , m_propertyCount(0)
    {
        // Temporarily wire up the QMetaObject
        Q_D(DynamicStateMachine);
        QMetaObjectBuilder b;
        b.setClassName("DynamicStateMachine");
        b.setSuperClass(&QScxmlStateMachine::staticMetaObject);
        b.setStaticMetacallFunction(qt_static_metacall);
        d->m_metaObject = b.toMetaObject();
    }

    void initDynamicParts(const MetaDataInfo &info)
    {
        Q_D(DynamicStateMachine);
        // Release the temporary QMetaObject.
        Q_ASSERT(d->m_metaObject != &QScxmlStateMachine::staticMetaObject);
        free(const_cast<QMetaObject *>(d->m_metaObject));
        d->m_metaObject = &QScxmlStateMachine::staticMetaObject;

        // Build the real one.
        QMetaObjectBuilder b;
        b.setClassName("DynamicStateMachine");
        b.setSuperClass(&QScxmlStateMachine::staticMetaObject);
        b.setStaticMetacallFunction(qt_static_metacall);

        // signals
        for (const QString &stateName : info.stateNames) {
            auto name = stateName.toUtf8();
            const QByteArray signalName = name + "Changed(bool)";
            QMetaMethodBuilder signalBuilder = b.addSignal(signalName);
            signalBuilder.setParameterNames(init("active"));
        }

        // properties
        int notifier = 0;
        for (const QString &stateName : info.stateNames) {
            QMetaPropertyBuilder prop = b.addProperty(stateName.toUtf8(), "bool", notifier);
            prop.setWritable(false);
            ++m_propertyCount;
            ++notifier;
        }

        // And we're done
        d->m_metaObject = b.toMetaObject();
    }

public:
    ~DynamicStateMachine()
    {
        Q_D(DynamicStateMachine);
        if (d->m_metaObject != &QScxmlStateMachine::staticMetaObject) {
            free(const_cast<QMetaObject *>(d->m_metaObject));
            d->m_metaObject = &QScxmlStateMachine::staticMetaObject;
        }
    }

    QScxmlInvokableServiceFactory *serviceFactory(int id) const Q_DECL_OVERRIDE Q_DECL_FINAL
    { return m_allFactoriesById.at(id); }

    static DynamicStateMachine *build(DocumentModel::ScxmlDocument *doc)
    {
        auto stateMachine = new DynamicStateMachine;
        MetaDataInfo info;
        DataModelInfo dm;
        auto factoryIdCreator = [stateMachine](
                const QScxmlExecutableContent::InvokeInfo &invokeInfo,
                const QVector<QScxmlExecutableContent::StringId> &namelist,
                const QVector<QScxmlExecutableContent::ParameterInfo> &params,
                const QSharedPointer<DocumentModel::ScxmlDocument> &content) -> int {
            auto factory = new InvokeDynamicScxmlFactory(invokeInfo, namelist, params);
            factory->setContent(content);
            stateMachine->m_allFactoriesById.append(factory);
            return stateMachine->m_allFactoriesById.size() - 1;
        };

        GeneratedTableData::build(doc, stateMachine, &info, &dm, factoryIdCreator);
        stateMachine->setTableData(stateMachine);
        stateMachine->initDynamicParts(info);

        return stateMachine;
    }

private:
    static QList<QByteArray> init(const char *s)
    {
#ifdef Q_COMPILER_INITIALIZER_LISTS
        return QList<QByteArray>({ QByteArray::fromRawData(s, int(strlen(s))) });
#else // insane compiler:
        return QList<QByteArray>() << QByteArray::fromRawData(s, int(strlen(s)));
#endif
    }

private:
    QVector<QScxmlInvokableServiceFactory *> m_allFactoriesById;
    int m_propertyCount;
};

inline QScxmlInvokableService *InvokeDynamicScxmlFactory::invoke(
        QScxmlStateMachine *parentStateMachine)
{
    bool ok = true;
    auto srcexpr = calculateSrcexpr(parentStateMachine, invokeInfo().expr, &ok);
    if (!ok)
        return Q_NULLPTR;

    if (!srcexpr.isEmpty())
        return invokeDynamicScxmlService(srcexpr, parentStateMachine, this);

    auto childStateMachine = DynamicStateMachine::build(m_content.data());

    auto dm = QScxmlDataModelPrivate::instantiateDataModel(m_content->root->dataModel);
    dm->setParent(childStateMachine);
    childStateMachine->setDataModel(dm);

    return invokeStaticScxmlService(childStateMachine, parentStateMachine, this);
}
#endif // BUILD_QSCXMLC

} // anonymous namespace

#ifndef BUILD_QSCXMLC
QScxmlScxmlService *invokeDynamicScxmlService(const QString &sourceUrl,
                                              QScxmlStateMachine *parentStateMachine,
                                              QScxmlInvokableServiceFactory *factory)
{
    QScxmlCompiler::Loader *loader = parentStateMachine->loader();

    const QString baseDir = sourceUrl.isEmpty() ? QString() : QFileInfo(sourceUrl).path();
    QStringList errs;
    const QByteArray data = loader->load(sourceUrl, baseDir, &errs);

    if (!errs.isEmpty()) {
        qWarning() << errs;
        return Q_NULLPTR;
    }

    QXmlStreamReader reader(data);
    QScxmlCompiler compiler(&reader);
    compiler.setFileName(sourceUrl);
    compiler.setLoader(parentStateMachine->loader());
    compiler.compile();
    if (!compiler.errors().isEmpty()) {
        const auto errors = compiler.errors();
        for (const QScxmlError &error : errors)
            qWarning() << error.toString();
        return Q_NULLPTR;
    }

    auto mainDoc = QScxmlCompilerPrivate::get(&compiler)->scxmlDocument();
    if (mainDoc == nullptr) {
        Q_ASSERT(!compiler.errors().isEmpty());
        const auto errors = compiler.errors();
        for (const QScxmlError &error : errors)
            qWarning() << error.toString();
        return Q_NULLPTR;
    }

    auto childStateMachine = DynamicStateMachine::build(mainDoc);

    auto dm = QScxmlDataModelPrivate::instantiateDataModel(mainDoc->root->dataModel);
    dm->setParent(childStateMachine);
    childStateMachine->setDataModel(dm);

    return invokeStaticScxmlService(childStateMachine, parentStateMachine, factory);
}
#endif // BUILD_QSCXMLC

/*!
 * \class QScxmlCompiler
 * \brief The QScxmlCompiler class is a compiler for SCXML files.
 * \since 5.7
 * \inmodule QtScxml
 *
 * Parses an \l{SCXML Specification}{SCXML} file and dynamically instantiates a
 * state machine for a successfully parsed SCXML file. If parsing fails, the
 * new state machine cannot start. All errors are returned by
 * QScxmlStateMachine::parseErrors().
 *
 * To load an SCXML file, QScxmlStateMachine::fromFile or QScxmlStateMachine::fromData should be
 * used. Using QScxmlCompiler directly is only needed when the compiler needs to use a custom
 * QScxmlCompiler::Loader.
 */

/*!
 * Creates a new SCXML compiler for the specified \a reader.
 */
QScxmlCompiler::QScxmlCompiler(QXmlStreamReader *reader)
    : d(new QScxmlCompilerPrivate(reader))
{ }

/*!
 * Destroys the SCXML compiler.
 */
QScxmlCompiler::~QScxmlCompiler()
{
    delete d;
}

/*!
 * Returns the file name associated with the current input.
 *
 * \sa setFileName()
 */
QString QScxmlCompiler::fileName() const
{
    return d->fileName();
}

/*!
 * Sets the file name for the current input to \a fileName.
 *
 * The file name is used for error reporting and for resolving relative path URIs.
 *
 * \sa fileName()
 */
void QScxmlCompiler::setFileName(const QString &fileName)
{
    d->setFileName(fileName);
}

/*!
 * Returns the loader that is currently used to resolve and load URIs for the
 * SCXML compiler.
 *
 * \sa setLoader()
 */
QScxmlCompiler::Loader *QScxmlCompiler::loader() const
{
    return d->loader();
}

/*!
 * Sets \a newLoader to be used for resolving and loading URIs for the SCXML
 * compiler.
 *
 * \sa loader()
 */
void QScxmlCompiler::setLoader(QScxmlCompiler::Loader *newLoader)
{
    d->setLoader(newLoader);
}

/*!
 * Parses an SCXML file and creates a new state machine from it.
 *
 * If parsing is successful, the returned state machine can be initialized and started. If
 * parsing fails, QScxmlStateMachine::parseErrors() can be used to retrieve a list of errors.
 */
QScxmlStateMachine *QScxmlCompiler::compile()
{
    d->readDocument();
    if (d->errors().isEmpty()) {
        // Only verify the document if there were no parse errors: if there were any, the document
        // is incomplete and will contain errors for sure. There is no need to heap more errors on
        // top of other errors.
        d->verifyDocument();
    }
    return d->instantiateStateMachine();
}

/*!
 * \internal
 * Instantiates a new state machine from the parsed SCXML.
 *
 * If parsing is successful, the returned state machine can be initialized and started. If
 * parsing fails, QScxmlStateMachine::parseErrors() can be used to retrieve a list of errors.
 *
 * \note The instantiated state machine will not have an associated data model set.
 * \sa QScxmlCompilerPrivate::instantiateDataModel
 */
QScxmlStateMachine *QScxmlCompilerPrivate::instantiateStateMachine() const
{
#ifdef BUILD_QSCXMLC
    return Q_NULLPTR;
#else // BUILD_QSCXMLC
    DocumentModel::ScxmlDocument *doc = scxmlDocument();
    if (doc && doc->root) {
        auto stateMachine = DynamicStateMachine::build(doc);
        instantiateDataModel(stateMachine);
        return stateMachine;
    } else {
        class InvalidStateMachine: public QScxmlStateMachine {
        public:
            InvalidStateMachine() : QScxmlStateMachine(&QScxmlStateMachine::staticMetaObject)
            {}
        };

        auto stateMachine = new InvalidStateMachine;
        QScxmlStateMachinePrivate::get(stateMachine)->parserData()->m_errors = errors();
        instantiateDataModel(stateMachine);
        return stateMachine;
    }
#endif // BUILD_QSCXMLC
}

/*!
 * \internal
 * Instantiates the data model as described in the SCXML file.
 *
 * After instantiation, the \a stateMachine takes ownership of the data model.
 */
void QScxmlCompilerPrivate::instantiateDataModel(QScxmlStateMachine *stateMachine) const
{
#ifdef BUILD_QSCXMLC
    Q_UNUSED(stateMachine)
#else
    auto doc = scxmlDocument();
    auto root = doc ? doc->root : Q_NULLPTR;
    if (root == Q_NULLPTR) {
        qWarning() << "SCXML document has no root element";
    } else {
        QScxmlDataModel *dm = QScxmlDataModelPrivate::instantiateDataModel(root->dataModel);
        QScxmlStateMachinePrivate::get(stateMachine)->parserData()->m_ownedDataModel.reset(dm);
        stateMachine->setDataModel(dm);
        if (dm == Q_NULLPTR)
            qWarning() << "No data-model instantiated";
    }
#endif // BUILD_QSCXMLC
}

/*!
 * Returns the list of parse errors.
 */
QVector<QScxmlError> QScxmlCompiler::errors() const
{
    return d->errors();
}

bool QScxmlCompilerPrivate::ParserState::collectChars() {
    switch (kind) {
    case Content:
    case Data:
    case Script:
        return true;
    default:
        break;
    }
    return false;
}

bool QScxmlCompilerPrivate::ParserState::validChild(ParserState::Kind child) const {
    return validChild(kind, child);
}

bool QScxmlCompilerPrivate::ParserState::validChild(ParserState::Kind parent, ParserState::Kind child)
{
    switch (parent) {
    case ParserState::Scxml:
        switch (child) {
        case ParserState::State:
        case ParserState::Parallel:
        case ParserState::Final:
        case ParserState::DataModel:
        case ParserState::Script:
        case ParserState::Transition:
            return true;
        default:
            break;
        }
        return false;
    case ParserState::State:
        switch (child) {
        case ParserState::OnEntry:
        case ParserState::OnExit:
        case ParserState::Transition:
        case ParserState::Initial:
        case ParserState::State:
        case ParserState::Parallel:
        case ParserState::Final:
        case ParserState::History:
        case ParserState::DataModel:
        case ParserState::Invoke:
            return true;
        default:
            break;
        }
        return false;
    case ParserState::Parallel:
        switch (child) {
        case ParserState::OnEntry:
        case ParserState::OnExit:
        case ParserState::Transition:
        case ParserState::State:
        case ParserState::Parallel:
        case ParserState::History:
        case ParserState::DataModel:
        case ParserState::Invoke:
            return true;
        default:
            break;
        }
        return false;
    case ParserState::Transition:
        return isExecutableContent(child);
    case ParserState::Initial:
        return (child == ParserState::Transition);
    case ParserState::Final:
        switch (child) {
        case ParserState::OnEntry:
        case ParserState::OnExit:
        case ParserState::DoneData:
            return true;
        default:
            break;
        }
        return false;
    case ParserState::OnEntry:
    case ParserState::OnExit:
        return isExecutableContent(child);
    case ParserState::History:
        return child == ParserState::Transition;
    case ParserState::Raise:
        return false;
    case ParserState::If:
        return child == ParserState::ElseIf || child == ParserState::Else
                || isExecutableContent(child);
    case ParserState::ElseIf:
    case ParserState::Else:
        return false;
    case ParserState::Foreach:
        return isExecutableContent(child);
    case ParserState::Log:
        return false;
    case ParserState::DataModel:
        return (child == ParserState::Data);
    case ParserState::Data:
        return false;
    case ParserState::Assign:
        return false;
    case ParserState::DoneData:
    case ParserState::Send:
        return child == ParserState::Content || child == ParserState::Param;
    case ParserState::Content:
        return child == ParserState::Scxml || isExecutableContent(child);
    case ParserState::Param:
    case ParserState::Cancel:
        return false;
    case ParserState::Finalize:
        return isExecutableContent(child);
    case ParserState::Invoke:
        return child == ParserState::Content || child == ParserState::Finalize
                || child == ParserState::Param;
    case ParserState::Script:
    case ParserState::None:
        break;
    }
    return false;
}

bool QScxmlCompilerPrivate::ParserState::isExecutableContent(ParserState::Kind kind) {
    switch (kind) {
    case Raise:
    case Send:
    case Log:
    case Script:
    case Assign:
    case If:
    case Foreach:
    case Cancel:
    case Invoke:
        return true;
    default:
        break;
    }
    return false;
}

QScxmlCompilerPrivate::ParserState::Kind QScxmlCompilerPrivate::ParserState::nameToParserStateKind(const QStringRef &name)
{
    static QMap<QString, ParserState::Kind> nameToKind;
    if (nameToKind.isEmpty()) {
        nameToKind.insert(QLatin1String("scxml"),       Scxml);
        nameToKind.insert(QLatin1String("state"),       State);
        nameToKind.insert(QLatin1String("parallel"),    Parallel);
        nameToKind.insert(QLatin1String("transition"),  Transition);
        nameToKind.insert(QLatin1String("initial"),     Initial);
        nameToKind.insert(QLatin1String("final"),       Final);
        nameToKind.insert(QLatin1String("onentry"),     OnEntry);
        nameToKind.insert(QLatin1String("onexit"),      OnExit);
        nameToKind.insert(QLatin1String("history"),     History);
        nameToKind.insert(QLatin1String("raise"),       Raise);
        nameToKind.insert(QLatin1String("if"),          If);
        nameToKind.insert(QLatin1String("elseif"),      ElseIf);
        nameToKind.insert(QLatin1String("else"),        Else);
        nameToKind.insert(QLatin1String("foreach"),     Foreach);
        nameToKind.insert(QLatin1String("log"),         Log);
        nameToKind.insert(QLatin1String("datamodel"),   DataModel);
        nameToKind.insert(QLatin1String("data"),        Data);
        nameToKind.insert(QLatin1String("assign"),      Assign);
        nameToKind.insert(QLatin1String("donedata"),    DoneData);
        nameToKind.insert(QLatin1String("content"),     Content);
        nameToKind.insert(QLatin1String("param"),       Param);
        nameToKind.insert(QLatin1String("script"),      Script);
        nameToKind.insert(QLatin1String("send"),        Send);
        nameToKind.insert(QLatin1String("cancel"),      Cancel);
        nameToKind.insert(QLatin1String("invoke"),      Invoke);
        nameToKind.insert(QLatin1String("finalize"),    Finalize);
    }
    QMap<QString, ParserState::Kind>::ConstIterator it = nameToKind.constBegin();
    const QMap<QString, ParserState::Kind>::ConstIterator itEnd = nameToKind.constEnd();
    while (it != itEnd) {
        if (it.key() == name)
            return it.value();
        ++it;
    }
    return None;
}

QStringList QScxmlCompilerPrivate::ParserState::requiredAttributes(QScxmlCompilerPrivate::ParserState::Kind kind)
{
    switch (kind) {
    case Scxml:      return QStringList() << QStringLiteral("version");
    case State:      return QStringList();
    case Parallel:   return QStringList();
    case Transition: return QStringList();
    case Initial:    return QStringList();
    case Final:      return QStringList();
    case OnEntry:    return QStringList();
    case OnExit:     return QStringList();
    case History:    return QStringList();
    case Raise:      return QStringList() << QStringLiteral("event");
    case If:         return QStringList() << QStringLiteral("cond");
    case ElseIf:     return QStringList() << QStringLiteral("cond");
    case Else:       return QStringList();
    case Foreach:    return QStringList() << QStringLiteral("array")
                                          << QStringLiteral("item");
    case Log:        return QStringList();
    case DataModel:  return QStringList();
    case Data:       return QStringList() << QStringLiteral("id");
    case Assign:     return QStringList() << QStringLiteral("location");
    case DoneData:   return QStringList();
    case Content:    return QStringList();
    case Param:      return QStringList() << QStringLiteral("name");
    case Script:     return QStringList();
    case Send:       return QStringList();
    case Cancel:     return QStringList();
    case Invoke:     return QStringList();
    case Finalize:   return QStringList();
    default:         return QStringList();
    }
    return QStringList();
}

QStringList QScxmlCompilerPrivate::ParserState::optionalAttributes(QScxmlCompilerPrivate::ParserState::Kind kind)
{
    switch (kind) {
    case Scxml:      return QStringList() << QStringLiteral("initial")
                                          << QStringLiteral("datamodel")
                                          << QStringLiteral("binding")
                                          << QStringLiteral("name");
    case State:      return QStringList() << QStringLiteral("id")
                                          << QStringLiteral("initial");
    case Parallel:   return QStringList() << QStringLiteral("id");
    case Transition: return QStringList() << QStringLiteral("event")
                                          << QStringLiteral("cond")
                                          << QStringLiteral("target")
                                          << QStringLiteral("type");
    case Initial:    return QStringList();
    case Final:      return QStringList() << QStringLiteral("id");
    case OnEntry:    return QStringList();
    case OnExit:     return QStringList();
    case History:    return QStringList() << QStringLiteral("id")
                                          << QStringLiteral("type");
    case Raise:      return QStringList();
    case If:         return QStringList();
    case ElseIf:     return QStringList();
    case Else:       return QStringList();
    case Foreach:    return QStringList() << QStringLiteral("index");
    case Log:        return QStringList() << QStringLiteral("label")
                                          << QStringLiteral("expr");
    case DataModel:  return QStringList();
    case Data:       return QStringList() << QStringLiteral("src")
                                          << QStringLiteral("expr");
    case Assign:     return QStringList() << QStringLiteral("expr");
    case DoneData:   return QStringList();
    case Content:    return QStringList() << QStringLiteral("expr");
    case Param:      return QStringList() << QStringLiteral("expr")
                                          << QStringLiteral("location");
    case Script:     return QStringList() << QStringLiteral("src");
    case Send:       return QStringList() << QStringLiteral("event")
                                          << QStringLiteral("eventexpr")
                                          << QStringLiteral("id")
                                          << QStringLiteral("idlocation")
                                          << QStringLiteral("type")
                                          << QStringLiteral("typeexpr")
                                          << QStringLiteral("namelist")
                                          << QStringLiteral("delay")
                                          << QStringLiteral("delayexpr")
                                          << QStringLiteral("target")
                                          << QStringLiteral("targetexpr");
    case Cancel:     return QStringList() << QStringLiteral("sendid")
                                          << QStringLiteral("sendidexpr");
    case Invoke:     return QStringList() << QStringLiteral("type")
                                          << QStringLiteral("typeexpr")
                                          << QStringLiteral("src")
                                          << QStringLiteral("srcexpr")
                                          << QStringLiteral("id")
                                          << QStringLiteral("idlocation")
                                          << QStringLiteral("namelist")
                                          << QStringLiteral("autoforward");
    case Finalize:   return QStringList();
    default:         return QStringList();
    }
    return QStringList();
}

DocumentModel::Node::~Node()
{
}

DocumentModel::AbstractState *DocumentModel::Node::asAbstractState()
{
    if (State *state = asState())
        return state;
    if (HistoryState *history = asHistoryState())
        return history;
    return Q_NULLPTR;
}

void DocumentModel::DataElement::accept(DocumentModel::NodeVisitor *visitor)
{
    visitor->visit(this);
}

void DocumentModel::Param::accept(DocumentModel::NodeVisitor *visitor)
{
    visitor->visit(this);
}

void DocumentModel::DoneData::accept(DocumentModel::NodeVisitor *visitor)
{
    if (visitor->visit(this)) {
        for (Param *param : qAsConst(params))
            param->accept(visitor);
    }
    visitor->endVisit(this);
}

void DocumentModel::Send::accept(DocumentModel::NodeVisitor *visitor)
{
    if (visitor->visit(this)) {
        visitor->visit(params);
    }
    visitor->endVisit(this);
}

void DocumentModel::Invoke::accept(DocumentModel::NodeVisitor *visitor)
{
    if (visitor->visit(this)) {
        visitor->visit(params);
        visitor->visit(&finalize);
    }
    visitor->endVisit(this);
}

void DocumentModel::Raise::accept(DocumentModel::NodeVisitor *visitor)
{
    visitor->visit(this);
}

void DocumentModel::Log::accept(DocumentModel::NodeVisitor *visitor)
{
    visitor->visit(this);
}

void DocumentModel::Script::accept(DocumentModel::NodeVisitor *visitor)
{
    visitor->visit(this);
}

void DocumentModel::Assign::accept(DocumentModel::NodeVisitor *visitor)
{
    visitor->visit(this);
}

void DocumentModel::If::accept(DocumentModel::NodeVisitor *visitor)
{
    if (visitor->visit(this)) {
        visitor->visit(blocks);
    }
    visitor->endVisit(this);
}

void DocumentModel::Foreach::accept(DocumentModel::NodeVisitor *visitor)
{
    if (visitor->visit(this)) {
        visitor->visit(&block);
    }
    visitor->endVisit(this);
}

void DocumentModel::Cancel::accept(DocumentModel::NodeVisitor *visitor)
{
    visitor->visit(this);
}

void DocumentModel::State::accept(DocumentModel::NodeVisitor *visitor)
{
    if (visitor->visit(this)) {
        visitor->visit(dataElements);
        visitor->visit(children);
        visitor->visit(onEntry);
        visitor->visit(onExit);
        if (doneData)
            doneData->accept(visitor);
        for (Invoke *invoke : qAsConst(invokes))
            invoke->accept(visitor);
    }
    visitor->endVisit(this);
}

void DocumentModel::Transition::accept(DocumentModel::NodeVisitor *visitor)
{
    if (visitor->visit(this)) {
        visitor->visit(&instructionsOnTransition);
    }
    visitor->endVisit(this);
}

void DocumentModel::HistoryState::accept(DocumentModel::NodeVisitor *visitor)
{
    if (visitor->visit(this)) {
        if (Transition *t = defaultConfiguration())
            t->accept(visitor);
    }
    visitor->endVisit(this);
}

void DocumentModel::Scxml::accept(DocumentModel::NodeVisitor *visitor)
{
    if (visitor->visit(this)) {
        visitor->visit(children);
        visitor->visit(dataElements);
        if (script)
            script->accept(visitor);
        visitor->visit(&initialSetup);
    }
    visitor->endVisit(this);
}

DocumentModel::NodeVisitor::~NodeVisitor()
{}

/*!
 * \class QScxmlCompiler::Loader
 * \brief The Loader class is a URI resolver and resource loader for an SCXML compiler.
 * \since 5.8
 * \inmodule QtScxml
 */

/*!
 * Creates a new loader.
 */
QScxmlCompiler::Loader::Loader()
{
}

/*!
 * Destroys the loader.
 */
QScxmlCompiler::Loader::~Loader()
{}

/*!
 * \fn QScxmlCompiler::Loader::load(const QString &name, const QString &baseDir, QStringList *errors)
 * Resolves the URI \a name and loads an SCXML file from the directory
 * specified by \a baseDir. \a errors contains information about the errors that
 * might have occurred.
 *
 * Returns a QByteArray that stores the contents of the file.
 */

QScxmlCompilerPrivate *QScxmlCompilerPrivate::get(QScxmlCompiler *compiler)
{
    return compiler->d;
}

QScxmlCompilerPrivate::QScxmlCompilerPrivate(QXmlStreamReader *reader)
    : m_currentState(Q_NULLPTR)
    , m_loader(&m_defaultLoader)
    , m_reader(reader)
{}

bool QScxmlCompilerPrivate::verifyDocument()
{
    if (!m_doc)
        return false;

    auto handler = [this](const DocumentModel::XmlLocation &location, const QString &msg) {
        this->addError(location, msg);
    };

    if (ScxmlVerifier(handler).verify(m_doc.data()))
        return true;
    else
        return false;
}

DocumentModel::ScxmlDocument *QScxmlCompilerPrivate::scxmlDocument() const
{
    return m_doc && m_errors.isEmpty() ? m_doc.data() : Q_NULLPTR;
}

QString QScxmlCompilerPrivate::fileName() const
{
    return m_fileName;
}

void QScxmlCompilerPrivate::setFileName(const QString &fileName)
{
    m_fileName = fileName;
}

QScxmlCompiler::Loader *QScxmlCompilerPrivate::loader() const
{
    return m_loader;
}

void QScxmlCompilerPrivate::setLoader(QScxmlCompiler::Loader *loader)
{
    m_loader = loader;
}

void QScxmlCompilerPrivate::parseSubDocument(DocumentModel::Invoke *parentInvoke,
                                           QXmlStreamReader *reader,
                                           const QString &fileName)
{
    QScxmlCompiler p(reader);
    p.setFileName(fileName);
    p.setLoader(loader());
    p.d->readDocument();
    parentInvoke->content.reset(p.d->m_doc.take());
    m_doc->allSubDocuments.append(parentInvoke->content.data());
    m_errors.append(p.errors());
}

bool QScxmlCompilerPrivate::parseSubElement(DocumentModel::Invoke *parentInvoke,
                                          QXmlStreamReader *reader,
                                          const QString &fileName)
{
    QScxmlCompiler p(reader);
    p.setFileName(fileName);
    p.setLoader(loader());
    p.d->resetDocument();
    bool ok = p.d->readElement();
    parentInvoke->content.reset(p.d->m_doc.take());
    m_doc->allSubDocuments.append(parentInvoke->content.data());
    m_errors.append(p.errors());
    return ok;
}

bool QScxmlCompilerPrivate::preReadElementScxml()
{
    if (m_doc->root) {
        addError(QLatin1String("Doc root already allocated"));
        return false;
    }
    m_doc->root = new DocumentModel::Scxml(xmlLocation());

    auto scxml = m_doc->root;
    const QXmlStreamAttributes attributes = m_reader->attributes();
    if (attributes.hasAttribute(QStringLiteral("initial"))) {
        const QString initial = attributes.value(QStringLiteral("initial")).toString();
        scxml->initial += initial.split(QChar::Space, QString::SkipEmptyParts);
    }

    const QStringRef datamodel = attributes.value(QLatin1String("datamodel"));
    if (datamodel.isEmpty() || datamodel == QLatin1String("null")) {
        scxml->dataModel = DocumentModel::Scxml::NullDataModel;
    } else if (datamodel == QLatin1String("ecmascript")) {
        scxml->dataModel = DocumentModel::Scxml::JSDataModel;
    } else if (datamodel.startsWith(QLatin1String("cplusplus"))) {
        scxml->dataModel = DocumentModel::Scxml::CppDataModel;
        int firstColon = datamodel.indexOf(QLatin1Char(':'));
        if (firstColon == -1) {
            scxml->cppDataModelClassName = attributes.value(QStringLiteral("name")).toString() + QStringLiteral("DataModel");
            scxml->cppDataModelHeaderName = scxml->cppDataModelClassName + QStringLiteral(".h");
        } else {
            int lastColon = datamodel.lastIndexOf(QLatin1Char(':'));
            if (lastColon == -1) {
                lastColon = datamodel.length();
            } else {
                scxml->cppDataModelHeaderName = datamodel.mid(lastColon + 1).toString();
            }
            scxml->cppDataModelClassName = datamodel.mid(firstColon + 1, lastColon - firstColon - 1).toString();
        }
    } else {
        addError(QStringLiteral("Unsupported data model '%1' in scxml")
                 .arg(datamodel.toString()));
    }
    const QStringRef binding = attributes.value(QLatin1String("binding"));
    if (binding.isEmpty() || binding == QLatin1String("early")) {
        scxml->binding = DocumentModel::Scxml::EarlyBinding;
    } else if (binding == QLatin1String("late")) {
        scxml->binding = DocumentModel::Scxml::LateBinding;
    } else {
        addError(QStringLiteral("Unsupperted binding type '%1'")
                 .arg(binding.toString()));
        return false;
    }
    const QStringRef name = attributes.value(QLatin1String("name"));
    if (!name.isEmpty()) {
        scxml->name = name.toString();
    }
    m_currentState = m_doc->root;
    current().instructionContainer = &m_doc->root->initialSetup;
    return true;
}


bool QScxmlCompilerPrivate::preReadElementState()
{
    const QXmlStreamAttributes attributes = m_reader->attributes();
    auto newState = m_doc->newState(m_currentState, DocumentModel::State::Normal, xmlLocation());
    if (!maybeId(attributes, &newState->id))
        return false;

    if (attributes.hasAttribute(QStringLiteral("initial"))) {
        const QString initial = attributes.value(QStringLiteral("initial")).toString();
        newState->initial += initial.split(QChar::Space, QString::SkipEmptyParts);
    }
    m_currentState = newState;
    return true;
}

bool QScxmlCompilerPrivate::preReadElementParallel()
{
    const QXmlStreamAttributes attributes = m_reader->attributes();
    auto newState = m_doc->newState(m_currentState, DocumentModel::State::Parallel, xmlLocation());
    if (!maybeId(attributes, &newState->id))
        return false;

    m_currentState = newState;
    return true;
}

bool QScxmlCompilerPrivate::preReadElementInitial()
{
    DocumentModel::AbstractState *parent = currentParent();
    if (!parent) {
        addError(QStringLiteral("<initial> found outside a state"));
        return false;
    }

    DocumentModel::State *parentState = parent->asState();
    if (!parentState) {
        addError(QStringLiteral("<initial> found outside a state"));
        return false;
    }

    if (parentState->type == DocumentModel::State::Parallel) {
        addError(QStringLiteral("Explicit initial state for parallel states not supported (only implicitly through the initial states of its substates)"));
        return false;
    }
    return true;
}

bool QScxmlCompilerPrivate::preReadElementTransition()
{
    // Parser stack at this point:
    // <transition>
    // <initial>
    // <state> or <scxml>
    //
    // Or:
    // <transition>
    // <state> or <scxml>

    DocumentModel::Transition *transition = nullptr;
    if (previous().kind == ParserState::Initial) {
        transition = m_doc->newTransition(nullptr, xmlLocation());
        const auto &initialParentState = m_stack.at(m_stack.size() - 3);
        if (initialParentState.kind == ParserState::Scxml) {
            m_currentState->asScxml()->initialTransition = transition;
        } else if (initialParentState.kind == ParserState::State) {
            m_currentState->asState()->initialTransition = transition;
        } else {
            Q_UNREACHABLE();
        }
    } else {
        transition = m_doc->newTransition(m_currentState, xmlLocation());
    }

    const QXmlStreamAttributes attributes = m_reader->attributes();
    transition->events = attributes.value(QLatin1String("event")).toString().split(QLatin1Char(' '), QString::SkipEmptyParts);
    transition->targets = attributes.value(QLatin1String("target")).toString().split(QLatin1Char(' '), QString::SkipEmptyParts);
    if (attributes.hasAttribute(QStringLiteral("cond")))
        transition->condition.reset(new QString(attributes.value(QLatin1String("cond")).toString()));
    QStringRef type = attributes.value(QLatin1String("type"));
    if (type.isEmpty() || type == QLatin1String("external")) {
        transition->type = DocumentModel::Transition::External;
    } else if (type == QLatin1String("internal")) {
        transition->type = DocumentModel::Transition::Internal;
    } else {
        addError(QStringLiteral("invalid transition type '%1', valid values are 'external' and 'internal'").arg(type.toString()));
        return true; // TODO: verify me
    }
    current().instructionContainer = &transition->instructionsOnTransition;
    return true;
}

bool QScxmlCompilerPrivate::preReadElementFinal()
{
    const QXmlStreamAttributes attributes = m_reader->attributes();
    auto newState = m_doc->newState(m_currentState, DocumentModel::State::Final, xmlLocation());
    if (!maybeId(attributes, &newState->id))
        return false;
    m_currentState = newState;
    return true;
}

bool QScxmlCompilerPrivate::preReadElementHistory()
{
    const QXmlStreamAttributes attributes = m_reader->attributes();

    DocumentModel::AbstractState *parent = currentParent();
    if (!parent) {
        addError(QStringLiteral("<history> found outside a state"));
        return false;
    }
    auto newState = m_doc->newHistoryState(parent, xmlLocation());
    if (!maybeId(attributes, &newState->id))
        return false;

    const QStringRef type = attributes.value(QLatin1String("type"));
    if (type.isEmpty() || type == QLatin1String("shallow")) {
        newState->type = DocumentModel::HistoryState::Shallow;
    } else if (type == QLatin1String("deep")) {
        newState->type = DocumentModel::HistoryState::Deep;
    } else {
        addError(QStringLiteral("invalid history type %1, valid values are 'shallow' and 'deep'").arg(type.toString()));
        return false;
    }
    m_currentState = newState;
    return true;
}

bool QScxmlCompilerPrivate::preReadElementOnEntry()
{
    const ParserState::Kind previousKind = previous().kind;
    switch (previousKind) {
    case ParserState::Final:
    case ParserState::State:
    case ParserState::Parallel:
        if (DocumentModel::State *s = m_currentState->asState()) {
            current().instructionContainer = m_doc->newSequence(&s->onEntry);
            break;
        }
        // intentional fall-through
    default:
        addError(QStringLiteral("unexpected container state for onentry"));
        break;
    }
    return true;
}

bool QScxmlCompilerPrivate::preReadElementOnExit()
{
    ParserState::Kind previousKind = previous().kind;
    switch (previousKind) {
    case ParserState::Final:
    case ParserState::State:
    case ParserState::Parallel:
        if (DocumentModel::State *s = m_currentState->asState()) {
            current().instructionContainer = m_doc->newSequence(&s->onExit);
            break;
        }
        // intentional fall-through
    default:
        addError(QStringLiteral("unexpected container state for onexit"));
        break;
    }
    return true;
}

bool QScxmlCompilerPrivate::preReadElementRaise()
{
    const QXmlStreamAttributes attributes = m_reader->attributes();
    auto raise = m_doc->newNode<DocumentModel::Raise>(xmlLocation());
    raise->event = attributes.value(QLatin1String("event")).toString();
    current().instruction = raise;
    return true;
}

bool QScxmlCompilerPrivate::preReadElementIf()
{
    const QXmlStreamAttributes attributes = m_reader->attributes();
    auto *ifI = m_doc->newNode<DocumentModel::If>(xmlLocation());
    current().instruction = ifI;
    ifI->conditions.append(attributes.value(QLatin1String("cond")).toString());
    current().instructionContainer = m_doc->newSequence(&ifI->blocks);
    return true;
}

bool QScxmlCompilerPrivate::preReadElementElseIf()
{
    const QXmlStreamAttributes attributes = m_reader->attributes();

    DocumentModel::If *ifI = lastIf();
    if (!ifI)
        return false;

    ifI->conditions.append(attributes.value(QLatin1String("cond")).toString());
    previous().instructionContainer = m_doc->newSequence(&ifI->blocks);
    return true;
}

bool QScxmlCompilerPrivate::preReadElementElse()
{
    DocumentModel::If *ifI = lastIf();
    if (!ifI)
        return false;

    previous().instructionContainer = m_doc->newSequence(&ifI->blocks);
    return true;
}

bool QScxmlCompilerPrivate::preReadElementForeach()
{
    const QXmlStreamAttributes attributes = m_reader->attributes();
    auto foreachI = m_doc->newNode<DocumentModel::Foreach>(xmlLocation());
    foreachI->array = attributes.value(QLatin1String("array")).toString();
    foreachI->item = attributes.value(QLatin1String("item")).toString();
    foreachI->index = attributes.value(QLatin1String("index")).toString();
    current().instruction = foreachI;
    current().instructionContainer = &foreachI->block;
    return true;
}

bool QScxmlCompilerPrivate::preReadElementLog()
{
    const QXmlStreamAttributes attributes = m_reader->attributes();
    auto logI = m_doc->newNode<DocumentModel::Log>(xmlLocation());
    logI->label = attributes.value(QLatin1String("label")).toString();
    logI->expr = attributes.value(QLatin1String("expr")).toString();
    current().instruction = logI;
    return true;
}

bool QScxmlCompilerPrivate::preReadElementDataModel()
{
    return true;
}

bool QScxmlCompilerPrivate::preReadElementData()
{
    const QXmlStreamAttributes attributes = m_reader->attributes();
    auto data = m_doc->newNode<DocumentModel::DataElement>(xmlLocation());
    data->id = attributes.value(QLatin1String("id")).toString();
    data->src = attributes.value(QLatin1String("src")).toString();
    data->expr = attributes.value(QLatin1String("expr")).toString();
    if (DocumentModel::Scxml *scxml = m_currentState->asScxml()) {
        scxml->dataElements.append(data);
    } else if (DocumentModel::State *state = m_currentState->asState()) {
        state->dataElements.append(data);
    } else {
        Q_UNREACHABLE();
    }
    return true;
}

bool QScxmlCompilerPrivate::preReadElementAssign()
{
    const QXmlStreamAttributes attributes = m_reader->attributes();
    auto assign = m_doc->newNode<DocumentModel::Assign>(xmlLocation());
    assign->location = attributes.value(QLatin1String("location")).toString();
    assign->expr = attributes.value(QLatin1String("expr")).toString();
    current().instruction = assign;
    return true;
}

bool QScxmlCompilerPrivate::preReadElementDoneData()
{
    DocumentModel::State *s = m_currentState->asState();
    if (s && s->type == DocumentModel::State::Final) {
        if (s->doneData) {
            addError(QLatin1String("state can only have one donedata"));
        } else {
            s->doneData = m_doc->newNode<DocumentModel::DoneData>(xmlLocation());
        }
    } else {
        addError(QStringLiteral("donedata can only occur in a final state"));
    }
    return true;
}

bool QScxmlCompilerPrivate::preReadElementContent()
{
    const QXmlStreamAttributes attributes = m_reader->attributes();
    ParserState::Kind previousKind = previous().kind;
    switch (previousKind) {
    case ParserState::DoneData: {
        DocumentModel::State *s = m_currentState->asState();
        Q_ASSERT(s);
        s->doneData->expr = attributes.value(QLatin1String("expr")).toString();
    } break;
    case ParserState::Send: {
        DocumentModel::Send *s = previous().instruction->asSend();
        Q_ASSERT(s);
        s->contentexpr = attributes.value(QLatin1String("expr")).toString();
    } break;
    case ParserState::Invoke: {
        DocumentModel::Invoke *i = previous().instruction->asInvoke();
        Q_ASSERT(i);
        Q_UNUSED(i);
        if (attributes.hasAttribute(QStringLiteral("expr"))) {
            addError(QStringLiteral("expr attribute in content of invoke is not supported"));
            break;
        }
    } break;
    default:
        addError(QStringLiteral("unexpected parent of content %1").arg(previous().kind));
    }
    return true;
}

bool QScxmlCompilerPrivate::preReadElementParam()
{
    const QXmlStreamAttributes attributes = m_reader->attributes();
    auto param = m_doc->newNode<DocumentModel::Param>(xmlLocation());
    param->name = attributes.value(QLatin1String("name")).toString();
    param->expr = attributes.value(QLatin1String("expr")).toString();
    param->location = attributes.value(QLatin1String("location")).toString();

    ParserState::Kind previousKind = previous().kind;
    switch (previousKind) {
    case ParserState::DoneData: {
        DocumentModel::State *s = m_currentState->asState();
        Q_ASSERT(s);
        Q_ASSERT(s->doneData);
        s->doneData->params.append(param);
    } break;
    case ParserState::Send: {
        DocumentModel::Send *s = previous().instruction->asSend();
        Q_ASSERT(s);
        s->params.append(param);
    } break;
    case ParserState::Invoke: {
        DocumentModel::Invoke *i = previous().instruction->asInvoke();
        Q_ASSERT(i);
        i->params.append(param);
    } break;
    default:
        addError(QStringLiteral("unexpected parent of param %1").arg(previous().kind));
    }
    return true;
}

bool QScxmlCompilerPrivate::preReadElementScript()
{
    const QXmlStreamAttributes attributes = m_reader->attributes();
    auto *script = m_doc->newNode<DocumentModel::Script>(xmlLocation());
    script->src = attributes.value(QLatin1String("src")).toString();
    current().instruction = script;
    return true;
}

bool QScxmlCompilerPrivate::preReadElementSend()
{
    const QXmlStreamAttributes attributes = m_reader->attributes();
    auto *send = m_doc->newNode<DocumentModel::Send>(xmlLocation());
    send->event = attributes.value(QLatin1String("event")).toString();
    send->eventexpr = attributes.value(QLatin1String("eventexpr")).toString();
    send->delay = attributes.value(QLatin1String("delay")).toString();
    send->delayexpr = attributes.value(QLatin1String("delayexpr")).toString();
    send->id = attributes.value(QLatin1String("id")).toString();
    send->idLocation = attributes.value(QLatin1String("idlocation")).toString();
    send->type = attributes.value(QLatin1String("type")).toString();
    send->typeexpr = attributes.value(QLatin1String("typeexpr")).toString();
    send->target = attributes.value(QLatin1String("target")).toString();
    send->targetexpr = attributes.value(QLatin1String("targetexpr")).toString();
    if (attributes.hasAttribute(QLatin1String("namelist")))
        send->namelist = attributes.value(QLatin1String("namelist")).toString().split(QLatin1Char(' '), QString::SkipEmptyParts);
    current().instruction = send;
    return true;
}

bool QScxmlCompilerPrivate::preReadElementCancel()
{
    const QXmlStreamAttributes attributes = m_reader->attributes();
    auto *cancel = m_doc->newNode<DocumentModel::Cancel>(xmlLocation());
    cancel->sendid = attributes.value(QLatin1String("sendid")).toString();
    cancel->sendidexpr = attributes.value(QLatin1String("sendidexpr")).toString();
    current().instruction = cancel;
    return true;
}

bool QScxmlCompilerPrivate::preReadElementInvoke()
{
    const QXmlStreamAttributes attributes = m_reader->attributes();
    DocumentModel::State *parentState = m_currentState->asState();
    if (!parentState ||
            (parentState->type != DocumentModel::State::Normal && parentState->type != DocumentModel::State::Parallel)) {
        addError(QStringLiteral("invoke can only occur in <state> or <parallel>"));
        return true; // TODO: verify me
    }
    auto *invoke = m_doc->newNode<DocumentModel::Invoke>(xmlLocation());
    parentState->invokes.append(invoke);
    invoke->src = attributes.value(QLatin1String("src")).toString();
    invoke->srcexpr = attributes.value(QLatin1String("srcexpr")).toString();
    invoke->id = attributes.value(QLatin1String("id")).toString();
    invoke->idLocation = attributes.value(QLatin1String("idlocation")).toString();
    invoke->type = attributes.value(QLatin1String("type")).toString();
    invoke->typeexpr = attributes.value(QLatin1String("typeexpr")).toString();
    QStringRef autoforwardS = attributes.value(QLatin1String("autoforward"));
    if (QStringRef::compare(autoforwardS, QLatin1String("true"), Qt::CaseInsensitive) == 0
            || QStringRef::compare(autoforwardS, QLatin1String("yes"), Qt::CaseInsensitive) == 0
            || QStringRef::compare(autoforwardS, QLatin1String("t"), Qt::CaseInsensitive) == 0
            || QStringRef::compare(autoforwardS, QLatin1String("y"), Qt::CaseInsensitive) == 0
            || autoforwardS == QLatin1String("1"))
        invoke->autoforward = true;
    else
        invoke->autoforward = false;
    invoke->namelist = attributes.value(QLatin1String("namelist")).toString().split(QLatin1Char(' '), QString::SkipEmptyParts);
    current().instruction = invoke;
    return true;
}

bool QScxmlCompilerPrivate::preReadElementFinalize()
{
    auto instr = previous().instruction;
    if (!instr) {
        addError(QStringLiteral("no previous instruction found for <finalize>"));
        return false;
    }
    auto invoke = instr->asInvoke();
    if (!invoke) {
        addError(QStringLiteral("instruction before <finalize> is not <invoke>"));
        return false;
    }
    current().instructionContainer = &invoke->finalize;
    return true;
}

bool QScxmlCompilerPrivate::postReadElementScxml()
{
    return true;
}

bool QScxmlCompilerPrivate::postReadElementState()
{
    currentStateUp();
    return true;
}

bool QScxmlCompilerPrivate::postReadElementParallel()
{
    currentStateUp();
    return true;
}

bool QScxmlCompilerPrivate::postReadElementInitial()
{
    return true;
}

bool QScxmlCompilerPrivate::postReadElementTransition()
{
    return true;
}

bool QScxmlCompilerPrivate::postReadElementFinal()
{
    currentStateUp();
    return true;
}

bool QScxmlCompilerPrivate::postReadElementHistory()
{
    currentStateUp();
    return true;
}

bool QScxmlCompilerPrivate::postReadElementOnEntry()
{
    return true;
}

bool QScxmlCompilerPrivate::postReadElementOnExit()
{
    return true;
}

bool QScxmlCompilerPrivate::postReadElementRaise()
{
    return flushInstruction();
}

bool QScxmlCompilerPrivate::postReadElementIf()
{
    return flushInstruction();
}

bool QScxmlCompilerPrivate::postReadElementElseIf()
{
    return true;
}

bool QScxmlCompilerPrivate::postReadElementElse()
{
    return true;
}

bool QScxmlCompilerPrivate::postReadElementForeach()
{
    return flushInstruction();
}

bool QScxmlCompilerPrivate::postReadElementLog()
{
    return flushInstruction();
}

bool QScxmlCompilerPrivate::postReadElementDataModel()
{
    return true;
}

bool QScxmlCompilerPrivate::postReadElementData()
{
    const ParserState parserState = current();
    DocumentModel::DataElement *data = Q_NULLPTR;
    if (auto state = m_currentState->asState()) {
        data = state->dataElements.last();
    } else if (auto scxml = m_currentState->asScxml()) {
        data = scxml->dataElements.last();
    } else {
        Q_UNREACHABLE();
    }
    if (!data->src.isEmpty() && !data->expr.isEmpty()) {
        addError(QStringLiteral("data element with both 'src' and 'expr' attributes"));
        return false;
    }
    if (!parserState.chars.trimmed().isEmpty()) {
        if (!data->src.isEmpty()) {
            addError(QStringLiteral("data element with both 'src' attribute and CDATA"));
            return false;
        } else if (!data->expr.isEmpty()) {
            addError(QStringLiteral("data element with both 'expr' attribute and CDATA"));
            return false;
        } else {
            // w3c-ecma/test558 - "if a child element of <data> is not a XML,
            // treat it as a string with whitespace normalization"
            // We've modified the test, so that a string is enclosed with quotes.
            data->expr = parserState.chars;
        }
    } else if (!data->src.isEmpty()) {
        if (!m_loader) {
            addError(QStringLiteral("cannot parse a document with external dependencies without a loader"));
        } else {
            bool ok;
            const QByteArray ba = load(data->src, &ok);
            if (!ok) {
                addError(QStringLiteral("failed to load external dependency"));
            } else {
                // w3c-ecma/test558 - "if XML is loaded via "src" attribute,
                // treat it as a string with whitespace normalization"
                // We've enclosed the text in file with quotes.
                data->expr = QString::fromUtf8(ba);
            }
        }
    }
    return true;
}

bool QScxmlCompilerPrivate::postReadElementAssign()
{
    return flushInstruction();
}

bool QScxmlCompilerPrivate::postReadElementDoneData()
{
    return true;
}

bool QScxmlCompilerPrivate::postReadElementContent()
{
    const ParserState parserState = current();
    if (!parserState.chars.trimmed().isEmpty()) {

        switch (previous().kind) {
        case ParserState::DoneData: // see test529
            m_currentState->asState()->doneData->contents = parserState.chars.simplified();
            break;
        case ParserState::Send: // see test179
            previous().instruction->asSend()->content = parserState.chars.simplified();
            break;
        default:
            break;
        }
    }
    return true;
}

bool QScxmlCompilerPrivate::postReadElementParam()
{
    return true;
}

bool QScxmlCompilerPrivate::postReadElementScript()
{
    const ParserState parserState = current();
    DocumentModel::Script *scriptI = parserState.instruction->asScript();
    if (!parserState.chars.trimmed().isEmpty()) {
        scriptI->content = parserState.chars.trimmed();
        if (!scriptI->src.isEmpty())
            addError(QStringLiteral("both src and source content given to script, will ignore external content"));
    } else if (!scriptI->src.isEmpty()) {
        if (!m_loader) {
            addError(QStringLiteral("cannot parse a document with external dependencies without a loader"));
        } else {
            bool ok;
            const QByteArray data = load(scriptI->src, &ok);
            if (!ok) {
                addError(QStringLiteral("failed to load external dependency"));
            } else {
                scriptI->content = QString::fromUtf8(data);
            }
        }
    }
    return flushInstruction();
}

bool QScxmlCompilerPrivate::postReadElementSend()
{
    return flushInstruction();
}

bool QScxmlCompilerPrivate::postReadElementCancel()
{
    return flushInstruction();
}

bool QScxmlCompilerPrivate::postReadElementInvoke()
{
    DocumentModel::Invoke *i = current().instruction->asInvoke();
    const QString fileName = i->src;
    if (!i->content.data()) {
        if (!fileName.isEmpty()) {
            bool ok = true;
            const QByteArray data = load(fileName, &ok);
            if (!ok) {
                addError(QStringLiteral("failed to load external dependency"));
            } else {
                QXmlStreamReader reader(data);
                parseSubDocument(i, &reader, fileName);
            }
        }
    } else if (!fileName.isEmpty()) {
        addError(QStringLiteral("both src and content given to invoke"));
    }

    return true;
}

bool QScxmlCompilerPrivate::postReadElementFinalize()
{
    return true;
}

void QScxmlCompilerPrivate::resetDocument()
{
    m_doc.reset(new DocumentModel::ScxmlDocument(fileName()));
}

bool QScxmlCompilerPrivate::readDocument()
{
    resetDocument();
    m_currentState = m_doc->root;
    for (bool finished = false; !finished && !m_reader->hasError();) {
        switch (m_reader->readNext()) {
        case QXmlStreamReader::StartElement : {
            const QStringRef newTag = m_reader->name();
            const ParserState::Kind newElementKind = ParserState::nameToParserStateKind(newTag);

            auto ns = m_reader->namespaceUri();

            if (ns != scxmlNamespace) {
                m_reader->skipCurrentElement();
            } else if (newElementKind == ParserState::None) {
                addError(QStringLiteral("Unknown element %1").arg(newTag.toString()));
                m_reader->skipCurrentElement();
            } else if (newElementKind == ParserState::Scxml) {
                if (readElement() == false)
                    return false;
            } else {
                addError(QStringLiteral("Unexpected element %1").arg(newTag.toString()));
                m_reader->skipCurrentElement();
            }
        }
            break;
        case QXmlStreamReader::EndElement :
            finished = true;
            break;
        default :
            break;
        }
    }
    if (!m_doc->root) {
        addError(QStringLiteral("Missing root element"));
        return false;
    }

    if (m_reader->hasError() && m_reader->error() != QXmlStreamReader::PrematureEndOfDocumentError) {
        addError(QStringLiteral("Error parsing SCXML file: %1").arg(m_reader->errorString()));
        return false;
    }

    return true;
}

bool QScxmlCompilerPrivate::readElement()
{
    const QStringRef currentTag = m_reader->name();
    const QXmlStreamAttributes attributes = m_reader->attributes();

    const ParserState::Kind elementKind = ParserState::nameToParserStateKind(currentTag);

    if (!checkAttributes(attributes, elementKind))
        return false;

    if (elementKind == ParserState::Scxml && m_doc->root) {
        if (!hasPrevious()) {
            addError(QStringLiteral("misplaced scxml"));
            return false;
        }

        DocumentModel::Invoke *i = previous().instruction->asInvoke();
        if (!i) {
            addError(QStringLiteral("misplaced scxml"));
            return false;
        }

        return parseSubElement(i, m_reader, m_fileName);
    }

    if (elementKind != ParserState::Scxml && !m_stack.count()) {
        addError(QStringLiteral("misplaced %1").arg(currentTag.toString()));
        return false;
    }

    ParserState pNew = ParserState(elementKind);

    m_stack.append(pNew);

    switch (elementKind) {
    case ParserState::Scxml:      if (!preReadElementScxml())      return false; break;
    case ParserState::State:      if (!preReadElementState())      return false; break;
    case ParserState::Parallel:   if (!preReadElementParallel())   return false; break;
    case ParserState::Initial:    if (!preReadElementInitial())    return false; break;
    case ParserState::Transition: if (!preReadElementTransition()) return false; break;
    case ParserState::Final:      if (!preReadElementFinal())      return false; break;
    case ParserState::History:    if (!preReadElementHistory())    return false; break;
    case ParserState::OnEntry:    if (!preReadElementOnEntry())    return false; break;
    case ParserState::OnExit:     if (!preReadElementOnExit())     return false; break;
    case ParserState::Raise:      if (!preReadElementRaise())      return false; break;
    case ParserState::If:         if (!preReadElementIf())         return false; break;
    case ParserState::ElseIf:     if (!preReadElementElseIf())     return false; break;
    case ParserState::Else:       if (!preReadElementElse())       return false; break;
    case ParserState::Foreach:    if (!preReadElementForeach())    return false; break;
    case ParserState::Log:        if (!preReadElementLog())        return false; break;
    case ParserState::DataModel:  if (!preReadElementDataModel())  return false; break;
    case ParserState::Data:       if (!preReadElementData())       return false; break;
    case ParserState::Assign:     if (!preReadElementAssign())     return false; break;
    case ParserState::DoneData:   if (!preReadElementDoneData())   return false; break;
    case ParserState::Content:    if (!preReadElementContent())    return false; break;
    case ParserState::Param:      if (!preReadElementParam())      return false; break;
    case ParserState::Script:     if (!preReadElementScript())     return false; break;
    case ParserState::Send:       if (!preReadElementSend())       return false; break;
    case ParserState::Cancel:     if (!preReadElementCancel())     return false; break;
    case ParserState::Invoke:     if (!preReadElementInvoke())     return false; break;
    case ParserState::Finalize:   if (!preReadElementFinalize())   return false; break;
    default: addError(QStringLiteral("Unknown element %1").arg(currentTag.toString())); return false;
    }

    for (bool finished = false; !finished && !m_reader->hasError();) {
        switch (m_reader->readNext()) {
        case QXmlStreamReader::StartElement : {
            const QStringRef newTag = m_reader->name();
            const ParserState::Kind newElementKind = ParserState::nameToParserStateKind(newTag);

            auto ns = m_reader->namespaceUri();

            if (ns != scxmlNamespace) {
                m_reader->skipCurrentElement();
            } else if (newElementKind == ParserState::None) {
                addError(QStringLiteral("Unknown element %1").arg(newTag.toString()));
                m_reader->skipCurrentElement();
            } else if (pNew.validChild(newElementKind)) {
                if (readElement() == false)
                    return false;
            } else {
                addError(QStringLiteral("Unexpected element %1").arg(newTag.toString()));
                m_reader->skipCurrentElement();
            }
        }
            break;
        case QXmlStreamReader::EndElement :
            finished = true;
            break;
        case QXmlStreamReader::Characters :
            if (m_stack.isEmpty())
                break;
            if (current().collectChars())
                current().chars.append(m_reader->text());
            break;
        default :
            break;
        }
    }

    switch (elementKind) {
    case ParserState::Scxml:      if (!postReadElementScxml())      return false; break;
    case ParserState::State:      if (!postReadElementState())      return false; break;
    case ParserState::Parallel:   if (!postReadElementParallel())   return false; break;
    case ParserState::Initial:    if (!postReadElementInitial())    return false; break;
    case ParserState::Transition: if (!postReadElementTransition()) return false; break;
    case ParserState::Final:      if (!postReadElementFinal())      return false; break;
    case ParserState::History:    if (!postReadElementHistory())    return false; break;
    case ParserState::OnEntry:    if (!postReadElementOnEntry())    return false; break;
    case ParserState::OnExit:     if (!postReadElementOnExit())     return false; break;
    case ParserState::Raise:      if (!postReadElementRaise())      return false; break;
    case ParserState::If:         if (!postReadElementIf())         return false; break;
    case ParserState::ElseIf:     if (!postReadElementElseIf())     return false; break;
    case ParserState::Else:       if (!postReadElementElse())       return false; break;
    case ParserState::Foreach:    if (!postReadElementForeach())    return false; break;
    case ParserState::Log:        if (!postReadElementLog())        return false; break;
    case ParserState::DataModel:  if (!postReadElementDataModel())  return false; break;
    case ParserState::Data:       if (!postReadElementData())       return false; break;
    case ParserState::Assign:     if (!postReadElementAssign())     return false; break;
    case ParserState::DoneData:   if (!postReadElementDoneData())   return false; break;
    case ParserState::Content:    if (!postReadElementContent())    return false; break;
    case ParserState::Param:      if (!postReadElementParam())      return false; break;
    case ParserState::Script:     if (!postReadElementScript())     return false; break;
    case ParserState::Send:       if (!postReadElementSend())       return false; break;
    case ParserState::Cancel:     if (!postReadElementCancel())     return false; break;
    case ParserState::Invoke:     if (!postReadElementInvoke())     return false; break;
    case ParserState::Finalize:   if (!postReadElementFinalize())   return false; break;
    default: break;
    }

    m_stack.removeLast();

    if (m_reader->hasError()/* && m_reader->error() != QXmlStreamReader::PrematureEndOfDocumentError*/) {
        addError(QStringLiteral("Error parsing SCXML file: %1").arg(m_reader->errorString()));
        return false;
    }

    return true;
}

void QScxmlCompilerPrivate::currentStateUp()
{
    Q_ASSERT(m_currentState->parent);
    m_currentState = m_currentState->parent;
}

bool QScxmlCompilerPrivate::flushInstruction()
{
    if (!hasPrevious()) {
        addError(QStringLiteral("missing instructionContainer"));
        return false;
    }
    DocumentModel::InstructionSequence *instructions = previous().instructionContainer;
    if (!instructions) {
        addError(QStringLiteral("got executable content within an element that did not set instructionContainer"));
        return false;
    }
    instructions->append(current().instruction);
    return true;
}


QByteArray QScxmlCompilerPrivate::load(const QString &name, bool *ok)
{
    QStringList errs;
    const QByteArray result = m_loader->load(name, m_fileName.isEmpty() ?
                              QString() : QFileInfo(m_fileName).path(), &errs);
    for (const QString &err : errs)
        addError(err);

    *ok = errs.isEmpty();

    return result;
}

QVector<QScxmlError> QScxmlCompilerPrivate::errors() const
{
    return m_errors;
}

void QScxmlCompilerPrivate::addError(const QString &msg)
{
    m_errors.append(QScxmlError(m_fileName, m_reader->lineNumber(), m_reader->columnNumber(), msg));
}

void QScxmlCompilerPrivate::addError(const DocumentModel::XmlLocation &location, const QString &msg)
{
    m_errors.append(QScxmlError(m_fileName, location.line, location.column, msg));
}

DocumentModel::AbstractState *QScxmlCompilerPrivate::currentParent() const
{
    return m_currentState ? m_currentState->asAbstractState() : Q_NULLPTR;
}

DocumentModel::XmlLocation QScxmlCompilerPrivate::xmlLocation() const
{
    return DocumentModel::XmlLocation(m_reader->lineNumber(), m_reader->columnNumber());
}

bool QScxmlCompilerPrivate::maybeId(const QXmlStreamAttributes &attributes, QString *id)
{
    Q_ASSERT(id);
    QString idStr = attributes.value(QLatin1String("id")).toString();
    if (!idStr.isEmpty()) {
        if (m_allIds.contains(idStr)) {
            addError(xmlLocation(), QStringLiteral("duplicate id '%1'").arg(idStr));
        } else {
            m_allIds.insert(idStr);
            *id = idStr;
        }
    }
    return true;
}

DocumentModel::If *QScxmlCompilerPrivate::lastIf()
{
    if (!hasPrevious()) {
        addError(QStringLiteral("No previous instruction found for else block"));
        return Q_NULLPTR;
    }

    DocumentModel::Instruction *lastI = previous().instruction;
    if (!lastI) {
        addError(QStringLiteral("No previous instruction found for else block"));
        return Q_NULLPTR;
    }
    DocumentModel::If *ifI = lastI->asIf();
    if (!ifI) {
        addError(QStringLiteral("Previous instruction for else block is not an 'if'"));
        return Q_NULLPTR;
    }
    return ifI;
}

QScxmlCompilerPrivate::ParserState &QScxmlCompilerPrivate::current()
{
    return m_stack.last();
}

QScxmlCompilerPrivate::ParserState &QScxmlCompilerPrivate::previous()
{
    return m_stack[m_stack.count() - 2];
}

bool QScxmlCompilerPrivate::hasPrevious() const
{
    return m_stack.count() > 1;
}

bool QScxmlCompilerPrivate::checkAttributes(const QXmlStreamAttributes &attributes,
                                          QScxmlCompilerPrivate::ParserState::Kind kind)
{
    return checkAttributes(attributes,
                           ParserState::requiredAttributes(kind),
                           ParserState::optionalAttributes(kind));
}

bool QScxmlCompilerPrivate::checkAttributes(const QXmlStreamAttributes &attributes,
                                          const QStringList &requiredNames,
                                          const QStringList &optionalNames)
{
    QStringList required = requiredNames;
    for (const QXmlStreamAttribute &attribute : attributes) {
        const QStringRef ns = attribute.namespaceUri();
        if (!ns.isEmpty() && ns != scxmlNamespace && ns != qtScxmlNamespace)
            continue;

        const QString name = attribute.name().toString();
        if (!required.removeOne(name) && !optionalNames.contains(name)) {
            addError(QStringLiteral("Unexpected attribute '%1'").arg(name));
            return false;
        }
    }
    if (!required.isEmpty()) {
        addError(QStringLiteral("Missing required attributes: '%1'")
                 .arg(required.join(QLatin1String("', '"))));
        return false;
    }
    return true;
}

QScxmlCompilerPrivate::DefaultLoader::DefaultLoader()
    : Loader()
{}

QByteArray QScxmlCompilerPrivate::DefaultLoader::load(const QString &name, const QString &baseDir, QStringList *errors)
{
    QStringList errs;
    QByteArray contents;
#ifdef BUILD_QSCXMLC
    QString cleanName = name;
    if (name.startsWith(QStringLiteral("file:")))
        cleanName = name.mid(5);
    QFileInfo fInfo(cleanName);
#else
    const QUrl url(name);
    if (!url.isLocalFile() && !url.isRelative())
        errs << QStringLiteral("src attribute is not a local file (%1)").arg(name);
    QFileInfo fInfo = url.isLocalFile() ? url.toLocalFile() : name;
#endif // BUILD_QSCXMLC
    if (fInfo.isRelative())
        fInfo = QFileInfo(QDir(baseDir).filePath(fInfo.filePath()));

    if (!fInfo.exists()) {
        errs << QStringLiteral("src attribute resolves to non existing file (%1)").arg(fInfo.filePath());
    } else {
        QFile f(fInfo.filePath());
        if (f.open(QFile::ReadOnly))
            contents = f.readAll();
        else
            errs << QStringLiteral("Failure opening file %1: %2")
                      .arg(fInfo.filePath(), f.errorString());
    }
    if (errors)
        *errors = errs;

    return contents;
}

QScxmlCompilerPrivate::ParserState::ParserState(QScxmlCompilerPrivate::ParserState::Kind someKind)
    : kind(someKind)
    , instruction(0)
    , instructionContainer(0)
{}

QT_END_NAMESPACE

#ifndef BUILD_QSCXMLC
#include "qscxmlcompiler.moc"
#endif
