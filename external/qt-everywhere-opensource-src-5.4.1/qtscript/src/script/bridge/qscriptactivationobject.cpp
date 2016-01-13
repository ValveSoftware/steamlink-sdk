/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtScript module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL-ONLY$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you have questions regarding the use of this file, please contact
** us via http://www.qt-project.org/.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "config.h"
#include "qscriptactivationobject_p.h"

#include "JSVariableObject.h"

namespace JSC
{
    ASSERT_CLASS_FITS_IN_CELL(QT_PREPEND_NAMESPACE(QScript::QScriptActivationObject));
}

QT_BEGIN_NAMESPACE

/*!
  \class QScript::QScriptActivationObject
  \internal

  Represent a scope for native function call.
*/

namespace QScript
{

const JSC::ClassInfo QScriptActivationObject::info = { "QScriptActivationObject", 0, 0, 0 };

QScriptActivationObject::QScriptActivationObject(JSC::ExecState *callFrame, JSC::JSObject *delegate)
    : JSC::JSVariableObject(callFrame->globalData().activationStructure,
                            new QScriptActivationObjectData(callFrame->registers(), delegate))
{
}

QScriptActivationObject::~QScriptActivationObject()
{
    delete d_ptr();
}

bool QScriptActivationObject::getOwnPropertySlot(JSC::ExecState* exec, const JSC::Identifier& propertyName, JSC::PropertySlot& slot)
{
    if (d_ptr()->delegate != 0)
        return d_ptr()->delegate->getOwnPropertySlot(exec, propertyName, slot);
    return JSC::JSVariableObject::getOwnPropertySlot(exec, propertyName, slot);
}

bool QScriptActivationObject::getOwnPropertyDescriptor(JSC::ExecState* exec, const JSC::Identifier& propertyName, JSC::PropertyDescriptor& descriptor)
{
    if (d_ptr()->delegate != 0)
        return d_ptr()->delegate->getOwnPropertyDescriptor(exec, propertyName, descriptor);
    return JSC::JSVariableObject::getOwnPropertyDescriptor(exec, propertyName, descriptor);
}

void QScriptActivationObject::getOwnPropertyNames(JSC::ExecState* exec, JSC::PropertyNameArray& propertyNames, JSC::EnumerationMode mode)
{
    if (d_ptr()->delegate != 0) {
        d_ptr()->delegate->getOwnPropertyNames(exec, propertyNames, mode);
        return;
    }
    return JSC::JSVariableObject::getOwnPropertyNames(exec, propertyNames, mode);
}

void QScriptActivationObject::putWithAttributes(JSC::ExecState *exec, const JSC::Identifier &propertyName, JSC::JSValue value, unsigned attributes)
{
    if (d_ptr()->delegate != 0) {
        d_ptr()->delegate->putWithAttributes(exec, propertyName, value, attributes);
        return;
    }

    if (symbolTablePutWithAttributes(propertyName, value, attributes))
        return;
    
    JSC::PutPropertySlot slot;
    JSObject::putWithAttributes(exec, propertyName, value, attributes, true, slot);
}

void QScriptActivationObject::put(JSC::ExecState* exec, const JSC::Identifier& propertyName, JSC::JSValue value, JSC::PutPropertySlot& slot)
{
    if (d_ptr()->delegate != 0) {
        d_ptr()->delegate->put(exec, propertyName, value, slot);
        return;
    }
    JSC::JSVariableObject::put(exec, propertyName, value, slot);
}

void QScriptActivationObject::put(JSC::ExecState* exec, unsigned propertyName, JSC::JSValue value)
{
    if (d_ptr()->delegate != 0) {
        d_ptr()->delegate->put(exec, propertyName, value);
        return;
    }
    JSC::JSVariableObject::put(exec, propertyName, value);
}

bool QScriptActivationObject::deleteProperty(JSC::ExecState* exec, const JSC::Identifier& propertyName)
{
    if (d_ptr()->delegate != 0)
        return d_ptr()->delegate->deleteProperty(exec, propertyName);
    return JSC::JSVariableObject::deleteProperty(exec, propertyName);
}

void QScriptActivationObject::defineGetter(JSC::ExecState* exec, const JSC::Identifier& propertyName, JSC::JSObject* getterFunction)
{
    if (d_ptr()->delegate != 0)
        d_ptr()->delegate->defineGetter(exec, propertyName, getterFunction);
    else
        JSC::JSVariableObject::defineGetter(exec, propertyName, getterFunction);
}

void QScriptActivationObject::defineSetter(JSC::ExecState* exec, const JSC::Identifier& propertyName, JSC::JSObject* setterFunction)
{
    if (d_ptr()->delegate != 0)
        d_ptr()->delegate->defineSetter(exec, propertyName, setterFunction);
    else
        JSC::JSVariableObject::defineSetter(exec, propertyName, setterFunction);
}

JSC::JSValue QScriptActivationObject::lookupGetter(JSC::ExecState* exec, const JSC::Identifier& propertyName)
{
    if (d_ptr()->delegate != 0)
        return d_ptr()->delegate->lookupGetter(exec, propertyName);
    return JSC::JSVariableObject::lookupGetter(exec, propertyName);
}

JSC::JSValue QScriptActivationObject::lookupSetter(JSC::ExecState* exec, const JSC::Identifier& propertyName)
{
    if (d_ptr()->delegate != 0)
        return d_ptr()->delegate->lookupSetter(exec, propertyName);
    return JSC::JSVariableObject::lookupSetter(exec, propertyName);
}

} // namespace QScript

QT_END_NAMESPACE

