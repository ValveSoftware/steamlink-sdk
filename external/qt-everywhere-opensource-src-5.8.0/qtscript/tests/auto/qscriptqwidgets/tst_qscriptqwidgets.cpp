/****************************************************************************
**
** Copyright (C) 2012 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com, author Stephen Kelly <stephen.kelly@kdab.com>
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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

#include <QtTest/QTest>
#include <QWidget>
#include <QScriptEngine>

class CustomQObject : public QObject
{
    Q_OBJECT
public:
    CustomQObject(QObject *parent = 0)
      : QObject(parent)
    {

    }
};

class tst_QScriptQWidgets : public QObject
{
    Q_OBJECT
public:
    explicit tst_QScriptQWidgets(QObject *parent = 0);
    virtual ~tst_QScriptQWidgets();

private slots:
    void testProperty();
    void testSlot();
};

tst_QScriptQWidgets::tst_QScriptQWidgets(QObject *parent)
  : QObject(parent)
{
    qRegisterMetaType<QWidget*>();
    qRegisterMetaType<CustomQObject*>();
}

tst_QScriptQWidgets::~tst_QScriptQWidgets()
{

}

class ObjectUnderTest : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QWidget* widget READ widget CONSTANT)
    Q_PROPERTY(CustomQObject* customObject READ customObject CONSTANT)
public:
    ObjectUnderTest(QObject *parent = 0)
      : QObject(parent), m_widget(new QWidget), m_customObject(new CustomQObject)
    {

    }

    QWidget* widget() const
    {
        return m_widget.data();
    }

    CustomQObject* customObject() const
    {
        return m_customObject.data();
    }

public slots:
    QWidget* widgetAccessor() const
    {
        return m_widget.data();
    }

    QWidget* widgetReturner(QWidget* widget)
    {
        return widget;
    }

private:
    QScopedPointer<QWidget> m_widget;
    QScopedPointer<CustomQObject> m_customObject;
};

void tst_QScriptQWidgets::testProperty()
{
    QScriptEngine engine;
    ObjectUnderTest *testObject = new ObjectUnderTest(this);
    QCOMPARE(engine.newQObject(testObject).property("widget").toQObject(), testObject->widget());

    QCOMPARE(engine.newQObject(testObject).property("customObject").toQObject(), testObject->customObject());
}

void tst_QScriptQWidgets::testSlot()
{
    {
        QScriptEngine engine;
        ObjectUnderTest *testObject = new ObjectUnderTest(this);
        QCOMPARE(engine.newQObject(testObject).property("widgetAccessor").call(QScriptValue()).toQObject(), testObject->widget());
    }
    {
        QScriptEngine engine;
        ObjectUnderTest *testObject = new ObjectUnderTest(this);
        QCOMPARE(engine.newQObject(testObject).property("widgetReturner").call(QScriptValue(), QScriptValueList() << engine.toScriptValue(testObject->widget())).toQObject(), testObject->widget());
    }
}

QTEST_MAIN(tst_QScriptQWidgets)

#include "tst_qscriptqwidgets.moc"
