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


#include <QtTest/QtTest>

#include <qscriptengine.h>
#include <qscriptenginedebugger.h>
#include <qaction.h>
#include <qlineedit.h>
#include <qmainwindow.h>
#include <qmenu.h>
#include <qplaintextedit.h>
#include <qtoolbar.h>

// Can't use QTest::qWait() because it causes event loop to hang on some platforms
static void qsWait(int ms)
{
    QTimer timer;
    timer.setSingleShot(true);
    timer.setInterval(ms);
    timer.start();
    QEventLoop loop;
    QObject::connect(&timer, SIGNAL(timeout()), &loop, SLOT(quit()));
    loop.exec();
    QCoreApplication::processEvents();
}

class tst_QScriptEngineDebugger : public QObject
{
    Q_OBJECT

public:
    tst_QScriptEngineDebugger();
    virtual ~tst_QScriptEngineDebugger();

private slots:
    void attachAndDetach();
    void action();
    void widget();
    void standardObjects();
    void debuggerSignals();
    void consoleCommands();
    void multithreadedDebugging();
    void autoShowStandardWindow();
    void standardWindowOwnership();
    void engineDeleted();
};

tst_QScriptEngineDebugger::tst_QScriptEngineDebugger()
{
}

tst_QScriptEngineDebugger::~tst_QScriptEngineDebugger()
{
}

void tst_QScriptEngineDebugger::attachAndDetach()
{
#if defined(Q_OS_WINCE) && _WIN32_WCE < 0x600
    QSKIP("skipped due to high mem usage until task 261062 is fixed");
#endif
    {
        QScriptEngineDebugger debugger;
        QCOMPARE(debugger.state(), QScriptEngineDebugger::SuspendedState);
        debugger.attachTo(0);
        QScriptEngine engine;
        debugger.attachTo(&engine);
        QCOMPARE(debugger.state(), QScriptEngineDebugger::SuspendedState);
    }
    {
        QScriptEngineDebugger debugger;
        QScriptEngine engine;
        QScriptValue oldPrint = engine.globalObject().property("print");
        QVERIFY(oldPrint.isFunction());
        QVERIFY(!engine.globalObject().property("__FILE__").isValid());
        QVERIFY(!engine.globalObject().property("__LINE__").isValid());

        debugger.attachTo(&engine);
        QVERIFY(engine.globalObject().property("__FILE__").isUndefined());
        QVERIFY(engine.globalObject().property("__LINE__").isNumber());
        QVERIFY(!engine.globalObject().property("print").strictlyEquals(oldPrint));

        debugger.detach();
        QVERIFY(engine.globalObject().property("print").strictlyEquals(oldPrint));
        QVERIFY(!engine.globalObject().property("__FILE__").isValid());
        QVERIFY(!engine.globalObject().property("__LINE__").isValid());
    }
    {
        QScriptEngineDebugger debugger;
        QScriptEngine engine;
        debugger.attachTo(&engine);
        debugger.detach();
        QScriptEngine engine2;
        debugger.attachTo(&engine2);
    }
    {
        QScriptEngineDebugger debugger;
        QScriptEngine engine;
        debugger.attachTo(&engine);
        debugger.detach();
        QScriptEngine engine2;
        debugger.attachTo(&engine2);
        debugger.detach();
    }
#ifndef Q_OS_WINCE // demands too much memory for WinCE
    {
        QScriptEngineDebugger debugger;
        QScriptEngine engine;
        debugger.attachTo(&engine);
        QScriptEngineDebugger debugger2;
        debugger2.attachTo(&engine);
    }
#endif
    {
        QScriptEngine *engine = new QScriptEngine;
        QScriptEngineDebugger debugger;
        debugger.attachTo(engine);
        delete engine;
        QScriptEngine engine2;
        debugger.attachTo(&engine2);
    }
}

void tst_QScriptEngineDebugger::action()
{
#if defined(Q_OS_WINCE) && _WIN32_WCE < 0x600
    QSKIP("skipped due to high mem usage until task 261062 is fixed");
#endif

    QScriptEngine engine;
    QScriptEngineDebugger debugger;
    debugger.attachTo(&engine);
    QList<QScriptEngineDebugger::DebuggerAction> actions;
    actions
        << QScriptEngineDebugger::InterruptAction
        << QScriptEngineDebugger::ContinueAction
        << QScriptEngineDebugger::StepIntoAction
        << QScriptEngineDebugger::StepOverAction
        << QScriptEngineDebugger::StepOutAction
        << QScriptEngineDebugger::RunToCursorAction
        << QScriptEngineDebugger::RunToNewScriptAction
        << QScriptEngineDebugger::ToggleBreakpointAction
        << QScriptEngineDebugger::ClearDebugOutputAction
        << QScriptEngineDebugger::ClearErrorLogAction
        << QScriptEngineDebugger::ClearConsoleAction
        << QScriptEngineDebugger::FindInScriptAction
        << QScriptEngineDebugger::FindNextInScriptAction
        << QScriptEngineDebugger::FindPreviousInScriptAction
        << QScriptEngineDebugger::GoToLineAction;
    QList<QAction*> lst;
    for (int i = 0; i < actions.size(); ++i) {
        QScriptEngineDebugger::DebuggerAction da = actions.at(i);
        QAction *act = debugger.action(da);
        QVERIFY(act != 0);
        QCOMPARE(act, debugger.action(da));
        QCOMPARE(act->parent(), (QObject*)&debugger);
        QVERIFY(lst.indexOf(act) == -1);
        lst.append(act);
    }
}

void tst_QScriptEngineDebugger::widget()
{
#if defined(Q_OS_WINCE) && _WIN32_WCE < 0x600
    QSKIP("skipped due to high mem usage until task 261062 is fixed");
#endif

    QScriptEngine engine;
    QScriptEngineDebugger debugger;
    debugger.attachTo(&engine);
    QList<QScriptEngineDebugger::DebuggerWidget> widgets;
    widgets
        << QScriptEngineDebugger::ConsoleWidget
        << QScriptEngineDebugger::StackWidget
        << QScriptEngineDebugger::ScriptsWidget
        << QScriptEngineDebugger::LocalsWidget
        << QScriptEngineDebugger::CodeWidget
        << QScriptEngineDebugger::CodeFinderWidget
        << QScriptEngineDebugger::BreakpointsWidget
        << QScriptEngineDebugger::DebugOutputWidget
        << QScriptEngineDebugger::ErrorLogWidget;
    QList<QWidget*> lst;
    for (int i = 0; i < widgets.size(); ++i) {
        QScriptEngineDebugger::DebuggerWidget dw = widgets.at(i);
        QWidget *wid = debugger.widget(dw);
        QVERIFY(wid != 0);
        QCOMPARE(wid, debugger.widget(dw));
        QVERIFY(lst.indexOf(wid) == -1);
        lst.append(wid);
        QCOMPARE(static_cast<QWidget *>(wid->parent()), (QWidget*)0);
    }
}

void tst_QScriptEngineDebugger::standardObjects()
{
#if defined(Q_OS_WINCE) && _WIN32_WCE < 0x600
    QSKIP("skipped due to high mem usage until task 261062 is fixed");
#endif

    QScriptEngine engine;
    QScriptEngineDebugger debugger;
    debugger.attachTo(&engine);

    QMainWindow *win = debugger.standardWindow();
    QCOMPARE(static_cast<QWidget *>(win->parent()), (QWidget*)0);

    QMenu *menu = debugger.createStandardMenu();
    QCOMPARE(static_cast<QWidget *>(menu->parent()), (QWidget*)0);
#ifndef QT_NO_TOOLBAR
    QToolBar *toolBar = debugger.createStandardToolBar();
    QCOMPARE(static_cast<QWidget *>(toolBar->parent()), (QWidget*)0);
#endif

    QMenu *menu2 = debugger.createStandardMenu(win);
    QCOMPARE(static_cast<QWidget *>(menu2->parent()), (QWidget*)win);
    QVERIFY(menu2 != menu);
#ifndef QT_NO_TOOLBAR
    QToolBar *toolBar2 = debugger.createStandardToolBar(win);
    QCOMPARE(static_cast<QWidget *>(toolBar2->parent()), (QWidget*)win);
    QVERIFY(toolBar2 != toolBar);
#endif

    delete menu;
#ifndef QT_NO_TOOLBAR
    delete toolBar;
#endif
}

void tst_QScriptEngineDebugger::debuggerSignals()
{
#if defined(Q_OS_WINCE) && _WIN32_WCE < 0x600
    QSKIP("skipped due to high mem usage until task 261062 is fixed");
#endif

    QScriptEngine engine;
    QScriptEngineDebugger debugger;
    debugger.attachTo(&engine);
    debugger.setAutoShowStandardWindow(false);
    QSignalSpy evaluationSuspendedSpy(&debugger, SIGNAL(evaluationSuspended()));
    QSignalSpy evaluationResumedSpy(&debugger, SIGNAL(evaluationResumed()));
    QObject::connect(&debugger, SIGNAL(evaluationSuspended()),
                     debugger.action(QScriptEngineDebugger::ContinueAction),
                     SLOT(trigger()));
    engine.evaluate("123");
    QCOMPARE(evaluationSuspendedSpy.count(), 0);
    QCOMPARE(evaluationResumedSpy.count(), 0);
    engine.evaluate("debugger");
    QCoreApplication::processEvents();
    QCOMPARE(evaluationSuspendedSpy.count(), 1);
    QCOMPARE(evaluationResumedSpy.count(), 1);
}

static void executeConsoleCommand(QLineEdit *inputEdit, QPlainTextEdit *outputEdit,
                                  const QString &text)
{
    QString before = outputEdit->toPlainText();
    inputEdit->setText(text);
    QTest::keyPress(inputEdit, Qt::Key_Enter);
    const int delay = 100;
    qsWait(delay);
    QString after = outputEdit->toPlainText();
    int retryCount = 10;
LAgain:
    while ((before == after) && (retryCount != 0)) {
        qsWait(delay);
        after = outputEdit->toPlainText();
        --retryCount;
    }
    if (before != after) {
        before = after;
        qsWait(delay);
        after = outputEdit->toPlainText();
        if (before != after) {
            retryCount = 10;
            goto LAgain;
        }
    }
}

class DebuggerCommandExecutor : public QObject
{
    Q_OBJECT
public:
    DebuggerCommandExecutor(QLineEdit *inputEdit,
                            QPlainTextEdit *outputEdit,
                            const QString &text,
                            QObject *parent = 0)
        : QObject(parent), m_inputEdit(inputEdit),
          m_outputEdit(outputEdit), m_commands(text) {}
    DebuggerCommandExecutor(QLineEdit *inputEdit,
                            QPlainTextEdit *outputEdit,
                            const QStringList &commands,
                            QObject *parent = 0)
        : QObject(parent), m_inputEdit(inputEdit),
          m_outputEdit(outputEdit), m_commands(commands) {}
public Q_SLOTS:
    void execute() {
        for (int i = 0; i < m_commands.size(); ++i)
            executeConsoleCommand(m_inputEdit, m_outputEdit, m_commands.at(i));
    }
private:
    QLineEdit *m_inputEdit;
    QPlainTextEdit *m_outputEdit;
    QStringList m_commands;
};

void tst_QScriptEngineDebugger::consoleCommands()
{
    QSKIP("This test can hang / misbehave because of timing/event loop issues (task 241300)");

    QScriptEngine engine;
    QScriptEngineDebugger debugger;
    debugger.setAutoShowStandardWindow(false);
    debugger.attachTo(&engine);

    QWidget *consoleWidget = debugger.widget(QScriptEngineDebugger::ConsoleWidget);
    QLineEdit *inputEdit = qFindChild<QLineEdit*>(consoleWidget);
    QVERIFY(inputEdit != 0);
    QPlainTextEdit *outputEdit = qFindChild<QPlainTextEdit*>(consoleWidget);
    QVERIFY(outputEdit != 0);

    QVERIFY(outputEdit->toPlainText().startsWith("Welcome to the Qt Script debugger."));
    outputEdit->clear();

    // print()
    {
        QWidget *debugOutputWidget = debugger.widget(QScriptEngineDebugger::DebugOutputWidget);
        QPlainTextEdit *debugOutputEdit = qFindChild<QPlainTextEdit*>(debugOutputWidget);
        QVERIFY(debugOutputEdit != 0);

        QVERIFY(debugOutputEdit->toPlainText().isEmpty());
        executeConsoleCommand(inputEdit, outputEdit, "print('Test of debug output')");
        QCOMPARE(debugOutputEdit->toPlainText(), QString::fromLatin1("Test of debug output"));

        QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> print('Test of debug output')"));
    }

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".info scripts");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .info scripts\nNo scripts loaded."));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".break foo.qs:123");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .break foo.qs:123\nBreakpoint 1: foo.qs, line 123."));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".break 123");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .break 123\nNo script."));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".info breakpoints");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .info breakpoints\nId\tEnabled\tWhere\n1\tyes\tfoo.qs:123"));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".disable 1");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .disable 1"));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".info breakpoints");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .info breakpoints\nId\tEnabled\tWhere\n1\tno\tfoo.qs:123"));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".disable 1");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .disable 1"));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".disable 123");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .disable 123\nNo breakpoint number 123."));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".enable 1");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .enable 1"));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".info breakpoints");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .info breakpoints\nId\tEnabled\tWhere\n1\tyes\tfoo.qs:123"));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".enable 123");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .enable 123\nNo breakpoint number 123."));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".condition 1 i > 456");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .condition 1 i > 456"));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".info breakpoints");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .info breakpoints\nId\tEnabled\tWhere\n1\tyes\tfoo.qs:123\n\tstop only if i > 456"));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".condition 1");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .condition 1\nBreakpoint 1 now unconditional."));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".info breakpoints");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .info breakpoints\nId\tEnabled\tWhere\n1\tyes\tfoo.qs:123"));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".condition 123");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .condition 123\nNo breakpoint number 123."));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".ignore 1");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .ignore 1\nMissing argument (ignore-count)."));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".ignore 1 10");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .ignore 1 10\nBreakpoint 1 will be ignored the next 10 time(s)."));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".delete 1");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .delete 1"));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".info breakpoints");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .info breakpoints\nNo breakpoints set."));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".tbreak bar.qs:456");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .tbreak bar.qs:456\nBreakpoint 2: bar.qs, line 456."));

    {
        QString script;
        script.append("function foo(i) {\n");
        for (int i = 0; i < 100; ++i)
            script.append(QString::fromLatin1("    i = i + %0;\n").arg(i));
        script.append("    return i;\n}");
        engine.evaluate(script, "foo.qs");
    }
    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".info scripts");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .info scripts\n\tfoo.qs"));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".list foo.qs");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .list foo.qs\n"
                                                            "1\tfunction foo(i) {\n"
                                                            "2\t    i = i + 0;\n"
                                                            "3\t    i = i + 1;\n"
                                                            "4\t    i = i + 2;\n"
                                                            "5\t    i = i + 3;\n"
                                                            "6\t    i = i + 4;\n"
                                                            "7\t    i = i + 5;\n"
                                                            "8\t    i = i + 6;\n"
                                                            "9\t    i = i + 7;\n"
                                                            "10\t    i = i + 8;"));
    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".list");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .list\n"
                                                            "No script."));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".backtrace");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .backtrace\n#0  <global>() at -1"));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".down");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .down\nAlready at bottom (innermost) frame."));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".up");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .up\nAlready at top (outermost) frame."));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".frame");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .frame\n#0  <global>() at -1"));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".break foo.qs:789");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .break foo.qs:789\nBreakpoint 3: foo.qs, line 789."));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".clear foo.qs:789");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .clear foo.qs:789\nDeleted breakpoint 3."));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".info breakpoints");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .info breakpoints\nId\tEnabled\tWhere\n2\tyes\tbar.qs:456"));

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".info locals");
    QVERIFY(outputEdit->toPlainText().startsWith("qsdb> .info locals\n"
                                                 "NaN                : NaN\n"
                                                 "Infinity           : Infinity\n"
                                                 "undefined          : undefined\n"
                                                 "print              : function () { [native] }\n"
                                                 "parseInt           : function () { [native] }\n"
                                                 "parseFloat         : function () { [native] }\n"
                                                 "isNaN              : function () { [native] }\n"
                                                 "isFinite           : function () { [native] }\n"
                                                 "decodeURI          : function () { [native] }\n"
                                                 "decodeURIComponent : function () { [native] }\n"
                                                 "encodeURI          : function () { [native] }\n"
                                                 "encodeURIComponent : function () { [native] }\n"
                                                 "escape             : function () { [native] }\n"
                                                 "unescape           : function () { [native] }\n"
                                                 "version            : function () { [native] }\n"
                                                 "gc                 : function () { [native] }\n"
                                                 "Object             : function () { [native] }\n"
                                                 "Function           : function () { [native] }\n"
                                                 "Number             : function () { [native] }\n"
                                                 "Boolean            : function () { [native] }"));

    outputEdit->clear();
    QVERIFY(!engine.globalObject().property("a").isValid());
    executeConsoleCommand(inputEdit, outputEdit, ".eval a = 123");
    QVERIFY(engine.globalObject().property("a").isNumber());
    QCOMPARE(engine.globalObject().property("a").toInt32(), 123);

    outputEdit->clear();
    QVERIFY(!engine.globalObject().property("b").isValid());
    executeConsoleCommand(inputEdit, outputEdit, "b = 456");
    QVERIFY(engine.globalObject().property("b").isNumber());
    QCOMPARE(engine.globalObject().property("b").toInt32(), 456);

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".break myscript.qs:1");
    QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("qsdb> .break myscript.qs:1\nBreakpoint 4: myscript.qs, line 1."));

    {
        DebuggerCommandExecutor executor(inputEdit, outputEdit, ".continue");
        QObject::connect(&debugger, SIGNAL(evaluationSuspended()), &executor, SLOT(execute()), Qt::QueuedConnection);
        outputEdit->clear();
        engine.evaluate("void(123);", "myscript.qs");
        QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("Breakpoint 4 at myscript.qs, line 1.\n1\tvoid(123);\nqsdb> .continue"));
    }

    {
        DebuggerCommandExecutor executor(inputEdit, outputEdit, ".step");
        QObject::connect(&debugger, SIGNAL(evaluationSuspended()), &executor, SLOT(execute()), Qt::QueuedConnection);
        outputEdit->clear();
        engine.evaluate("void(123);\nvoid(456);", "myscript.qs");
        QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("Breakpoint 4 at myscript.qs, line 1.\n"
                                                                "1\tvoid(123);\n"
                                                                "qsdb> .step\n"
                                                                "2\tvoid(456);\n"
                                                                "qsdb> .step"));
    }

    {
        DebuggerCommandExecutor executor(inputEdit, outputEdit, ".step 2");
        QObject::connect(&debugger, SIGNAL(evaluationSuspended()), &executor, SLOT(execute()), Qt::QueuedConnection);
        outputEdit->clear();
        engine.evaluate("void(123);\nvoid(456);", "myscript.qs");
        QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("Breakpoint 4 at myscript.qs, line 1.\n1\tvoid(123);\nqsdb> .step 2"));
    }

    {
        DebuggerCommandExecutor executor(inputEdit, outputEdit, ".next");
        QObject::connect(&debugger, SIGNAL(evaluationSuspended()), &executor, SLOT(execute()), Qt::QueuedConnection);
        outputEdit->clear();
        engine.evaluate("void(123);\nvoid(456);", "myscript.qs");
        QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("Breakpoint 4 at myscript.qs, line 1.\n"
                                                                "1\tvoid(123);\n"
                                                                "qsdb> .next\n"
                                                                "2\tvoid(456);\n"
                                                                "qsdb> .next"));
    }

    {
        DebuggerCommandExecutor executor(inputEdit, outputEdit, ".next 2");
        QObject::connect(&debugger, SIGNAL(evaluationSuspended()), &executor, SLOT(execute()), Qt::QueuedConnection);
        outputEdit->clear();
        engine.evaluate("void(123);\nvoid(456);", "myscript.qs");
        QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("Breakpoint 4 at myscript.qs, line 1.\n"
                                                                "1\tvoid(123);\n"
                                                                "qsdb> .next 2"));
    }

    {
        DebuggerCommandExecutor executor(inputEdit, outputEdit, ".finish");
        QObject::connect(&debugger, SIGNAL(evaluationSuspended()), &executor, SLOT(execute()), Qt::QueuedConnection);
        outputEdit->clear();
        engine.evaluate("void(123);\nvoid(456);", "myscript.qs");
        QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("Breakpoint 4 at myscript.qs, line 1.\n1\tvoid(123);\nqsdb> .finish"));
    }

    {
        DebuggerCommandExecutor executor(inputEdit, outputEdit, ".return");
        QObject::connect(&debugger, SIGNAL(evaluationSuspended()), &executor, SLOT(execute()), Qt::QueuedConnection);
        outputEdit->clear();
        engine.evaluate("void(123);\nvoid(456);\n789;", "myscript.qs");
        QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("Breakpoint 4 at myscript.qs, line 1.\n1\tvoid(123);\nqsdb> .return"));
    }

    {
        DebuggerCommandExecutor executor(inputEdit, outputEdit, QStringList() << ".list" << ".continue");
        QObject::connect(&debugger, SIGNAL(evaluationSuspended()), &executor, SLOT(execute()), Qt::QueuedConnection);
        outputEdit->clear();
        engine.evaluate("void(123);\nvoid(456);\n789;", "myscript.qs");
        QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("Breakpoint 4 at myscript.qs, line 1.\n"
                                                                "1\tvoid(123);\n"
                                                                "qsdb> .list\n"
                                                                "1\tvoid(123);\n"
                                                                "2\tvoid(456);\n"
                                                                "3\t789;\n"
                                                                "4\n"
                                                                "5\n"
                                                                "6\n"
                                                                "7\n"
                                                                "8\n"
                                                                "9\n"
                                                                "10\n"
                                                                "qsdb> .continue"));
    }

    {
        QString script;
        script.append("function bar(i) {\n");
        for (int i = 0; i < 10; ++i)
            script.append(QString::fromLatin1("    i = i + %0;\n").arg(i));
        script.append("    return i;\n}");
        engine.evaluate(script, "bar.qs");
    }

    outputEdit->clear();
    executeConsoleCommand(inputEdit, outputEdit, ".break bar.qs:7");

    {
        DebuggerCommandExecutor executor(inputEdit, outputEdit, QStringList()
                                         << ".list"
                                         << ".up"
                                         << ".list"
                                         << ".frame"
                                         << ".down"
                                         << ".list"
                                         << ".continue");
        QObject::connect(&debugger, SIGNAL(evaluationSuspended()), &executor, SLOT(execute()), Qt::QueuedConnection);
        outputEdit->clear();
        engine.evaluate("bar(123);", "testscript.qs");
        QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("Breakpoint 5 at bar.qs, line 7.\n"
                                                                "7\t    i = i + 5;\n"
                                                                "qsdb> .list\n"
                                                                "2\t    i = i + 0;\n"
                                                                "3\t    i = i + 1;\n"
                                                                "4\t    i = i + 2;\n"
                                                                "5\t    i = i + 3;\n"
                                                                "6\t    i = i + 4;\n"
                                                                "7\t    i = i + 5;\n"
                                                                "8\t    i = i + 6;\n"
                                                                "9\t    i = i + 7;\n"
                                                                "10\t    i = i + 8;\n"
                                                                "11\t    i = i + 9;\n"
                                                                "qsdb> .up\n"
                                                                "#1  <global>()@testscript.qs:1\n"
                                                                "qsdb> .list\n"
                                                                "1\tbar(123);\n"
                                                                "2\n"
                                                                "3\n"
                                                                "4\n"
                                                                "5\n"
                                                                "6\n"
                                                                "7\n"
                                                                "8\n"
                                                                "9\n"
                                                                "10\n"
                                                                "qsdb> .frame\n"
                                                                "#1  <global>()@testscript.qs:1\n"
                                                                "qsdb> .down\n"
                                                                "#0  bar(123)@bar.qs:7\n"
                                                                "qsdb> .list\n"
                                                                "2\t    i = i + 0;\n"
                                                                "3\t    i = i + 1;\n"
                                                                "4\t    i = i + 2;\n"
                                                                "5\t    i = i + 3;\n"
                                                                "6\t    i = i + 4;\n"
                                                                "7\t    i = i + 5;\n"
                                                                "8\t    i = i + 6;\n"
                                                                "9\t    i = i + 7;\n"
                                                                "10\t    i = i + 8;\n"
                                                                "11\t    i = i + 9;\n"
                                                                "qsdb> .continue"));
    }

    {
        DebuggerCommandExecutor executor(inputEdit, outputEdit, QStringList()
                                         << ".list 9"
                                         << ".continue");
        QObject::connect(&debugger, SIGNAL(evaluationSuspended()), &executor, SLOT(execute()), Qt::QueuedConnection);
        outputEdit->clear();
        engine.evaluate("bar(123);", "testscript.qs");
        QCOMPARE(outputEdit->toPlainText(), QString::fromLatin1("Breakpoint 5 at bar.qs, line 7.\n"
                                                                "7\t    i = i + 5;\n"
                                                                "qsdb> .list 9\n"
                                                                "4\t    i = i + 2;\n"
                                                                "5\t    i = i + 3;\n"
                                                                "6\t    i = i + 4;\n"
                                                                "7\t    i = i + 5;\n"
                                                                "8\t    i = i + 6;\n"
                                                                "9\t    i = i + 7;\n"
                                                                "10\t    i = i + 8;\n"
                                                                "11\t    i = i + 9;\n"
                                                                "12\t    return i;\n"
                                                                "13\t}\n"
                                                                "qsdb> .continue"));
    }
}

class ScriptEvaluator : public QObject
{
    Q_OBJECT
public:
    ScriptEvaluator(QObject *parent = 0)
        : QObject(parent) {
        m_engine = new QScriptEngine(this);
    }
    QScriptEngine *engine() const {
        return m_engine;
    }
public Q_SLOTS:
    QScriptValue evaluate(const QString &program) {
        return m_engine->evaluate(program);
    }
private:
    QScriptEngine *m_engine;
};

void tst_QScriptEngineDebugger::multithreadedDebugging()
{
#ifdef Q_OS_WINCE
    QSKIP("This tests uses too much memory for Windows CE");
#endif
    ScriptEvaluator eval;
    QThread thread;
    eval.moveToThread(&thread);
    eval.engine()->moveToThread(&thread);
    QScriptEngineDebugger debugger;
    QSignalSpy evaluationSuspendedSpy(&debugger, SIGNAL(evaluationSuspended()));
    QSignalSpy evaluationResumedSpy(&debugger, SIGNAL(evaluationResumed()));
    debugger.attachTo(eval.engine());
    QMetaObject::invokeMethod(&eval, "evaluate", Qt::QueuedConnection, Q_ARG(QString, "debugger"));
    QSignalSpy threadFinishedSpy(&thread, SIGNAL(finished()));
    thread.start();
    QTRY_COMPARE(evaluationSuspendedSpy.count(), 1);
    QTRY_COMPARE(evaluationResumedSpy.count(), 0);
    thread.quit();
    QTRY_COMPARE(threadFinishedSpy.count(), 1);
}

void tst_QScriptEngineDebugger::autoShowStandardWindow()
{
    {
        QScriptEngine engine;
        QScriptEngineDebugger debugger;
        QCOMPARE(debugger.autoShowStandardWindow(), true);
        debugger.attachTo(&engine);
        QObject::connect(&debugger, SIGNAL(evaluationSuspended()),
                         debugger.action(QScriptEngineDebugger::ContinueAction),
                         SLOT(trigger()));
        engine.evaluate("debugger");
        QTRY_VERIFY(debugger.standardWindow()->isVisible());

        debugger.setAutoShowStandardWindow(true);
        QCOMPARE(debugger.autoShowStandardWindow(), true);

        debugger.setAutoShowStandardWindow(false);
        QCOMPARE(debugger.autoShowStandardWindow(), false);

        debugger.setAutoShowStandardWindow(true);
        QCOMPARE(debugger.autoShowStandardWindow(), true);

        debugger.standardWindow()->hide();

        engine.evaluate("debugger");
        QTRY_VERIFY(debugger.standardWindow()->isVisible());
    }

    {
        QScriptEngine engine;
        QScriptEngineDebugger debugger;
        debugger.setAutoShowStandardWindow(false);
        debugger.attachTo(&engine);
        QObject::connect(&debugger, SIGNAL(evaluationSuspended()),
                         debugger.action(QScriptEngineDebugger::ContinueAction),
                         SLOT(trigger()));
        QSignalSpy evaluationResumedSpy(&debugger, SIGNAL(evaluationResumed()));
        engine.evaluate("debugger");
        QTRY_COMPARE(evaluationResumedSpy.count(), 1);
        QVERIFY(!debugger.standardWindow()->isVisible());
    }
}

void tst_QScriptEngineDebugger::standardWindowOwnership()
{
    QScriptEngine engine;
    QPointer<QMainWindow> win;
    {
        QScriptEngineDebugger debugger;
        win = debugger.standardWindow();
    }
    QVERIFY(win == 0);

    // Reparent the window.
    QWidget widget;
    {
        QScriptEngineDebugger debugger;
        win = debugger.standardWindow();
        win->setParent(&widget);
    }
    QVERIFY(win != 0);
}

void tst_QScriptEngineDebugger::engineDeleted()
{
    QScriptEngine* engine = new QScriptEngine;
    QScriptEngineDebugger *debugger = new QScriptEngineDebugger;
    debugger->attachTo(engine);

    debugger->standardWindow()->show();
    QTest::qWaitForWindowShown(debugger->standardWindow());

    QSignalSpy destroyedSpy(engine, SIGNAL(destroyed()));
    engine->deleteLater();
    QTRY_COMPARE(destroyedSpy.count(), 1);

    // Shouldn't crash (QTBUG-21548)
    debugger->action(QScriptEngineDebugger::ContinueAction)->trigger();
}

QTEST_MAIN(tst_QScriptEngineDebugger)
#include "tst_qscriptenginedebugger.moc"
