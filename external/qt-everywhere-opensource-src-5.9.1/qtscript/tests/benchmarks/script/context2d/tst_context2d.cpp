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

#include <qtest.h>
#include <QtCore/qdir.h>
#include <QtCore/qfile.h>
#include <QtCore/qtextstream.h>
#include <QtScript/qscriptengine.h>
#include <QtScript/qscriptvalue.h>
#include "context2d.h"
#include "environment.h"
#include "qcontext2dcanvas.h"

static QString readFile(const QString &filename)
{
    QFile file(filename);
    if (!file.open(QFile::ReadOnly))
        return QString();
    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    return stream.readAll();
}

class tst_Context2D : public QObject
{
    Q_OBJECT

public:
    tst_Context2D();
    ~tst_Context2D();

private slots:
    void singleExecution_data();
    void singleExecution();
    void repeatedExecution_data();
    void repeatedExecution();

private:
    void newEnvironment();

private:
    QDir testsDir;
    Environment *m_env;
    QContext2DCanvas *m_canvas;
};

tst_Context2D::tst_Context2D()
    : m_env(0), m_canvas(0)
{
    testsDir = QDir(":/scripts");
    if (!testsDir.exists())
        qWarning("*** no scripts/ dir!");
}

tst_Context2D::~tst_Context2D()
{
    delete m_canvas;
    delete m_env;
}

void tst_Context2D::newEnvironment()
{
    delete m_canvas;
    delete m_env;
    m_env = new Environment();
    Context2D *context = new Context2D(m_env);
    context->setSize(150, 150); // Hard-coded in many of the scripts.
    m_canvas = new QContext2DCanvas(context, m_env);
    m_canvas->setFixedSize(context->size());
    m_canvas->setObjectName("tutorial"); // Acts as the DOM element ID.
    m_env->addCanvas(m_canvas);
}

void tst_Context2D::singleExecution_data()
{
    QTest::addColumn<QString>("testName");
    const QFileInfoList testFileInfos = testsDir.entryInfoList(QStringList() << "*.js", QDir::Files);
    for (const QFileInfo &tfi : testFileInfos) {
        QString name = tfi.baseName();
        QTest::newRow(name.toLatin1().constData()) << name;
    }
}

void tst_Context2D::singleExecution()
{
    QFETCH(QString, testName);
    QString script = readFile(testsDir.absoluteFilePath(testName + ".js"));
    QVERIFY(!script.isEmpty());

    newEnvironment();
    QBENCHMARK {
        m_env->evaluate(script, testName);
        // Some of the scripts (e.g. plasma.js) merely start a timer and do
        // the actual drawing in the timer event. Trigger the timers now to
        // ensure that the real work is done.
        m_env->triggerTimers();
    }
    QVERIFY(!m_env->engine()->hasUncaughtException());
}

void tst_Context2D::repeatedExecution_data()
{
    // We look for scripts that register an interval timer.
    // Such scripts run a function every n milliseconds to update the canvas.
    // The benchmark will execute this function repeatedly, which can allow
    // us to observe potential effects of profiling-based JIT optimizations.
    QTest::addColumn<QString>("testName");
    QTest::addColumn<QString>("script");
    const QFileInfoList testFileInfos = testsDir.entryInfoList(QStringList() << "*.js", QDir::Files);
    for (const QFileInfo &tfi : testFileInfos) {
        QString script = readFile(tfi.absoluteFilePath());
        QString name = tfi.baseName();
        newEnvironment();
        m_env->evaluate(script, name);
        if (m_env->engine()->hasUncaughtException())
            continue;
        if (m_env->hasIntervalTimers())
            QTest::newRow(name.toLatin1().constData()) << name << script;
    }
}

void tst_Context2D::repeatedExecution()
{
    QFETCH(QString, testName);
    QFETCH(QString, script);

    newEnvironment();
    m_env->evaluate(script, testName);
    QBENCHMARK {
        // Trigger the update function repeatedly, effectively
        // performing several frames of animation.
        for (int i = 0; i < 16; ++i)
            m_env->triggerTimers();
    }
    QVERIFY(!m_env->engine()->hasUncaughtException());
}

QTEST_MAIN(tst_Context2D)
#include "tst_context2d.moc"
