/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <QtQml/qqmlextensionplugin.h>
#include <QtQml/qqml.h>
#include <QtQml/qjsvalue.h>
#include <QtQml/qjsengine.h>
#include "QtQuickTest/private/quicktestresult_p.h"
#include "QtQuickTest/private/quicktestevent_p.h"
#include "private/qtestoptions_p.h"
#include "QtQuick/qquickitem.h"
#include <QtQml/private/qqmlengine_p.h>
#include <QtGui/QGuiApplication>
#include <QtGui/qstylehints.h>

QML_DECLARE_TYPE(QuickTestResult)
QML_DECLARE_TYPE(QuickTestEvent)

#include <QtDebug>

static void initResources()
{
#ifdef QT_STATIC
    Q_INIT_RESOURCE(qmake_QtTest);
#endif
}

QT_BEGIN_NAMESPACE

class QuickTestUtil : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool printAvailableFunctions READ printAvailableFunctions NOTIFY printAvailableFunctionsChanged)
    Q_PROPERTY(int dragThreshold READ dragThreshold NOTIFY dragThresholdChanged)
public:
    QuickTestUtil(QObject *parent = 0)
        :QObject(parent)
    {}

    ~QuickTestUtil()
    {}
    bool printAvailableFunctions() const
    {
        return QTest::printAvailableFunctions;
    }
    int dragThreshold() const { return QGuiApplication::styleHints()->startDragDistance(); }

Q_SIGNALS:
    void printAvailableFunctionsChanged();
    void dragThresholdChanged();

public Q_SLOTS:

    QQmlV4Handle typeName(const QVariant& v) const
    {
        QString name(v.typeName());
        if (v.canConvert<QObject*>()) {
            QQmlType *type = 0;
            const QMetaObject *mo = v.value<QObject*>()->metaObject();
            while (!type && mo) {
                type = QQmlMetaType::qmlType(mo);
                mo = mo->superClass();
            }
            if (type) {
                name = type->qmlTypeName();
            }
        }

        QQmlEngine *engine = qmlEngine(this);
        QV4::ExecutionEngine *v4 = QV8Engine::getV4(engine->handle());
        QV4::Scope scope(v4);
        QV4::ScopedValue s(scope, v4->newString(name));
        return QQmlV4Handle(s);
    }

    bool compare(const QVariant& act, const QVariant& exp) const {
        return act == exp;
    }

    QQmlV4Handle callerFile(int frameIndex = 0) const
    {
        QQmlEngine *engine = qmlEngine(this);
        QV4::ExecutionEngine *v4 = QV8Engine::getV4(engine->handle());
        QV4::Scope scope(v4);

        QVector<QV4::StackFrame> stack = v4->stackTrace(frameIndex + 2);
        if (stack.size() > frameIndex + 1) {
            QV4::ScopedValue s(scope, v4->newString(stack.at(frameIndex + 1).source));
            return QQmlV4Handle(s);
        }
        return QQmlV4Handle();
    }
    int callerLine(int frameIndex = 0) const
    {
        QQmlEngine *engine = qmlEngine(this);
        QV4::ExecutionEngine *v4 = QV8Engine::getV4(engine->handle());

        QVector<QV4::StackFrame> stack = v4->stackTrace(frameIndex + 2);
        if (stack.size() > frameIndex + 1)
            return stack.at(frameIndex + 1).line;
        return -1;
    }
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QuickTestUtil)

QT_BEGIN_NAMESPACE

class QTestQmlModule : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    QTestQmlModule(QObject *parent = 0) : QQmlExtensionPlugin(parent) { initResources(); }
    void registerTypes(const char *uri) Q_DECL_OVERRIDE
    {
        Q_ASSERT(QLatin1String(uri) == QLatin1String("QtTest"));
        qmlRegisterType<QuickTestResult, 0>(uri,1,0,"TestResult");
        qmlRegisterType<QuickTestResult, 1>(uri,1,1,"TestResult");
        qmlRegisterType<QuickTestEvent>(uri,1,0,"TestEvent");
        qmlRegisterType<QuickTestUtil>(uri,1,0,"TestUtil");
    }
};

QT_END_NAMESPACE

#include "main.moc"
