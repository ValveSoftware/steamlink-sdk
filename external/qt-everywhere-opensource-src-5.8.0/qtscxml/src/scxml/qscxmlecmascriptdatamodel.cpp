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
#include "qscxmlecmascriptdatamodel.h"
#include "qscxmlecmascriptplatformproperties_p.h"
#include "qscxmlexecutablecontent_p.h"
#include "qscxmlstatemachine_p.h"
#include "qscxmldatamodel_p.h"

#include <QJSEngine>
#include <QJsonDocument>
#include <QtQml/private/qjsvalue_p.h>
#include <QtQml/private/qv4scopedvalue_p.h>

#include <functional>

QT_BEGIN_NAMESPACE

using namespace QScxmlExecutableContent;

typedef std::function<QString (bool *)> ToStringEvaluator;
typedef std::function<bool (bool *)> ToBoolEvaluator;
typedef std::function<QVariant (bool *)> ToVariantEvaluator;
typedef std::function<void (bool *)> ToVoidEvaluator;
typedef std::function<bool (bool *, std::function<bool ()>)> ForeachEvaluator;

class QScxmlEcmaScriptDataModelPrivate : public QScxmlDataModelPrivate
{
    Q_DECLARE_PUBLIC(QScxmlEcmaScriptDataModel)
public:
    QScxmlEcmaScriptDataModelPrivate()
        : jsEngine(Q_NULLPTR)
    {}

    QString evalStr(const QString &expr, const QString &context, bool *ok)
    {
        QString script = QStringLiteral("(%1).toString()").arg(expr);
        QJSValue v = eval(script, context, ok);
        if (*ok)
            return v.toString();
        else
            return QString();
    }

    bool evalBool(const QString &expr, const QString &context, bool *ok)
    {
        QString script = QStringLiteral("(function(){return !!(%1); })()").arg(expr);
        QJSValue v = eval(script, context, ok);
        if (*ok)
            return v.toBool();
        else
            return false;
    }

    QJSValue evalJSValue(const QString &expr, const QString &context, bool *ok)
    {
        assertEngine();

        QString script = QStringLiteral("(function(){'use strict'; return (\n%1\n); })()").arg(expr);
        return eval(script, context, ok);
    }

    QJSValue eval(const QString &script, const QString &context, bool *ok)
    {
        Q_ASSERT(ok);
        QJSEngine *engine = assertEngine();

        // TODO: copy QJSEngine::evaluate and handle the case of v4->catchException() "our way"

        QJSValue v = engine->evaluate(QStringLiteral("'use strict'; ") + script, QStringLiteral("<expr>"), 0);
        if (v.isError()) {
            *ok = false;
            submitError(QStringLiteral("error.execution"),
                        QStringLiteral("%1 in %2").arg(v.toString(), context));
            return QJSValue(QJSValue::UndefinedValue);
        } else {
            *ok = true;
            return v;
        }
    }

    void setupDataModel()
    {
        QJSEngine *engine = assertEngine();
        dataModel = engine->globalObject();

        qCDebug(qscxmlLog) << m_stateMachine << "initializing the datamodel";
        setupSystemVariables();
    }

    void setupSystemVariables()
    {
        setReadonlyProperty(&dataModel, QStringLiteral("_sessionid"),
                            m_stateMachine->sessionId());

        setReadonlyProperty(&dataModel, QStringLiteral("_name"), m_stateMachine->name());

        QJSEngine *engine = assertEngine();
        auto scxml = engine->newObject();
        scxml.setProperty(QStringLiteral("location"), QStringLiteral("#_scxml_%1")
                          .arg(m_stateMachine->sessionId()));
        auto ioProcs = engine->newObject();
        setReadonlyProperty(&ioProcs, QStringLiteral("scxml"), scxml);
        setReadonlyProperty(&dataModel, QStringLiteral("_ioprocessors"), ioProcs);

        auto platformVars = QScxmlPlatformProperties::create(engine, m_stateMachine);
        dataModel.setProperty(QStringLiteral("_x"), platformVars->jsValue());

        dataModel.setProperty(QStringLiteral("In"), engine->evaluate(
                                  QStringLiteral("function(id){return _x.inState(id);}")));
    }

    void assignEvent(const QScxmlEvent &event)
    {
        if (event.name().isEmpty())
            return;

        QJSEngine *engine = assertEngine();
        QJSValue _event = engine->newObject();
        QJSValue dataValue = eventDataAsJSValue(event.data());
        _event.setProperty(QStringLiteral("data"), dataValue.isUndefined() ? QJSValue(QJSValue::UndefinedValue)
                                                                           : dataValue);
        _event.setProperty(QStringLiteral("invokeid"), event.invokeId().isEmpty() ? QJSValue(QJSValue::UndefinedValue)
                                                                                  : engine->toScriptValue(event.invokeId()));
        if (!event.originType().isEmpty())
            _event.setProperty(QStringLiteral("origintype"), engine->toScriptValue(event.originType()));
        _event.setProperty(QStringLiteral("origin"), event.origin().isEmpty() ? QJSValue(QJSValue::UndefinedValue)
                                                                              : engine->toScriptValue(event.origin()) );
        _event.setProperty(QStringLiteral("sendid"), event.sendId().isEmpty() ? QJSValue(QJSValue::UndefinedValue)
                                                                              : engine->toScriptValue(event.sendId()));
        _event.setProperty(QStringLiteral("type"), engine->toScriptValue(event.scxmlType()));
        _event.setProperty(QStringLiteral("name"), engine->toScriptValue(event.name()));
        _event.setProperty(QStringLiteral("raw"), QStringLiteral("unsupported")); // See test178
        if (event.isErrorEvent())
            _event.setProperty(QStringLiteral("errorMessage"), event.errorMessage());

        setReadonlyProperty(&dataModel, QStringLiteral("_event"), _event);
    }

    QJSValue eventDataAsJSValue(const QVariant &eventData)
    {
        if (!eventData.isValid()) {
            return QJSValue(QJSValue::UndefinedValue);
        }

        QJSEngine *engine = assertEngine();
        if (eventData.canConvert<QVariantMap>()) {
            auto keyValues = eventData.value<QVariantMap>();
            auto data = engine->newObject();

            for (QVariantMap::const_iterator it = keyValues.begin(), eit = keyValues.end(); it != eit; ++it) {
                data.setProperty(it.key(), engine->toScriptValue(it.value()));
            }

            return data;
        }

        if (eventData == QVariant(QMetaType::VoidStar, 0)) {
            return QJSValue(QJSValue::NullValue);
        }

        QString data = eventData.toString();
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &err);
        if (err.error == QJsonParseError::NoError)
            return engine->toScriptValue(doc.toVariant());
        else
            return engine->toScriptValue(data);
    }

    QJSEngine *assertEngine()
    {
        if (!jsEngine) {
            Q_Q(QScxmlEcmaScriptDataModel);
            setEngine(new QJSEngine(q->stateMachine()));
        }

        return jsEngine;
    }

    QJSEngine *engine() const
    {
        return jsEngine;
    }

    void setEngine(QJSEngine *engine)
    { jsEngine = engine; }

    QString string(StringId id) const
    {
        return m_stateMachine->tableData()->string(id);
    }

    bool hasProperty(const QString &name) const
    { return dataModel.hasProperty(name); }

    QJSValue property(const QString &name) const
    { return dataModel.property(name); }

    bool setProperty(const QString &name, const QJSValue &value, const QString &context)
    {
        QString msg;
        switch (setProperty(&dataModel, name, value)) {
        case SetPropertySucceeded:
            return true;
        case SetReadOnlyPropertyFailed:
            msg = QStringLiteral("cannot assign to read-only property %1 in %2");
            break;
        case SetUnknownPropertyFailed:
            msg = QStringLiteral("cannot assign to unknown propety %1 in %2");
            break;
        case SetPropertyFailedForAnotherReason:
            msg = QStringLiteral("assignment to property %1 failed in %2");
            break;
        default:
            Q_UNREACHABLE();
        }

        submitError(QStringLiteral("error.execution"), msg.arg(name, context));
        return false;
    }

    void submitError(const QString &type, const QString &msg, const QString &sendid = QString())
    {
        QScxmlStateMachinePrivate::get(m_stateMachine)->submitError(type, msg, sendid);
    }

public:
    QStringList initialDataNames;

private: // Uses private API
    static void setReadonlyProperty(QJSValue *object, const QString& name, const QJSValue& value)
    {
        qCDebug(qscxmlLog) << "setting read-only property" << name;
        QV4::ExecutionEngine *engine = QJSValuePrivate::engine(object);
        Q_ASSERT(engine);
        QV4::Scope scope(engine);

        QV4::ScopedObject o(scope, QJSValuePrivate::getValue(object));
        if (!o)
            return;

        if (!QJSValuePrivate::checkEngine(engine, value)) {
            qCWarning(qscxmlLog, "EcmaScriptDataModel::setReadonlyProperty(%s) failed: cannot set value created in a different engine", name.toUtf8().constData());
            return;
        }

        QV4::ScopedString s(scope, engine->newString(name));
        uint idx = s->asArrayIndex();
        if (idx < UINT_MAX) {
            Q_UNIMPLEMENTED();
            return;
        }

        s->makeIdentifier(scope.engine);
        QV4::ScopedValue v(scope, QJSValuePrivate::convertedToValue(engine, value));
        o->defineReadonlyProperty(s, v);
        if (engine->hasException)
            engine->catchException();
    }

    enum SetPropertyResult {
        SetPropertySucceeded,
        SetReadOnlyPropertyFailed,
        SetUnknownPropertyFailed,
        SetPropertyFailedForAnotherReason,
    };

    static SetPropertyResult setProperty(QJSValue *object, const QString& name, const QJSValue& value)
    {
        QV4::ExecutionEngine *engine = QJSValuePrivate::engine(object);
        Q_ASSERT(engine);
        if (engine->hasException)
            return SetPropertyFailedForAnotherReason;

        QV4::Scope scope(engine);
        QV4::ScopedObject o(scope, QJSValuePrivate::getValue(object));
        if (o == Q_NULLPTR) {
            return SetPropertyFailedForAnotherReason;
        }

        QV4::ScopedString s(scope, engine->newString(name));
        uint idx = s->asArrayIndex();
        if (idx < UINT_MAX) {
            Q_UNIMPLEMENTED();
            return SetPropertyFailedForAnotherReason;
        }

        QV4::PropertyAttributes attrs = o->query(s);
        if (attrs.isWritable() || attrs.isEmpty()) {
            QV4::ScopedValue v(scope, QJSValuePrivate::convertedToValue(engine, value));
            o->insertMember(s, v);
            if (engine->hasException) {
                engine->catchException();
                return SetPropertyFailedForAnotherReason;
            } else {
                return SetPropertySucceeded;
            }
        } else {
            return SetReadOnlyPropertyFailed;
        }
    }

private:
    QJSEngine *jsEngine;
    QJSValue dataModel;
};

/*!
 * \class QScxmlEcmaScriptDataModel
 * \brief The QScxmlEcmaScriptDataModel class is the ECMAScript data model for
 * a Qt SCXML state machine.
 * \since 5.7
 * \inmodule QtScxml
 *
 * This class implements the ECMAScript data model as described in
 * \l {SCXML Specification - B.2 The ECMAScript Data Model}. It can be
 * subclassed to perform custom initialization.
 *
 * \sa QScxmlStateMachine QScxmlDataModel
 */

/*!
 * Creates a new ECMAScript data model, with the parent object \a parent.
 */
QScxmlEcmaScriptDataModel::QScxmlEcmaScriptDataModel(QObject *parent)
    : QScxmlDataModel(*(new QScxmlEcmaScriptDataModelPrivate), parent)
{}

/*!
  \reimp
 */
bool QScxmlEcmaScriptDataModel::setup(const QVariantMap &initialDataValues)
{
    Q_D(QScxmlEcmaScriptDataModel);
    d->setupDataModel();

    bool ok = true;
    QJSValue undefined(QJSValue::UndefinedValue); // See B.2.1, and test456.
    int count;
    StringId *names = d->m_stateMachine->tableData()->dataNames(&count);
    for (int i = 0; i < count; ++i) {
        auto name = d->string(names[i]);
        QJSValue v = undefined;
        QVariantMap::const_iterator it = initialDataValues.find(name);
        if (it != initialDataValues.end()) {
            QJSEngine *engine = d->assertEngine();
            v = engine->toScriptValue(it.value());
        }
        if (!d->setProperty(name, v, QStringLiteral("<data>"))) {
            ok = false;
        }
    }
    d->initialDataNames = initialDataValues.keys();

    return ok;
}

/*!
  \reimp
 */
QString QScxmlEcmaScriptDataModel::evaluateToString(QScxmlExecutableContent::EvaluatorId id,
                                                    bool *ok)
{
    Q_D(QScxmlEcmaScriptDataModel);
    const EvaluatorInfo &info = d->m_stateMachine->tableData()->evaluatorInfo(id);

    return d->evalStr(d->string(info.expr), d->string(info.context), ok);
}

/*!
  \reimp
 */
bool QScxmlEcmaScriptDataModel::evaluateToBool(QScxmlExecutableContent::EvaluatorId id,
                                               bool *ok)
{
    Q_D(QScxmlEcmaScriptDataModel);
    const EvaluatorInfo &info = d->m_stateMachine->tableData()->evaluatorInfo(id);

    return d->evalBool(d->string(info.expr), d->string(info.context), ok);
}

/*!
  \reimp
 */
QVariant QScxmlEcmaScriptDataModel::evaluateToVariant(QScxmlExecutableContent::EvaluatorId id,
                                                      bool *ok)
{
    Q_D(QScxmlEcmaScriptDataModel);
    const EvaluatorInfo &info = d->m_stateMachine->tableData()->evaluatorInfo(id);

    return d->evalJSValue(d->string(info.expr), d->string(info.context), ok).toVariant();
}

/*!
  \reimp
 */
void QScxmlEcmaScriptDataModel::evaluateToVoid(QScxmlExecutableContent::EvaluatorId id,
                                               bool *ok)
{
    Q_D(QScxmlEcmaScriptDataModel);
    const EvaluatorInfo &info = d->m_stateMachine->tableData()->evaluatorInfo(id);

    d->eval(d->string(info.expr), d->string(info.context), ok);
}

/*!
  \reimp
 */
void QScxmlEcmaScriptDataModel::evaluateAssignment(QScxmlExecutableContent::EvaluatorId id,
                                                   bool *ok)
{
    Q_D(QScxmlEcmaScriptDataModel);
    Q_ASSERT(ok);

    const AssignmentInfo &info = d->m_stateMachine->tableData()->assignmentInfo(id);

    QString dest = d->string(info.dest);

    if (hasScxmlProperty(dest)) {
        QJSValue v = d->evalJSValue(d->string(info.expr), d->string(info.context), ok);
        if (*ok)
            *ok = d->setProperty(dest, v, d->string(info.context));
    } else {
        *ok = false;
        d->submitError(QStringLiteral("error.execution"),
                       QStringLiteral("%1 in %2 does not exist").arg(dest, d->string(info.context)));
    }
}

/*!
  \reimp
 */
void QScxmlEcmaScriptDataModel::evaluateInitialization(QScxmlExecutableContent::EvaluatorId id,
                                                       bool *ok)
{
    Q_D(QScxmlEcmaScriptDataModel);
    const AssignmentInfo &info = d->m_stateMachine->tableData()->assignmentInfo(id);
    QString dest = d->string(info.dest);
    if (d->initialDataNames.contains(dest)) {
        *ok = true; // silently ignore the <data> tag
        return;
    }

    evaluateAssignment(id, ok);
}

/*!
  \reimp
 */
void QScxmlEcmaScriptDataModel::evaluateForeach(QScxmlExecutableContent::EvaluatorId id, bool *ok,
                                                ForeachLoopBody *body)
{
    Q_D(QScxmlEcmaScriptDataModel);
    Q_ASSERT(ok);
    Q_ASSERT(body);
    const ForeachInfo &info = d->m_stateMachine->tableData()->foreachInfo(id);

    QJSValue jsArray = d->property(d->string(info.array));
    if (!jsArray.isArray()) {
        d->submitError(QStringLiteral("error.execution"), QStringLiteral("invalid array '%1' in %2").arg(d->string(info.array), d->string(info.context)));
        *ok = false;
        return;
    }

    QString item = d->string(info.item);

    QJSEngine *engine = d->assertEngine();
    if (engine->evaluate(QStringLiteral("(function(){var %1 = 0})()").arg(item)).isError()) {
        d->submitError(QStringLiteral("error.execution"), QStringLiteral("invalid item '%1' in %2")
                      .arg(d->string(info.item), d->string(info.context)));
        *ok = false;
        return;
    }

    const int length = jsArray.property(QStringLiteral("length")).toInt();
    QString idx = d->string(info.index);
    QString context = d->string(info.context);
    const bool hasIndex = !idx.isEmpty();

    for (int currentIndex = 0; currentIndex < length; ++currentIndex) {
        QJSValue currentItem = jsArray.property(static_cast<quint32>(currentIndex));
        *ok = d->setProperty(item, currentItem, context);
        if (!*ok)
            return;
        if (hasIndex) {
            *ok = d->setProperty(idx, currentIndex, context);
            if (!*ok)
                return;
        }
        body->run(ok);
        if (!*ok)
            return;
    }
    *ok = true;
}

/*!
 * \reimp
 */
void QScxmlEcmaScriptDataModel::setScxmlEvent(const QScxmlEvent &event)
{
    Q_D(QScxmlEcmaScriptDataModel);
    d->assignEvent(event);
}

/*!
 * \reimp
 */
QVariant QScxmlEcmaScriptDataModel::scxmlProperty(const QString &name) const
{
    Q_D(const QScxmlEcmaScriptDataModel);
    return d->property(name).toVariant();
}

/*!
 * \reimp
 */
bool QScxmlEcmaScriptDataModel::hasScxmlProperty(const QString &name) const
{
    Q_D(const QScxmlEcmaScriptDataModel);
    return d->hasProperty(name);
}

/*!
 * \reimp
 */
bool QScxmlEcmaScriptDataModel::setScxmlProperty(const QString &name, const QVariant &value,
                                                 const QString &context)
{
    Q_D(QScxmlEcmaScriptDataModel);
    Q_ASSERT(hasScxmlProperty(name));

    QJSEngine *engine = d->assertEngine();
    QJSValue v = engine->toScriptValue(
                value.canConvert<QJSValue>() ? value.value<QJSValue>().toVariant() : value);
    return d->setProperty(name, v, context);
}

QT_END_NAMESPACE
