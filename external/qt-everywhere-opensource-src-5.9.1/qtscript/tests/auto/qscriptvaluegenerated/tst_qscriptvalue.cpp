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

#include "tst_qscriptvalue.h"

QT_BEGIN_NAMESPACE
extern bool qt_script_isJITEnabled();
QT_END_NAMESPACE

tst_QScriptValueGenerated::tst_QScriptValueGenerated()
    : engine(0)
{
}

tst_QScriptValueGenerated::~tst_QScriptValueGenerated()
{
    delete engine;
}

void tst_QScriptValueGenerated::dataHelper(InitDataFunction init, DefineDataFunction define)
{
    QTest::addColumn<QString>("__expression__");
    (this->*init)();
    QHash<QString,QScriptValue>::const_iterator it;
    for (it = m_values.constBegin(); it != m_values.constEnd(); ++it) {
        m_currentExpression = it.key();
        (this->*define)(it.key().toLatin1());
    }
    m_currentExpression = QString();
}

QTestData &tst_QScriptValueGenerated::newRow(const char *tag)
{
    return QTest::newRow(tag) << m_currentExpression;
}

void tst_QScriptValueGenerated::testHelper(TestFunction fun)
{
    QFETCH(QString, __expression__);
    QScriptValue value = m_values.value(__expression__);
    (this->*fun)(__expression__.toLatin1(), value);
}

void tst_QScriptValueGenerated::assignAndCopyConstruct_initData()
{
    QTest::addColumn<int>("dummy");
    initScriptValues();
}

void tst_QScriptValueGenerated::assignAndCopyConstruct_makeData(const char *expr)
{
    newRow(expr) << 0;
}

void tst_QScriptValueGenerated::assignAndCopyConstruct_test(const char *, const QScriptValue &value)
{
    QScriptValue copy(value);
    QCOMPARE(copy.strictlyEquals(value), !value.isNumber() || !qIsNaN(value.toNumber()));
    QCOMPARE(copy.engine(), value.engine());

    QScriptValue assigned = copy;
    QCOMPARE(assigned.strictlyEquals(value), !copy.isNumber() || !qIsNaN(copy.toNumber()));
    QCOMPARE(assigned.engine(), assigned.engine());

    QScriptValue other(!value.toBool());
    assigned = other;
    QVERIFY(!assigned.strictlyEquals(copy));
    QVERIFY(assigned.strictlyEquals(other));
    QCOMPARE(assigned.engine(), other.engine());
}

DEFINE_TEST_FUNCTION(assignAndCopyConstruct)

QTEST_MAIN(tst_QScriptValueGenerated)
