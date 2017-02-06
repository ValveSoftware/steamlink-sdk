/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#include "mainwindow.h"
#include "changeproperties.h"
#include "invokemethod.h"
#include "ambientproperties.h"
#include "controlinfo.h"
#include "docuwindow.h"

#include <QtWidgets/QMdiArea>
#include <QtWidgets/QMdiSubWindow>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QMessageBox>
#include <QtGui/QCloseEvent>
#include <QtCore/QDebug>
#include <QtCore/qt_windows.h>
#include <ActiveQt/QAxScriptManager>
#include <ActiveQt/QAxSelect>
#include <ActiveQt/QAxWidget>
#include <ActiveQt/qaxtypes.h>

QT_BEGIN_NAMESPACE

QT_END_NAMESPACE

QT_USE_NAMESPACE

struct ScriptLanguage {
    const char *name;
    const char *suffix;
};

static const ScriptLanguage scriptLanguages[] = {
    {"PerlScript", ".pl"},
    {"Python", ".py"}
};

MainWindow *MainWindow::m_instance = Q_NULLPTR;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_dlgInvoke(Q_NULLPTR)
    , m_dlgProperties(Q_NULLPTR)
    , m_dlgAmbient(Q_NULLPTR)
    , m_scripts(Q_NULLPTR)
{
    setupUi(this);
    MainWindow::m_instance = this; // Logging handler needs the UI

    setObjectName(QLatin1String("MainWindow"));

    const int scriptCount = int(sizeof(scriptLanguages) / sizeof(scriptLanguages[0]));
    for (int s = 0; s < scriptCount; ++s) {
        const QString name = QLatin1String(scriptLanguages[s].name);
        const QString suffix = QLatin1String(scriptLanguages[s].suffix);
        if (!QAxScriptManager::registerEngine(name, suffix))
            qWarning().noquote().nospace() << "Failed to register \"" << name
                << "\" (*" << suffix << ") with QAxScriptManager.";
    }

    QHBoxLayout *layout = new QHBoxLayout(Workbase);
    m_mdiArea = new QMdiArea(Workbase);
    layout->addWidget(m_mdiArea);
    layout->setMargin(0);

    connect(m_mdiArea, &QMdiArea::subWindowActivated, this, &MainWindow::updateGUI);
    connect(actionFileExit, &QAction::triggered, QCoreApplication::quit);
}

MainWindow::~MainWindow()
{
    MainWindow::m_instance = Q_NULLPTR;
}

QAxWidget *MainWindow::activeAxWidget() const
{
    if (const QMdiSubWindow *activeSubWindow = m_mdiArea->currentSubWindow())
        return qobject_cast<QAxWidget*>(activeSubWindow->widget());
    return 0;
}

QList<QAxWidget *> MainWindow::axWidgets() const
{
    QList<QAxWidget *> result;
    foreach (const QMdiSubWindow *subWindow, m_mdiArea->subWindowList())
        if (QAxWidget *axWidget = qobject_cast<QAxWidget *>(subWindow->widget()))
            result.push_back(axWidget);
    return result;
}

bool MainWindow::addControlFromClsid(const QString &clsid)
{
    QAxWidget *container = new QAxWidget;
    const bool result = container->setControl(clsid);
    if (result) {
        container->setObjectName(container->windowTitle());
        m_mdiArea->addSubWindow(container);
        container->show();
        updateGUI();
    } else {
        delete container;
        QMessageBox::information(this,
                                 tr("Error Loading Control"),
                                 tr("The control \"%1\" could not be loaded.").arg(clsid));
    }
    return result;
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    // Controls using the same version of Qt may set this to false, causing hangs.
    QGuiApplication::setQuitOnLastWindowClosed(true);
    m_mdiArea->closeAllSubWindows();
    e->accept();
}

void MainWindow::appendLogText(const QString &message)
{
    logDebug->append(message);
}

void MainWindow::on_actionFileNew_triggered()
{
    QAxSelect select(this);
    while (select.exec() && !addControlFromClsid(select.clsid())) { }
}

void MainWindow::on_actionFileLoad_triggered()
{
    while (true) {
        const QString fname = QFileDialog::getOpenFileName(this, tr("Load"), QString(), QLatin1String("*.qax"));
        if (fname.isEmpty() || addControlFromFile(fname))
            break;
    }
}

bool MainWindow::addControlFromFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::information(this,
                                 tr("Error Loading File"),
                                 tr("The file could not be opened for reading.\n%1\n%2")
                                 .arg(QDir::toNativeSeparators(fileName), file.errorString()));
        return false;
    }

    QAxWidget *container = new QAxWidget(m_mdiArea);
    container->setObjectName(container->windowTitle());

    QDataStream d(&file);
    d >> *container;

    m_mdiArea->addSubWindow(container);
    container->show();

    updateGUI();
    return true;
}

void MainWindow::on_actionFileSave_triggered()
{
    QAxWidget *container = activeAxWidget();
    if (!container)
        return;

    QString fname = QFileDialog::getSaveFileName(this, tr("Save"), QString(), QLatin1String("*.qax"));
    if (fname.isEmpty())
        return;

    QFile file(fname);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::information(this, tr("Error Saving File"), tr("The file could not be opened for writing.\n%1").arg(fname));
        return;
    }
    QDataStream d(&file);
    d << *container;
}


void MainWindow::on_actionContainerSet_triggered()
{
    QAxWidget *container = activeAxWidget();
    if (!container)
        return;

    QAxSelect select(this);
    if (select.exec())
        container->setControl(select.clsid());
    updateGUI();
}

void MainWindow::on_actionContainerClear_triggered()
{
    QAxWidget *container = activeAxWidget();
    if (container)
        container->clear();
    updateGUI();
}

void MainWindow::on_actionContainerProperties_triggered()
{
    if (!m_dlgAmbient) {
        m_dlgAmbient = new AmbientProperties(this);
        m_dlgAmbient->setControl(m_mdiArea);
    }
    m_dlgAmbient->show();
}


void MainWindow::on_actionControlInfo_triggered()
{
    QAxWidget *container = activeAxWidget();
    if (!container)
        return;

    ControlInfo info(this);
    info.setControl(container);
    info.exec();
}

void MainWindow::on_actionControlProperties_triggered()
{
    QAxWidget *container = activeAxWidget();
    if (!container)
        return;

    if (!m_dlgProperties) {
        m_dlgProperties = new ChangeProperties(this);
        connect(container, SIGNAL(propertyChanged(QString)), m_dlgProperties, SLOT(updateProperties()));
    }
    m_dlgProperties->setControl(container);
    m_dlgProperties->show();
}

void MainWindow::on_actionControlMethods_triggered()
{
    QAxWidget *container = activeAxWidget();
    if (!container)
        return;

    if (!m_dlgInvoke)
        m_dlgInvoke = new InvokeMethod(this);
    m_dlgInvoke->setControl(container);
    m_dlgInvoke->show();
}

void MainWindow::on_VerbMenu_aboutToShow()
{
    VerbMenu->clear();

    QAxWidget *container = activeAxWidget();
    if (!container)
        return;

    QStringList verbs = container->verbs();
    for (int i = 0; i < verbs.count(); ++i) {
        VerbMenu->addAction(verbs.at(i));
    }

    if (!verbs.count()) { // no verbs?
        VerbMenu->addAction(tr("-- Object does not support any verbs --"))->setEnabled(false);
    }
}

void MainWindow::on_VerbMenu_triggered(QAction *action)
{
    QAxWidget *container = activeAxWidget();
    if (!container)
        return;

    container->doVerb(action->text());
}

void MainWindow::on_actionControlDocumentation_triggered()
{
    QAxWidget *container = activeAxWidget();
    if (!container)
        return;

    const QString docu = container->generateDocumentation();
    if (docu.isEmpty())
        return;

    DocuWindow *docwindow = new DocuWindow(docu);
    QMdiSubWindow *subWindow = m_mdiArea->addSubWindow(docwindow);
    subWindow->setWindowTitle(DocuWindow::tr("%1 - Documentation").arg(container->windowTitle()));
    docwindow->show();
}

void MainWindow::on_actionControlPixmap_triggered()
{
    QAxWidget *container = activeAxWidget();
    if (!container)
        return;

    QLabel *label = new QLabel;
    label->setPixmap(QPixmap::grabWidget(container));
    QMdiSubWindow *subWindow = m_mdiArea->addSubWindow(label);
    subWindow->setWindowTitle(tr("%1 - Pixmap").arg(container->windowTitle()));
    label->show();
}

void MainWindow::on_actionScriptingRun_triggered()
{
#ifndef QT_NO_QAXSCRIPT
    if (!m_scripts)
        return;

    // If we have only one script loaded we can use the cool dialog
    QStringList scriptList = m_scripts->scriptNames();
    if (scriptList.count() == 1) {
        InvokeMethod scriptInvoke(this);
        scriptInvoke.setWindowTitle(tr("Execute Script Function"));
        scriptInvoke.setControl(m_scripts->script(scriptList[0])->scriptEngine());
        scriptInvoke.exec();
        return;
    }

    bool ok = false;
    QStringList macroList = m_scripts->functions(QAxScript::FunctionNames);
    QString macro = QInputDialog::getItem(this, tr("Select Macro"), tr("Macro:"), macroList, 0, true, &ok);

    if (!ok)
        return;

    QVariant result = m_scripts->call(macro);
    if (result.isValid())
        logMacros->append(tr("Return value of %1: %2").arg(macro).arg(result.toString()));
#endif
}

void MainWindow::on_actionFreeUnusedDLLs_triggered()
{
    // Explicitly unload unused DLLs with no remaining references.
    // This is also done automatically after 10min and in low memory situations.

    // must call twice due to DllCanUnloadNow implementation in qaxserverdll
    CoFreeUnusedLibrariesEx(0, 0);
    CoFreeUnusedLibrariesEx(0, 0);
}

#ifdef QT_NO_QAXSCRIPT
static inline void noScriptMessage(QWidget *parent)
{
    QMessageBox::information(parent, MainWindow::tr("Function not available"),
                             MainWindow::tr("QAxScript functionality is not available with this compiler."));
}
#endif // !QT_NO_QAXSCRIPT

void MainWindow::on_actionScriptingLoad_triggered()
{
#ifndef QT_NO_QAXSCRIPT
    QString file = QFileDialog::getOpenFileName(this, tr("Open Script"), QString(), QAxScriptManager::scriptFileFilter());

    if (!file.isEmpty())
        loadScript(file);
#else // !QT_NO_QAXSCRIPT
    noScriptMessage(this);
#endif
}

bool MainWindow::loadScript(const QString &file)
{
#ifndef QT_NO_QAXSCRIPT
    if (!m_scripts) {
        m_scripts = new QAxScriptManager(this);
        m_scripts->addObject(this);
    }

    foreach (QAxWidget *axWidget, axWidgets()) {
        QAxBase *ax = axWidget;
        m_scripts->addObject(ax);
    }

    QAxScript *script = m_scripts->load(file, file);
    if (script) {
        connect(script, &QAxScript::error, this, &MainWindow::logMacro);
        actionScriptingRun->setEnabled(true);
    }
    return script;
#else // !QT_NO_QAXSCRIPT
    Q_UNUSED(file)
    noScriptMessage(this);
    return false;
#endif
}

void MainWindow::updateGUI()
{
    QAxWidget *container = activeAxWidget();

    bool hasControl = container && !container->isNull();
    actionFileNew->setEnabled(true);
    actionFileLoad->setEnabled(true);
    actionFileSave->setEnabled(hasControl);
    actionContainerSet->setEnabled(container != 0);
    actionContainerClear->setEnabled(hasControl);
    actionControlProperties->setEnabled(hasControl);
    actionControlMethods->setEnabled(hasControl);
    actionControlInfo->setEnabled(hasControl);
    actionControlDocumentation->setEnabled(hasControl);
    actionControlPixmap->setEnabled(hasControl);
    VerbMenu->setEnabled(hasControl);
    if (m_dlgInvoke)
        m_dlgInvoke->setControl(hasControl ? container : 0);
    if (m_dlgProperties)
        m_dlgProperties->setControl(hasControl ? container : 0);

    foreach (QAxWidget *container, axWidgets()) {
        container->disconnect(SIGNAL(signal(QString,int,void*)));
        if (actionLogSignals->isChecked())
            connect(container, SIGNAL(signal(QString,int,void*)), this, SLOT(logSignal(QString,int,void*)));

        container->disconnect(SIGNAL(exception(int,QString,QString,QString)));
        connect(container, SIGNAL(exception(int,QString,QString,QString)),
                this, SLOT(logException(int,QString,QString,QString)));

        container->disconnect(SIGNAL(propertyChanged(QString)));
        if (actionLogProperties->isChecked())
            connect(container, SIGNAL(propertyChanged(QString)), this, SLOT(logPropertyChanged(QString)));
        container->blockSignals(actionFreezeEvents->isChecked());
    }
}

void MainWindow::logPropertyChanged(const QString &prop)
{
    QAxWidget *container = activeAxWidget();
    if (!container)
        return;

    QVariant var = container->property(prop.toLatin1());
    logProperties->append(tr("%1: Property Change: %2 - { %3 }").arg(container->windowTitle(), prop, var.toString()));
}

void MainWindow::logSignal(const QString &signal, int argc, void *argv)
{
    QAxWidget *container = activeAxWidget();
    if (!container)
        return;

    QString paramlist = QLatin1String(" - {");
    VARIANT *params = (VARIANT*)argv;
    for (int a = argc-1; a >= 0; --a) {
        paramlist += QLatin1Char(' ');
        paramlist += VARIANTToQVariant(params[a], 0).toString();
        paramlist += a > 0 ? QLatin1Char(',') : QLatin1Char(' ');
    }
    if (argc)
        paramlist += QLatin1Char('}');
    logSignals->append(container->windowTitle() + QLatin1String(": ") + signal + paramlist);
}

void MainWindow::logException(int code, const QString&source, const QString&desc, const QString&help)
{
    Q_UNUSED(desc);
    QAxWidget *container = qobject_cast<QAxWidget*>(sender());
    if (!container)
        return;

    QString str = tr("%1: Exception code %2 thrown by %3").
        arg(container->windowTitle()).arg(code).arg(source);
    logDebug->append(str);
    logDebug->append(tr("\tDescription: %1").arg(desc));

    if (!help.isEmpty())
        logDebug->append(tr("\tHelp available at %1").arg(help));
    else
        logDebug->append(tr("\tNo help available."));
}

void MainWindow::logMacro(int code, const QString &description, int sourcePosition, const QString &sourceText)
{
    /* FIXME This needs to be rewritten to not use string concatentation, such
     * that it can be translated in a sane way. */
    QString message = tr("Script: ");
    if (code)
        message += QString::number(code) + QLatin1Char(' ');

    const QChar singleQuote = QLatin1Char('\'');
    message += singleQuote + description + singleQuote;
    if (sourcePosition)
        message += tr(" at position ") + QString::number(sourcePosition);
    if (!sourceText.isEmpty())
        message += QLatin1String(" '") + sourceText + singleQuote;

    logMacros->append(message);
}
