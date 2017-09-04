/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef TST_QSCRIPTVALUE_H
#define TST_QSCRIPTVALUE_H

#include <QtCore/qobject.h>
#include <QtCore/qnumeric.h>
#include <QtScript/qscriptclass.h>
#include <QtScript/qscriptengine.h>
#include <QtScript/qscriptvalue.h>
#include <QtTest/QtTest>

Q_DECLARE_METATYPE(QVariant)
Q_DECLARE_METATYPE(QScriptValue)

class tst_QScriptValue : public QObject
{
    Q_OBJECT

public:
    tst_QScriptValue();
    virtual ~tst_QScriptValue();

private slots:
    void toObject();

    void ctor_invalid();
    void ctor_undefinedWithEngine();
    void ctor_undefined();
    void ctor_nullWithEngine();
    void ctor_null();
    void ctor_boolWithEngine();
    void ctor_bool();
    void ctor_intWithEngine();
    void ctor_int();
    void ctor_uintWithEngine();
    void ctor_uint();
    void ctor_floatWithEngine();
    void ctor_float();
    void ctor_stringWithEngine();
    void ctor_string();
    void ctor_copyAndAssignWithEngine();
    void ctor_copyAndAssign();
    void ctor_nullEngine();

    void toString();
    void toNumber();
    void toBoolean();
    void toBool();
    void toInteger();
    void toInt32();
    void toUInt32();
    void toUInt16();
    void toVariant();
    void toQObject_nonQObject_data();
    void toQObject_nonQObject();
    void toQObject();
    void toDateTime();
    void toRegExp();
    void instanceOf_twoEngines();
    void instanceOf();
    void isArray_data();
    void isArray();
    void isDate();
    void isDate_data();
    void isError_propertiesOfGlobalObject();
    void isError_data();
    void isError();
    void isRegExp_data();
    void isRegExp();

    void lessThan();
    void equals();
    void strictlyEquals();

    void getSetPrototype_cyclicPrototype();
    void getSetPrototype_evalCyclicPrototype();
    void getSetPrototype_eval();
    void getSetPrototype_invalidPrototype();
    void getSetPrototype_twoEngines();
    void getSetPrototype_null();
    void getSetPrototype_notObjectOrNull();
    void getSetPrototype();
    void getSetScope();
    void getSetProperty_HooliganTask162051();
    void getSetProperty_HooliganTask183072();
    void getSetProperty_propertyRemoval();
    void getSetProperty_resolveMode();
    void getSetProperty_twoEngines();
    void getSetProperty_gettersAndSetters();
    void getSetProperty_gettersAndSettersThrowErrorNative();
    void getSetProperty_gettersAndSettersThrowErrorJS();
    void getSetProperty_gettersAndSettersOnNative();
    void getSetProperty_gettersAndSettersOnGlobalObject();
    void getSetProperty_gettersAndSettersChange();
    void getSetProperty_array();
    void getSetProperty();
    void arrayElementGetterSetter();
    void getSetData_objects_data();
    void getSetData_objects();
    void getSetData_nonObjects_data();
    void getSetData_nonObjects();
    void setData_QTBUG15144();
    void getSetScriptClass_emptyClass_data();
    void getSetScriptClass_emptyClass();
    void getSetScriptClass_JSObjectFromCpp();
    void getSetScriptClass_JSObjectFromJS();
    void getSetScriptClass_QVariant();
    void getSetScriptClass_QObject();
    void call_function();
    void call_object();
    void call_newObjects();
    void call_this();
    void call_arguments();
    void call();
    void call_invalidArguments();
    void call_invalidReturn();
    void call_twoEngines();
    void call_array();
    void call_nonFunction_data();
    void call_nonFunction();
    void construct_nonFunction_data();
    void construct_nonFunction();
    void construct_simple();
    void construct_newObjectJS();
    void construct_undefined();
    void construct_newObjectCpp();
    void construct_arg();
    void construct_proto();
    void construct_returnInt();
    void construct_throw();
    void construct();
    void construct_twoEngines();
    void construct_constructorThrowsPrimitive();
    void castToPointer();
    void prettyPrinter_data();
    void prettyPrinter();
    void engineDeleted();
    void valueOfWithClosure();
    void objectId();
    void nestedObjectToVariant_data();
    void nestedObjectToVariant();
    void propertyFlags_data();
    void propertyFlags();


private:
    void newEngine()
    {
        if (engine)
            delete engine;
        engine = new QScriptEngine();
    }
    QScriptEngine *engine;
};

#endif
