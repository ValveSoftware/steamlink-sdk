/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#ifndef TST_QJSVALUE_H
#define TST_QJSVALUE_H

#include <QtCore/qobject.h>
#include <QtCore/qnumeric.h>
#include <qjsengine.h>
#include <qjsvalue.h>
#include <QtTest/QtTest>

Q_DECLARE_METATYPE(QVariant)

class tst_QJSValue : public QObject
{
    Q_OBJECT

public:
    tst_QJSValue();
    virtual ~tst_QJSValue();

private slots:
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

    void toString();
    void toNumber();
    void toBoolean();
    void toBool();
    void toInt();
    void toUInt();
    void toVariant();
    void toQObject_nonQObject_data();
    void toQObject_nonQObject();
    void toQObject();
    void toDateTime();
    void toRegExp();
    void isArray_data();
    void isArray();
    void isDate();
    void isDate_data();
    void isError_propertiesOfGlobalObject();
    void isError_data();
    void isError();
    void isRegExp_data();
    void isRegExp();

    void equals();
    void strictlyEquals();

    void hasProperty_basic();
    void hasProperty_globalObject();
    void hasProperty_changePrototype();
    void hasProperty_QTBUG56830_data();
    void hasProperty_QTBUG56830();

    void deleteProperty_basic();
    void deleteProperty_globalObject();
    void deleteProperty_inPrototype();

    void getSetPrototype_cyclicPrototype();
    void getSetPrototype_evalCyclicPrototype();
    void getSetPrototype_eval();
    void getSetPrototype_invalidPrototype();
    void getSetPrototype_twoEngines();
    void getSetPrototype_null();
    void getSetPrototype_notObjectOrNull();
    void getSetPrototype();
    void getSetProperty_HooliganTask162051();
    void getSetProperty_HooliganTask183072();
    void getSetProperty_propertyRemoval();
    void getSetProperty_resolveMode();
    void getSetProperty_twoEngines();
    void getSetProperty_gettersAndSettersThrowErrorJS();
    void getSetProperty_array();
    void getSetProperty();

    void call_function();
    void call_object();
    void call_newObjects();
    void call_this();
    void call_arguments();
    void call();
    void call_twoEngines();
    void call_nonFunction_data();
    void call_nonFunction();
    void construct_nonFunction_data();
    void construct_nonFunction();
    void construct_simple();
    void construct_newObjectJS();
    void construct_arg();
    void construct_proto();
    void construct_returnInt();
    void construct_throw();
    void construct_twoEngines();
    void construct_constructorThrowsPrimitive();
    void castToPointer();
    void prettyPrinter_data();
    void prettyPrinter();
    void engineDeleted();
    void valueOfWithClosure();
    void nestedObjectToVariant_data();
    void nestedObjectToVariant();

private:
    void newEngine()
    {
        if (engine)
            delete engine;
        engine = new QJSEngine();
    }
    QJSEngine *engine;
};

#endif
