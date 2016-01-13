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
#include <QQmlEngine>
#include <QQmlComponent>
#include <QDebug>
#include <QApplication>
#include <QTime>
#include <QQmlContext>
#include <QGraphicsScene>
#include <QGraphicsRectItem>

class Timer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlComponent *component READ component WRITE setComponent)

public:
    Timer();

    QQmlComponent *component() const;
    void setComponent(QQmlComponent *);

    static Timer *timerInstance();

    void run(uint);

    bool willParent() const;
    void setWillParent(bool p);

private:
    void runTest(QQmlContext *, uint);

    QQmlComponent *m_component;
    static Timer *m_timer;

    bool m_willparent;
    QGraphicsScene m_scene;
    QGraphicsRectItem m_item;
};
QML_DECLARE_TYPE(Timer);

Timer *Timer::m_timer = 0;

Timer::Timer()
: m_component(0), m_willparent(false)
{
    if (m_timer)
        qWarning("Timer: Timer already registered");
    m_timer = this;

    m_scene.setItemIndexMethod(QGraphicsScene::NoIndex);
    m_scene.addItem(&m_item);
}

QQmlComponent *Timer::component() const
{
    return m_component;
}

void Timer::setComponent(QQmlComponent *c)
{
    m_component = c;
}

Timer *Timer::timerInstance()
{
    return m_timer;
}

void Timer::run(uint iterations)
{
    QQmlContext context(qmlContext(this));

    QObject *o = m_component->create(&context);
    QGraphicsObject *go = qobject_cast<QGraphicsObject *>(o);
    if (m_willparent && go)
        go->setParentItem(&m_item);
    delete o;

    runTest(&context, iterations);
}

bool Timer::willParent() const
{
    return m_willparent;
}

void Timer::setWillParent(bool p)
{
    m_willparent = p;
}

void Timer::runTest(QQmlContext *context, uint iterations)
{
    QTime t;
    t.start();
    for (uint ii = 0; ii < iterations; ++ii) {
        QObject *o = m_component->create(context);
        QGraphicsObject *go = qobject_cast<QGraphicsObject *>(o);
        if (m_willparent && go)
            go->setParentItem(&m_item);
        delete o;
    }

    int e = t.elapsed();

    qWarning() << "Total:" << e << "ms, Per iteration:" << qreal(e) / qreal(iterations) << "ms";

}

void usage(const char *name)
{
    qWarning("Usage: %s [-iterations <count>] [-parent] <qml file>\n", name);

    qWarning("qmltime is a tool for benchmarking the runtime cost of instantiating\n"
    "a QML component. It is typically run as follows:\n"
    "\n"
    "%s path/to/benchmark.qml\n"
    "\n"
    "If the -parent option is specified, the component being measured will also\n"
    "be parented to an item already in the scene.\n"
    "\n"
    "If the -iterations option is specified, the benchmark will run the specified\n"
    "number of iterations. If -iterations is not specified, 1024 iterations\n"
    "are performed.\n"
    "\n"
    "qmltime expects the file to be benchmarked to contain a certain structure.\n"
    "Specifically, it requires the presence of a QmlTime.Timer element. For example,\n"
    "say we wanted to benchmark the following list delegate:\n"
    "\n"
    "Rectangle {\n"
    "    color: \"green\"\n"
    "    width: 400; height: 100\n"
    "    Text {\n"
    "        anchors.centerIn: parent\n"
    "        text: name\n"
    "    }\n"
    "}\n"
    "\n"
    "we would create a benchmark file that looks like this:\n"
    "\n"
    "import QtQuick 2.0\n"
    "import QmlTime 1.0 as QmlTime\n"
    "\n"
    "Item {\n"
    "\n"
    "    property string name: \"Bob Smith\"\n"
    "\n"
    "    QmlTime.Timer {\n"
    "        component: Rectangle {\n"
    "            color: \"green\"\n"
    "            width: 400; height: 100\n"
    "            Text {\n"
    "                anchors.centerIn: parent\n"
    "                text: name\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "}\n"
    "\n"
    "The outer Item functions as a dummy data provider for any additional\n"
    "data required by the bindings in the component being benchmarked (in the\n"
    "example above we provide a \"name\" property).\n"
    "\n"
    "When started, the component is instantiated once before running\n"
    "the benchmark, which means that the reported time does not include\n"
    "compile time (as the results of compilation are cached internally).\n"
    "In this sense the times reported by qmltime best correspond to the\n"
    "costs associated with delegate creation in the view classes, where the\n"
    "same delegate is instantiated over and over. Conversely, it is not a\n"
    "good approximation for e.g. Loader, which typically only instantiates\n"
    "an element once (and so for Loader the compile time is very relevant\n"
    "to the overall cost).", name);

    exit(-1);
}

int main(int argc, char ** argv)
{
    QApplication app(argc, argv);

    qmlRegisterType<Timer>("QmlTime", 1, 0, "Timer");

    uint iterations = 1024;
    QString filename;
    bool willParent = false;

    for (int ii = 1; ii < argc; ++ii) {
        QByteArray arg(argv[ii]);

        if (arg == "-iterations") {
            if (ii + 1 < argc) {
                ++ii;
                QByteArray its(argv[ii]);
                bool ok = false;
                iterations = its.toUInt(&ok);
                if (!ok)
                    usage(argv[0]);
            } else {
                usage(argv[0]);
            }
        } else if (arg == "-parent") {
            willParent = true;
        } else if (arg == "-help") {
            usage(argv[0]);
        } else {
            filename = QLatin1String(argv[ii]);
        }
    }

    if (filename.isEmpty())
        usage(argv[0]);

    QQmlEngine engine;
    QQmlComponent component(&engine, filename);
    if (component.isError()) {
        qWarning() << component.errors();
        return -1;
    }

    QObject *obj = component.create();
    if (!obj) {
        qWarning() << component.errors();
        return -1;
    }

    Timer *timer = Timer::timerInstance();
    if (!timer) {
        qWarning() << "A Tester.Timer instance is required.";
        return -1;
    }

    timer->setWillParent(willParent);

    if (!timer->component()) {
        qWarning() << "The timer has no component";
        return -1;
    }

    timer->run(iterations);

    return 0;
}

#include "qmltime.moc"
