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

#include "qquickpropertychanges_p.h"

#include <private/qqmlopenmetaobject_p.h>
#include <private/qqmlengine_p.h>

#include <qqmlinfo.h>
#include <private/qqmlcustomparser_p.h>
#include <qqmlexpression.h>
#include <private/qqmlbinding_p.h>
#include <qqmlcontext.h>
#include <private/qqmlproperty_p.h>
#include <private/qqmlcontext_p.h>
#include <private/qquickstate_p_p.h>
#include <private/qqmlboundsignal_p.h>

#include <QtCore/qdebug.h>

#include <private/qobject_p.h>

QT_BEGIN_NAMESPACE

/*!
    \qmltype PropertyChanges
    \inqmlmodule QtQuick
    \ingroup qtquick-states
    \brief Describes new property bindings or values for a state.

    PropertyChanges is used to define the property values or bindings in a
    \l State. This enables an item's property values to be changed when it
    \l {Qt Quick States}{changes between states}.

    To create a PropertyChanges object, specify the \l target item whose
    properties are to be modified, and define the new property values or
    bindings. For example:

    \snippet qml/propertychanges.qml import
    \codeline
    \snippet qml/propertychanges.qml 0

    When the mouse is pressed, the \l Rectangle changes to the \e resized
    state. In this state, the PropertyChanges object sets the rectangle's
    color to blue and the \c height value to that of \c container.height.

    Note this automatically binds \c rect.height to \c container.height
    in the \e resized state. If a property binding should not be
    established, and the height should just be set to the value of
    \c container.height at the time of the state change, set the \l explicit
    property to \c true.

    A PropertyChanges object can also override the default signal handler
    for an object to implement a signal handler specific to the new state:

    \qml
    PropertyChanges {
        target: myMouseArea
        onClicked: doSomethingDifferent()
    }
    \endqml

    \note PropertyChanges can be used to change anchor margins, but not other anchor
    values; use AnchorChanges for this instead. Similarly, to change an \l Item's
    \l {Item::}{parent} value, use ParentChange instead.


    \section2 Resetting property values

    The \c undefined value can be used to reset the property value for a state.
    In the following example, when \c myText changes to the \e widerText
    state, its \c width property is reset, giving the text its natural width
    and displaying the whole string on a single line.

    \snippet qml/propertychanges.qml reset


    \section2 Immediate property changes in transitions

    When \l{Animation and Transitions in Qt Quick}{Transitions} are used to animate
    state changes, they animate properties from their values in the current
    state to those defined in the new state (as defined by PropertyChanges
    objects). However, it is sometimes desirable to set a property value
    \e immediately during a \l Transition, without animation; in these cases,
    the PropertyAction type can be used to force an immediate property
    change.

    See the PropertyAction documentation for more details.

    \note The \l{Item::}{visible} and \l{Item::}{enabled} properties of \l Item do not behave
    exactly the same as other properties in PropertyChanges. Since these properties can be
    changed implicitly through their parent's state, they should be set explicitly in all PropertyChanges.
    An item will still not be enabled/visible if one of its parents is not enabled or visible.

    \sa {Qt Quick Examples - Animation#States}{States example}, {Qt Quick States}, {Qt QML}
*/

/*!
    \qmlproperty Object QtQuick::PropertyChanges::target
    This property holds the object which contains the properties to be changed.
*/

class QQuickReplaceSignalHandler : public QQuickStateActionEvent
{
public:
    QQuickReplaceSignalHandler() {}
    ~QQuickReplaceSignalHandler() {}

    virtual EventType type() const { return SignalHandler; }

    QQmlProperty property;
    QQmlBoundSignalExpressionPointer expression;
    QQmlBoundSignalExpressionPointer reverseExpression;
    QQmlBoundSignalExpressionPointer rewindExpression;

    virtual void execute() {
        QQmlPropertyPrivate::setSignalExpression(property, expression);
    }

    virtual bool isReversable() { return true; }
    virtual void reverse() {
        QQmlPropertyPrivate::setSignalExpression(property, reverseExpression);
    }

    virtual void saveOriginals() {
        saveCurrentValues();
        reverseExpression = rewindExpression;
    }

    virtual bool needsCopy() { return true; }
    virtual void copyOriginals(QQuickStateActionEvent *other)
    {
        QQuickReplaceSignalHandler *rsh = static_cast<QQuickReplaceSignalHandler*>(other);
        saveCurrentValues();
        if (rsh == this)
            return;
        reverseExpression = rsh->reverseExpression;
    }

    virtual void rewind() {
        QQmlPropertyPrivate::setSignalExpression(property, rewindExpression);
    }
    virtual void saveCurrentValues() {
        rewindExpression = QQmlPropertyPrivate::signalExpression(property);
    }

    virtual bool override(QQuickStateActionEvent*other) {
        if (other == this)
            return true;
        if (other->type() != type())
            return false;
        if (static_cast<QQuickReplaceSignalHandler*>(other)->property == property)
            return true;
        return false;
    }
};


class QQuickPropertyChangesPrivate : public QQuickStateOperationPrivate
{
    Q_DECLARE_PUBLIC(QQuickPropertyChanges)
public:
    QQuickPropertyChangesPrivate() : decoded(true), restore(true),
                                isExplicit(false) {}

    QPointer<QObject> object;
    QList<const QV4::CompiledData::Binding *> bindings;
    QQmlRefPointer<QV4::CompiledData::CompilationUnit> compilationUnit;

    bool decoded : 1;
    bool restore : 1;
    bool isExplicit : 1;

    void decode();
    void decodeBinding(const QString &propertyPrefix, const QV4::CompiledData::Unit *qmlUnit, const QV4::CompiledData::Binding *binding);

    class ExpressionChange {
    public:
        ExpressionChange(const QString &_name,
                         QQmlBinding::Identifier _id,
                         const QString& _expr,
                         const QUrl &_url,
                         int _line,
                         int _column)
            : name(_name), id(_id), expression(_expr), url(_url), line(_line), column(_column) {}
        QString name;
        QQmlBinding::Identifier id;
        QString expression;
        QUrl url;
        int line;
        int column;
    };

    QList<QPair<QString, QVariant> > properties;
    QList<ExpressionChange> expressions;
    QList<QQuickReplaceSignalHandler*> signalReplacements;

    QQmlProperty property(const QString &);
};

void QQuickPropertyChangesParser::verifyList(const QV4::CompiledData::Unit *qmlUnit, const QV4::CompiledData::Binding *binding)
{
    if (binding->type == QV4::CompiledData::Binding::Type_Object) {
        error(qmlUnit->objectAt(binding->value.objectIndex), QQuickPropertyChanges::tr("PropertyChanges does not support creating state-specific objects."));
        return;
    }

    if (binding->type == QV4::CompiledData::Binding::Type_GroupProperty
        || binding->type == QV4::CompiledData::Binding::Type_AttachedProperty) {
        const QV4::CompiledData::Object *subObj = qmlUnit->objectAt(binding->value.objectIndex);
        const QV4::CompiledData::Binding *subBinding = subObj->bindingTable();
        for (quint32 i = 0; i < subObj->nBindings; ++i, ++subBinding) {
            verifyList(qmlUnit, subBinding);
        }
    }
}

void QQuickPropertyChangesPrivate::decode()
{
    if (decoded)
        return;

    for (const QV4::CompiledData::Binding *binding : qAsConst(bindings))
        decodeBinding(QString(), compilationUnit->data, binding);

    bindings.clear();

    decoded = true;
}

void QQuickPropertyChangesPrivate::decodeBinding(const QString &propertyPrefix, const QV4::CompiledData::Unit *qmlUnit, const QV4::CompiledData::Binding *binding)
{
    Q_Q(QQuickPropertyChanges);

    QString propertyName = propertyPrefix + qmlUnit->stringAt(binding->propertyNameIndex);

    if (binding->type == QV4::CompiledData::Binding::Type_GroupProperty
        || binding->type == QV4::CompiledData::Binding::Type_AttachedProperty) {
        QString pre = propertyName + QLatin1Char('.');
        const QV4::CompiledData::Object *subObj = qmlUnit->objectAt(binding->value.objectIndex);
        const QV4::CompiledData::Binding *subBinding = subObj->bindingTable();
        for (quint32 i = 0; i < subObj->nBindings; ++i, ++subBinding) {
            decodeBinding(pre, qmlUnit, subBinding);
        }
        return;
    }

    QQmlProperty prop = property(propertyName);      //### better way to check for signal property?

    if (prop.type() & QQmlProperty::SignalProperty) {
        QQuickReplaceSignalHandler *handler = new QQuickReplaceSignalHandler;
        handler->property = prop;
        handler->expression.take(new QQmlBoundSignalExpression(object, QQmlPropertyPrivate::get(prop)->signalIndex(),
                                                               QQmlContextData::get(qmlContext(q)), object, compilationUnit->runtimeFunctions.at(binding->value.compiledScriptIndex)));
        signalReplacements << handler;
        return;
    }

    if (binding->type == QV4::CompiledData::Binding::Type_Script) {
        QString expression = binding->valueAsString(qmlUnit);
        QUrl url = QUrl();
        int line = -1;
        int column = -1;

        QQmlData *ddata = QQmlData::get(q);
        if (ddata && ddata->outerContext && !ddata->outerContext->url().isEmpty()) {
            url = ddata->outerContext->url();
            line = ddata->lineNumber;
            column = ddata->columnNumber;
        }

        expressions << ExpressionChange(propertyName, binding->value.compiledScriptIndex, expression, url, line, column);
        return;
    }

    QVariant var;
    switch (binding->type) {
    case QV4::CompiledData::Binding::Type_Script:
        Q_UNREACHABLE();
    case QV4::CompiledData::Binding::Type_Translation:
    case QV4::CompiledData::Binding::Type_TranslationById:
    case QV4::CompiledData::Binding::Type_String:
        var = binding->valueAsString(qmlUnit);
        break;
    case QV4::CompiledData::Binding::Type_Number:
        var = binding->valueAsNumber();
        break;
    case QV4::CompiledData::Binding::Type_Boolean:
        var = binding->valueAsBoolean();
        break;
    default:
        break;
    }

    properties << qMakePair(propertyName, var);
}

void QQuickPropertyChangesParser::verifyBindings(const QV4::CompiledData::Unit *qmlUnit, const QList<const QV4::CompiledData::Binding *> &props)
{
    for (int ii = 0; ii < props.count(); ++ii)
        verifyList(qmlUnit, props.at(ii));
}

void QQuickPropertyChangesParser::applyBindings(QObject *obj, QV4::CompiledData::CompilationUnit *compilationUnit, const QList<const QV4::CompiledData::Binding *> &bindings)
{
    QQuickPropertyChangesPrivate *p =
        static_cast<QQuickPropertyChangesPrivate *>(QObjectPrivate::get(obj));
    p->bindings = bindings;
    p->compilationUnit = compilationUnit;
    p->decoded = false;
}

QQuickPropertyChanges::QQuickPropertyChanges()
: QQuickStateOperation(*(new QQuickPropertyChangesPrivate))
{
}

QQuickPropertyChanges::~QQuickPropertyChanges()
{
    Q_D(QQuickPropertyChanges);
    for(int ii = 0; ii < d->signalReplacements.count(); ++ii)
        delete d->signalReplacements.at(ii);
}

QObject *QQuickPropertyChanges::object() const
{
    Q_D(const QQuickPropertyChanges);
    return d->object;
}

void QQuickPropertyChanges::setObject(QObject *o)
{
    Q_D(QQuickPropertyChanges);
    d->object = o;
}

/*!
    \qmlproperty bool QtQuick::PropertyChanges::restoreEntryValues

    This property holds whether the previous values should be restored when
    leaving the state.

    The default value is \c true. Setting this value to \c false creates a
    temporary state that has permanent effects on property values.
*/
bool QQuickPropertyChanges::restoreEntryValues() const
{
    Q_D(const QQuickPropertyChanges);
    return d->restore;
}

void QQuickPropertyChanges::setRestoreEntryValues(bool v)
{
    Q_D(QQuickPropertyChanges);
    d->restore = v;
}

QQmlProperty
QQuickPropertyChangesPrivate::property(const QString &property)
{
    Q_Q(QQuickPropertyChanges);
    QQmlProperty prop(object, property, qmlContext(q));
    if (!prop.isValid()) {
        qmlInfo(q) << QQuickPropertyChanges::tr("Cannot assign to non-existent property \"%1\"").arg(property);
        return QQmlProperty();
    } else if (!(prop.type() & QQmlProperty::SignalProperty) && !prop.isWritable()) {
        qmlInfo(q) << QQuickPropertyChanges::tr("Cannot assign to read-only property \"%1\"").arg(property);
        return QQmlProperty();
    }
    return prop;
}

QQuickPropertyChanges::ActionList QQuickPropertyChanges::actions()
{
    Q_D(QQuickPropertyChanges);

    d->decode();

    ActionList list;

    for (int ii = 0; ii < d->properties.count(); ++ii) {

        QQuickStateAction a(d->object, d->properties.at(ii).first,
                 qmlContext(this), d->properties.at(ii).second);

        if (a.property.isValid()) {
            a.restore = restoreEntryValues();
            list << a;
        }
    }

    for (int ii = 0; ii < d->signalReplacements.count(); ++ii) {

        QQuickReplaceSignalHandler *handler = d->signalReplacements.at(ii);

        if (handler->property.isValid()) {
            QQuickStateAction a;
            a.event = handler;
            list << a;
        }
    }

    for (int ii = 0; ii < d->expressions.count(); ++ii) {

        QQuickPropertyChangesPrivate::ExpressionChange e = d->expressions.at(ii);
        const QString &property = e.name;
        QQmlProperty prop = d->property(property);

        if (prop.isValid()) {
            QQuickStateAction a;
            a.restore = restoreEntryValues();
            a.property = prop;
            a.fromValue = a.property.read();
            a.specifiedObject = d->object;
            a.specifiedProperty = property;

            QQmlContextData *context = QQmlContextData::get(qmlContext(this));

            QQmlBinding *newBinding = 0;
            if (e.id != QQmlBinding::Invalid) {
                QV4::Scope scope(QQmlEnginePrivate::getV4Engine(qmlEngine(this)));
                QV4::ScopedValue function(scope, QV4::FunctionObject::createQmlFunction(context, object(), d->compilationUnit->runtimeFunctions.at(e.id)));
                newBinding = QQmlBinding::create(&QQmlPropertyPrivate::get(prop)->core,
                                                 function, object(), context);
            }
//            QQmlBinding *newBinding = e.id != QQmlBinding::Invalid ? QQmlBinding::createBinding(e.id, object(), qmlContext(this)) : 0;
            if (!newBinding)
                newBinding = QQmlBinding::create(&QQmlPropertyPrivate::get(prop)->core,
                                                 e.expression, object(), context, e.url.toString(), e.line, e.column);

            if (d->isExplicit) {
                // in this case, we don't want to assign a binding, per se,
                // so we evaluate the expression and assign the result.
                // XXX TODO: add a static QQmlJavaScriptExpression::evaluate(QString)
                // so that we can avoid creating then destroying the binding in this case.
                a.toValue = newBinding->evaluate();
                delete newBinding;
            } else {
                newBinding->setTarget(prop);
                a.toBinding = newBinding;
                a.deletableToBinding = true;
            }

            list << a;
        }
    }

    return list;
}

/*!
    \qmlproperty bool QtQuick::PropertyChanges::explicit

    If explicit is set to true, any potential bindings will be interpreted as
    once-off assignments that occur when the state is entered.

    In the following example, the addition of explicit prevents \c myItem.width from
    being bound to \c parent.width. Instead, it is assigned the value of \c parent.width
    at the time of the state change.
    \qml
    PropertyChanges {
        target: myItem
        explicit: true
        width: parent.width
    }
    \endqml

    By default, explicit is false.
*/
bool QQuickPropertyChanges::isExplicit() const
{
    Q_D(const QQuickPropertyChanges);
    return d->isExplicit;
}

void QQuickPropertyChanges::setIsExplicit(bool e)
{
    Q_D(QQuickPropertyChanges);
    d->isExplicit = e;
}

bool QQuickPropertyChanges::containsValue(const QString &name) const
{
    Q_D(const QQuickPropertyChanges);
    typedef QPair<QString, QVariant> PropertyEntry;

    QListIterator<PropertyEntry> propertyIterator(d->properties);
    while (propertyIterator.hasNext()) {
        const PropertyEntry &entry = propertyIterator.next();
        if (entry.first == name) {
            return true;
        }
    }

    return false;
}

bool QQuickPropertyChanges::containsExpression(const QString &name) const
{
    Q_D(const QQuickPropertyChanges);
    typedef QQuickPropertyChangesPrivate::ExpressionChange ExpressionEntry;

    QListIterator<ExpressionEntry> expressionIterator(d->expressions);
    while (expressionIterator.hasNext()) {
        const ExpressionEntry &entry = expressionIterator.next();
        if (entry.name == name) {
            return true;
        }
    }

    return false;
}

bool QQuickPropertyChanges::containsProperty(const QString &name) const
{
    return containsValue(name) || containsExpression(name);
}

void QQuickPropertyChanges::changeValue(const QString &name, const QVariant &value)
{
    Q_D(QQuickPropertyChanges);
    typedef QPair<QString, QVariant> PropertyEntry;
    typedef QQuickPropertyChangesPrivate::ExpressionChange ExpressionEntry;

    QMutableListIterator<ExpressionEntry> expressionIterator(d->expressions);
    while (expressionIterator.hasNext()) {
        const ExpressionEntry &entry = expressionIterator.next();
        if (entry.name == name) {
            expressionIterator.remove();
            if (state() && state()->isStateActive()) {
                QQmlPropertyPrivate::removeBinding(d->property(name));
                d->property(name).write(value);
            }

            d->properties.append(PropertyEntry(name, value));
            return;
        }
    }

    QMutableListIterator<PropertyEntry> propertyIterator(d->properties);
    while (propertyIterator.hasNext()) {
        PropertyEntry &entry = propertyIterator.next();
        if (entry.first == name) {
            entry.second = value;
            if (state() && state()->isStateActive())
                d->property(name).write(value);
            return;
        }
    }

    QQuickStateAction action;
    action.restore = restoreEntryValues();
    action.property = d->property(name);
    action.fromValue = action.property.read();
    action.specifiedObject = object();
    action.specifiedProperty = name;
    action.toValue = value;

    propertyIterator.insert(PropertyEntry(name, value));
    if (state() && state()->isStateActive()) {
        state()->addEntryToRevertList(action);
        QQmlAbstractBinding *oldBinding = QQmlPropertyPrivate::binding(action.property);
        if (oldBinding)
            oldBinding->setEnabled(false, QQmlPropertyData::DontRemoveBinding | QQmlPropertyData::BypassInterceptor);
        d->property(name).write(value);
    }
}

void QQuickPropertyChanges::changeExpression(const QString &name, const QString &expression)
{
    Q_D(QQuickPropertyChanges);
    typedef QPair<QString, QVariant> PropertyEntry;
    typedef QQuickPropertyChangesPrivate::ExpressionChange ExpressionEntry;

    bool hadValue = false;

    QMutableListIterator<PropertyEntry> propertyIterator(d->properties);
    while (propertyIterator.hasNext()) {
        PropertyEntry &entry = propertyIterator.next();
        if (entry.first == name) {
            propertyIterator.remove();
            hadValue = true;
            break;
        }
    }

    QMutableListIterator<ExpressionEntry> expressionIterator(d->expressions);
    while (expressionIterator.hasNext()) {
        ExpressionEntry &entry = expressionIterator.next();
        if (entry.name == name) {
            entry.expression = expression;
            if (state() && state()->isStateActive()) {
                auto prop = d->property(name);
                QQmlBinding *newBinding = QQmlBinding::create(
                            &QQmlPropertyPrivate::get(prop)->core, expression, object(),
                            qmlContext(this));
                newBinding->setTarget(prop);
                QQmlPropertyPrivate::setBinding(newBinding, QQmlPropertyPrivate::None, QQmlPropertyData::DontRemoveBinding | QQmlPropertyData::BypassInterceptor);
            }
            return;
        }
    }

    // adding a new expression.
    expressionIterator.insert(ExpressionEntry(name, QQmlBinding::Invalid, expression, QUrl(), -1, -1));

    if (state() && state()->isStateActive()) {
        if (hadValue) {
            QQmlAbstractBinding *oldBinding = QQmlPropertyPrivate::binding(d->property(name));
            if (oldBinding) {
                oldBinding->setEnabled(false, QQmlPropertyData::DontRemoveBinding | QQmlPropertyData::BypassInterceptor);
                state()->changeBindingInRevertList(object(), name, oldBinding);
            }

            auto prop = d->property(name);
            QQmlBinding *newBinding = QQmlBinding::create(
                        &QQmlPropertyPrivate::get(prop)->core, expression, object(),
                        qmlContext(this));
            newBinding->setTarget(prop);
            QQmlPropertyPrivate::setBinding(newBinding, QQmlPropertyPrivate::None, QQmlPropertyData::DontRemoveBinding | QQmlPropertyData::BypassInterceptor);
        } else {
            QQuickStateAction action;
            action.restore = restoreEntryValues();
            action.property = d->property(name);
            action.fromValue = action.property.read();
            action.specifiedObject = object();
            action.specifiedProperty = name;

            QQmlBinding *newBinding = QQmlBinding::create(
                        &QQmlPropertyPrivate::get(action.property)->core, expression,
                        object(), qmlContext(this));
            if (d->isExplicit) {
                // don't assign the binding, merely evaluate the expression.
                // XXX TODO: add a static QQmlJavaScriptExpression::evaluate(QString)
                // so that we can avoid creating then destroying the binding in this case.
                action.toValue = newBinding->evaluate();
                delete newBinding;
            } else {
                newBinding->setTarget(action.property);
                action.toBinding = newBinding;
                action.deletableToBinding = true;

                state()->addEntryToRevertList(action);
                QQmlAbstractBinding *oldBinding = QQmlPropertyPrivate::binding(action.property);
                if (oldBinding)
                    oldBinding->setEnabled(false, QQmlPropertyData::DontRemoveBinding | QQmlPropertyData::BypassInterceptor);

                QQmlPropertyPrivate::setBinding(newBinding, QQmlPropertyPrivate::None, QQmlPropertyData::DontRemoveBinding | QQmlPropertyData::BypassInterceptor);
            }
        }
    }
    // what about the signal handler?
}

QVariant QQuickPropertyChanges::property(const QString &name) const
{
    Q_D(const QQuickPropertyChanges);
    typedef QPair<QString, QVariant> PropertyEntry;
    typedef QQuickPropertyChangesPrivate::ExpressionChange ExpressionEntry;

    QListIterator<PropertyEntry> propertyIterator(d->properties);
    while (propertyIterator.hasNext()) {
        const PropertyEntry &entry = propertyIterator.next();
        if (entry.first == name) {
            return entry.second;
        }
    }

    QListIterator<ExpressionEntry> expressionIterator(d->expressions);
    while (expressionIterator.hasNext()) {
        const ExpressionEntry &entry = expressionIterator.next();
        if (entry.name == name) {
            return QVariant(entry.expression);
        }
    }

    return QVariant();
}

void QQuickPropertyChanges::removeProperty(const QString &name)
{
    Q_D(QQuickPropertyChanges);
    typedef QPair<QString, QVariant> PropertyEntry;
    typedef QQuickPropertyChangesPrivate::ExpressionChange ExpressionEntry;

    QMutableListIterator<ExpressionEntry> expressionIterator(d->expressions);
    while (expressionIterator.hasNext()) {
        const ExpressionEntry &entry = expressionIterator.next();
        if (entry.name == name) {
            expressionIterator.remove();
            state()->removeEntryFromRevertList(object(), name);
            return;
        }
    }

    QMutableListIterator<PropertyEntry> propertyIterator(d->properties);
    while (propertyIterator.hasNext()) {
        const PropertyEntry &entry = propertyIterator.next();
        if (entry.first == name) {
            propertyIterator.remove();
            state()->removeEntryFromRevertList(object(), name);
            return;
        }
    }
}

QVariant QQuickPropertyChanges::value(const QString &name) const
{
    Q_D(const QQuickPropertyChanges);
    typedef QPair<QString, QVariant> PropertyEntry;

    QListIterator<PropertyEntry> propertyIterator(d->properties);
    while (propertyIterator.hasNext()) {
        const PropertyEntry &entry = propertyIterator.next();
        if (entry.first == name) {
            return entry.second;
        }
    }

    return QVariant();
}

QString QQuickPropertyChanges::expression(const QString &name) const
{
    Q_D(const QQuickPropertyChanges);
    typedef QQuickPropertyChangesPrivate::ExpressionChange ExpressionEntry;

    QListIterator<ExpressionEntry> expressionIterator(d->expressions);
    while (expressionIterator.hasNext()) {
        const ExpressionEntry &entry = expressionIterator.next();
        if (entry.name == name) {
            return entry.expression;
        }
    }

    return QString();
}

void QQuickPropertyChanges::detachFromState()
{
    if (state())
        state()->removeAllEntriesFromRevertList(object());
}

void QQuickPropertyChanges::attachToState()
{
    if (state())
        state()->addEntriesToRevertList(actions());
}

QT_END_NAMESPACE
