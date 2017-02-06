/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include <ActiveQt/QAxSelect>
#include <ActiveQt/QAxWidget>

#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QToolBar>

#include <QtCore/QCommandLineOption>
#include <QtCore/QCommandLineParser>
#include <QtCore/QDebug>
#include <QtCore/QStringList>
#include <QtCore/QSysInfo>

#ifdef QT_DIAG_LIB
#  include <qwidgetdump.h>
#  include <nativewindowdump.h>
#  include <qwindowdump.h>
#endif

#include <algorithm>
#include <iterator>

QT_USE_NAMESPACE

static inline bool isOptionSet(int argc, char *argv[], const char *option)
{
    return (argv + argc) !=
        std::find_if(argv + 1, argv + argc,
                     [option] (const char *arg) { return !qstrcmp(arg, option); });
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow();
    bool setControl(const QString &clsid);

private:
    QAxWidget *m_axWidget;
};

MainWindow::MainWindow()
    : m_axWidget(new QAxWidget)
{
    const QString title = QGuiApplication::applicationDisplayName() + QLatin1String(" Qt ")
        + QLatin1String(QT_VERSION_STR) + QLatin1String(", ")
        + QString::number(QSysInfo::WordSize) + QLatin1String("bit");
    setWindowTitle(title);

    setObjectName(QLatin1String("MainWindow"));
    m_axWidget->setObjectName(QLatin1String("AxWidget"));

    setCentralWidget(m_axWidget);

    QMenu *fileMenu = menuBar()->addMenu(QLatin1String("File"));
    fileMenu->setObjectName(QLatin1String("FileMenu"));
    QToolBar *toolbar = new QToolBar;
    toolbar->setObjectName(QLatin1String("ToolBar"));
    addToolBar(Qt::TopToolBarArea, toolbar);

    QAction *action;
#ifdef QT_DIAG_LIB
    action = fileMenu->addAction("Dump Widgets",
                                 this, [] () { QtDiag::dumpAllWidgets(); });
    toolbar->addAction(action);
    action = fileMenu->addAction("Dump Windows",
                                 this, [] () { QtDiag::dumpAllWindows(); });
    toolbar->addAction(action);
    action = fileMenu->addAction("Dump Native Windows",
                                 this, [this] () { QtDiag::dumpNativeWindows(winId()); });
    toolbar->addAction(action);
    fileMenu->addSeparator();
#endif // QT_DIAG_LIB
    action = fileMenu->addAction("Quit", qApp, &QCoreApplication::quit);
    toolbar->addAction(action);
    action->setShortcut(Qt::CTRL + Qt::Key_Q);
}

bool MainWindow::setControl(const QString &clsid)
{
    const bool result = m_axWidget->setControl(clsid);
    if (result)
        statusBar()->showMessage(QLatin1String("Loaded ") + clsid);
    return result;
}

int main(int argc, char* argv[])
{
    if (isOptionSet(argc, argv, "-s"))
        QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
    if (!isOptionSet(argc, argv, "-n"))
        QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    QApplication app(argc, argv);
    QCoreApplication::setApplicationVersion(QLatin1String(QT_VERSION_STR));
    QGuiApplication::setApplicationName("Ax Viewer");

    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption noScalingDummy(QStringLiteral("s"),
                                      QStringLiteral("Set Qt::AA_DisableHighDpiScaling."));
    parser.addOption(noScalingDummy);
    QCommandLineOption nativeSiblingsDummy(QStringLiteral("n"),
                                           QStringLiteral("Do not set Qt::AA_DontCreateNativeWidgetSiblings."));
    parser.addOption(nativeSiblingsDummy);
    parser.addPositionalArgument(QStringLiteral("[clsid]"), QStringLiteral("Class ID"));

    parser.process(QCoreApplication::arguments());

    QString clsid = parser.positionalArguments().value(0, QString());
    if (clsid.isEmpty()) {
        QAxSelect select;
        if (select.exec() != QDialog::Accepted)
            return 0;
        clsid = select.clsid();
    }

    MainWindow mainWindow;

    qDebug() << QT_VERSION_STR << "Loading" << clsid;

    if (!mainWindow.setControl(clsid)) {
        qWarning().noquote().nospace() << "Failed to set \"" << clsid << '"';
        return -1;
    }

    mainWindow.show();
    return app.exec();
}

#include "main.moc"
