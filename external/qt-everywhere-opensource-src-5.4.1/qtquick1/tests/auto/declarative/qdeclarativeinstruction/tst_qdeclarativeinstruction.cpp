/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
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
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qtest.h>
#include <private/qdeclarativecompiler_p.h>

class tst_qdeclarativeinstruction : public QObject
{
    Q_OBJECT
public:
    tst_qdeclarativeinstruction() {}

private slots:
    void dump();
};

static QStringList messages;
static void msgHandler(QtMsgType, const QMessageLogContext &, const QString &msg)
{
    messages << msg;
}

void tst_qdeclarativeinstruction::dump()
{
    QDeclarativeCompiledData *data = new QDeclarativeCompiledData(0);
    {
        QDeclarativeInstruction i;
        i.line = 0;
        i.type = QDeclarativeInstruction::Init;
        i.init.bindingsSize = 0;
        i.init.parserStatusSize = 3;
        i.init.contextCache = -1;
        i.init.compiledBinding = -1;
        data->bytecode << i;
    }

    {
        QDeclarativeCompiledData::TypeReference ref;
        ref.className = "Test";
        data->types << ref;

        QDeclarativeInstruction i;
        i.line = 1;
        i.type = QDeclarativeInstruction::CreateObject;
        i.create.type = 0;
        i.create.data = -1;
        i.create.bindingBits = -1;
        i.create.column = 10;
        data->bytecode << i;
    }

    {
        data->primitives << "testId";

        QDeclarativeInstruction i;
        i.line = 2;
        i.type = QDeclarativeInstruction::SetId;
        i.setId.value = data->primitives.count() - 1;
        i.setId.index = 0;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 3;
        i.type = QDeclarativeInstruction::SetDefault;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 4;
        i.type = QDeclarativeInstruction::CreateComponent;
        i.createComponent.count = 3;
        i.createComponent.column = 4;
        i.createComponent.endLine = 14;
        i.createComponent.metaObject = 0;

        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 5;
        i.type = QDeclarativeInstruction::StoreMetaObject;
        i.storeMeta.data = 3;
        i.storeMeta.aliasData = 6;
        i.storeMeta.propertyCache = 7;

        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 6;
        i.type = QDeclarativeInstruction::StoreFloat;
        i.storeFloat.propertyIndex = 3;
        i.storeFloat.value = 11.3f;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 7;
        i.type = QDeclarativeInstruction::StoreDouble;
        i.storeDouble.propertyIndex = 4;
        i.storeDouble.value = 14.8;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 8;
        i.type = QDeclarativeInstruction::StoreInteger;
        i.storeInteger.propertyIndex = 5;
        i.storeInteger.value = 9;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 9;
        i.type = QDeclarativeInstruction::StoreBool;
        i.storeBool.propertyIndex = 6;
        i.storeBool.value = true;

        data->bytecode << i;
    }

    {
        data->primitives << "Test String";
        QDeclarativeInstruction i;
        i.line = 10;
        i.type = QDeclarativeInstruction::StoreString;
        i.storeString.propertyIndex = 7;
        i.storeString.value = data->primitives.count() - 1;
        data->bytecode << i;
    }

    {
        data->urls << QUrl("http://www.qt-project.org");
        QDeclarativeInstruction i;
        i.line = 11;
        i.type = QDeclarativeInstruction::StoreUrl;
        i.storeUrl.propertyIndex = 8;
        i.storeUrl.value = data->urls.count() - 1;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 12;
        i.type = QDeclarativeInstruction::StoreColor;
        i.storeColor.propertyIndex = 9;
        i.storeColor.value = 0xFF00FF00;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 13;
        i.type = QDeclarativeInstruction::StoreDate;
        i.storeDate.propertyIndex = 10;
        i.storeDate.value = 9;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 14;
        i.type = QDeclarativeInstruction::StoreTime;
        i.storeTime.propertyIndex = 11;
        i.storeTime.valueIndex = 33;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 15;
        i.type = QDeclarativeInstruction::StoreDateTime;
        i.storeDateTime.propertyIndex = 12;
        i.storeDateTime.valueIndex = 44;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 16;
        i.type = QDeclarativeInstruction::StorePoint;
        i.storeRealPair.propertyIndex = 13;
        i.storeRealPair.valueIndex = 3;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 17;
        i.type = QDeclarativeInstruction::StorePointF;
        i.storeRealPair.propertyIndex = 14;
        i.storeRealPair.valueIndex = 9;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 18;
        i.type = QDeclarativeInstruction::StoreSize;
        i.storeRealPair.propertyIndex = 15;
        i.storeRealPair.valueIndex = 8;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 19;
        i.type = QDeclarativeInstruction::StoreSizeF;
        i.storeRealPair.propertyIndex = 16;
        i.storeRealPair.valueIndex = 99;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 20;
        i.type = QDeclarativeInstruction::StoreRect;
        i.storeRect.propertyIndex = 17;
        i.storeRect.valueIndex = 2;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 21;
        i.type = QDeclarativeInstruction::StoreRectF;
        i.storeRect.propertyIndex = 18;
        i.storeRect.valueIndex = 19;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 22;
        i.type = QDeclarativeInstruction::StoreVector3D;
        i.storeVector3D.propertyIndex = 19;
        i.storeVector3D.valueIndex = 9;
        data->bytecode << i;
    }

    {
        data->primitives << "color(1, 1, 1, 1)";
        QDeclarativeInstruction i;
        i.line = 23;
        i.type = QDeclarativeInstruction::StoreVariant;
        i.storeString.propertyIndex = 20;
        i.storeString.value = data->primitives.count() - 1;

        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 24;
        i.type = QDeclarativeInstruction::StoreObject;
        i.storeObject.propertyIndex = 21;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 25;
        i.type = QDeclarativeInstruction::StoreVariantObject;
        i.storeObject.propertyIndex = 22;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 26;
        i.type = QDeclarativeInstruction::StoreInterface;
        i.storeObject.propertyIndex = 23;
        data->bytecode << i;
    }

    {
        data->primitives << "console.log(1921)";

        QDeclarativeInstruction i;
        i.line = 27;
        i.type = QDeclarativeInstruction::StoreSignal;
        i.storeSignal.signalIndex = 2;
        i.storeSignal.value = data->primitives.count() - 1;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 29;
        i.type = QDeclarativeInstruction::StoreScriptString;
        i.storeScriptString.propertyIndex = 24;
        i.storeScriptString.value = 3;
        i.storeScriptString.scope = 1;
        data->bytecode << i;
    }

    {
        data->datas << "mySignal";

        QDeclarativeInstruction i;
        i.line = 30;
        i.type = QDeclarativeInstruction::AssignSignalObject;
        i.assignSignalObject.signal = 0;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 31;
        i.type = QDeclarativeInstruction::AssignCustomType;
        i.assignCustomType.propertyIndex = 25;
        i.assignCustomType.valueIndex = 4;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 32;
        i.type = QDeclarativeInstruction::StoreBinding;
        i.assignBinding.property = 26;
        i.assignBinding.value = 3;
        i.assignBinding.context = 2;
        i.assignBinding.owner = 0;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 33;
        i.type = QDeclarativeInstruction::StoreCompiledBinding;
        i.assignBinding.property = 27;
        i.assignBinding.value = 2;
        i.assignBinding.context = 4;
        i.assignBinding.owner = 0;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 34;
        i.type = QDeclarativeInstruction::StoreValueSource;
        i.assignValueSource.property = 29;
        i.assignValueSource.owner = 1;
        i.assignValueSource.castValue = 4;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 35;
        i.type = QDeclarativeInstruction::StoreValueInterceptor;
        i.assignValueInterceptor.property = 30;
        i.assignValueInterceptor.owner = 2;
        i.assignValueInterceptor.castValue = -4;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 36;
        i.type = QDeclarativeInstruction::BeginObject;
        i.begin.castValue = 4;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 38;
        i.type = QDeclarativeInstruction::StoreObjectQList;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 39;
        i.type = QDeclarativeInstruction::AssignObjectList;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 40;
        i.type = QDeclarativeInstruction::FetchAttached;
        i.fetchAttached.id = 23;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 42;
        i.type = QDeclarativeInstruction::FetchQList;
        i.fetch.property = 32;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 43;
        i.type = QDeclarativeInstruction::FetchObject;
        i.fetch.property = 33;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 44;
        i.type = QDeclarativeInstruction::FetchValueType;
        i.fetchValue.property = 34;
        i.fetchValue.type = 6;
        i.fetchValue.bindingSkipList = 7;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 45;
        i.type = QDeclarativeInstruction::PopFetchedObject;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 46;
        i.type = QDeclarativeInstruction::PopQList;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 47;
        i.type = QDeclarativeInstruction::PopValueType;
        i.fetchValue.property = 35;
        i.fetchValue.type = 8;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 48;
        i.type = QDeclarativeInstruction::Defer;
        i.defer.deferCount = 7;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = -1;
        i.type = QDeclarativeInstruction::Defer;
        i.defer.deferCount = 7;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 48;
        i.type = QDeclarativeInstruction::StoreImportedScript;
        i.storeScript.value = 2;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 50;
        i.type = (QDeclarativeInstruction::Type)(1234); // Non-existent
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 51;
        i.type = QDeclarativeInstruction::StoreVariantInteger;
        i.storeInteger.value = 11;
        i.storeInteger.propertyIndex = 32;
        data->bytecode << i;
    }

    {
        QDeclarativeInstruction i;
        i.line = 52;
        i.type = QDeclarativeInstruction::StoreVariantDouble;
        i.storeDouble.value = 33.7;
        i.storeDouble.propertyIndex = 19;
        data->bytecode << i;
    }

    QStringList expect;
    expect
        << "Index\tLine\tOperation\t\tData1\tData2\tData3\tComments"
        << "-------------------------------------------------------------------------------"
        << "0\t\t0\tINIT\t\t\t0\t3\t-1\t-1"
        << "1\t\t1\tCREATE\t\t\t0\t-1\t\t\"Test\""
        << "2\t\t2\tSETID\t\t\t0\t\t\t\"testId\""
        << "3\t\t3\tSET_DEFAULT"
        << "4\t\t4\tCREATE_COMPONENT\t3"
        << "5\t\t5\tSTORE_META\t\t3"
        << "6\t\t6\tSTORE_FLOAT\t\t3\t11.3"
        << "7\t\t7\tSTORE_DOUBLE\t\t4\t14.8"
        << "8\t\t8\tSTORE_INTEGER\t\t5\t9"
        << "9\t\t9\tSTORE_BOOL\t\t6\ttrue"
        << "10\t\t10\tSTORE_STRING\t\t7\t1\t\t\"Test String\""
        << "11\t\t11\tSTORE_URL\t\t8\t0\t\tQUrl(\"http://www.qt-project.org\")"
        << "12\t\t12\tSTORE_COLOR\t\t9\t\t\t\"ff00ff00\""
        << "13\t\t13\tSTORE_DATE\t\t10\t9"
        << "14\t\t14\tSTORE_TIME\t\t11\t33"
        << "15\t\t15\tSTORE_DATETIME\t\t12\t44"
        << "16\t\t16\tSTORE_POINT\t\t13\t3"
        << "17\t\t17\tSTORE_POINTF\t\t14\t9"
        << "18\t\t18\tSTORE_SIZE\t\t15\t8"
        << "19\t\t19\tSTORE_SIZEF\t\t16\t99"
        << "20\t\t20\tSTORE_RECT\t\t17\t2"
        << "21\t\t21\tSTORE_RECTF\t\t18\t19"
        << "22\t\t22\tSTORE_VECTOR3D\t\t19\t9"
        << "23\t\t23\tSTORE_VARIANT\t\t20\t2\t\t\"color(1, 1, 1, 1)\""
        << "24\t\t24\tSTORE_OBJECT\t\t21"
        << "25\t\t25\tSTORE_VARIANT_OBJECT\t22"
        << "26\t\t26\tSTORE_INTERFACE\t\t23"
        << "27\t\t27\tSTORE_SIGNAL\t\t2\t3\t\t\"console.log(1921)\""
        << "28\t\t29\tSTORE_SCRIPT_STRING\t24\t3\t1"
        << "29\t\t30\tASSIGN_SIGNAL_OBJECT\t0\t\t\t\"mySignal\""
        << "30\t\t31\tASSIGN_CUSTOMTYPE\t25\t4"
        << "31\t\t32\tSTORE_BINDING\t26\t3\t2"
        << "32\t\t33\tSTORE_COMPILED_BINDING\t27\t2\t4"
        << "33\t\t34\tSTORE_VALUE_SOURCE\t29\t4"
        << "34\t\t35\tSTORE_VALUE_INTERCEPTOR\t30\t-4"
        << "35\t\t36\tBEGIN\t\t\t4"
        << "36\t\t38\tSTORE_OBJECT_QLIST"
        << "37\t\t39\tASSIGN_OBJECT_LIST"
        << "38\t\t40\tFETCH_ATTACHED\t\t23"
        << "39\t\t42\tFETCH_QLIST\t\t32"
        << "40\t\t43\tFETCH\t\t\t33"
        << "41\t\t44\tFETCH_VALUE\t\t34\t6\t7"
        << "42\t\t45\tPOP"
        << "43\t\t46\tPOP_QLIST"
        << "44\t\t47\tPOP_VALUE\t\t35\t8"
        << "45\t\t48\tDEFER\t\t\t7"
        << "46\t\tNA\tDEFER\t\t\t7"
        << "47\t\t48\tSTORE_IMPORTED_SCRIPT\t2"
        << "48\t\t50\tXXX UNKNOWN INSTRUCTION\t1234"
        << "49\t\t51\tSTORE_VARIANT_INTEGER\t\t32\t11"
        << "50\t\t52\tSTORE_VARIANT_DOUBLE\t\t19\t33.7"
        << "-------------------------------------------------------------------------------";

    messages = QStringList();
    QtMessageHandler old = qInstallMessageHandler(msgHandler);
    data->dumpInstructions();
    qInstallMessageHandler(old);

    QCOMPARE(messages.count(), expect.count());
    for (int ii = 0; ii < messages.count(); ++ii) {
        QCOMPARE(messages.at(ii), expect.at(ii));
    }

    data->release();
}

QTEST_MAIN(tst_qdeclarativeinstruction)

#include "tst_qdeclarativeinstruction.moc"
