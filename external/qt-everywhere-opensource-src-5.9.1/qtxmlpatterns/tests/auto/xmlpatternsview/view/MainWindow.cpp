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

#include <QCloseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QSettings>
#include <QTextStream>

#include "ASTItem.h"
#include "FunctionSignaturesView.h"
#include "Global.h"
#include "TestCaseView.h"
#include "TestResultView.h"
#include "TestSuite.h"
#include "TreeModel.h"
#include "TreeSortFilter.h"
#include "UserTestCase.h"

#include "MainWindow.h"

using namespace QPatternistSDK;

MainWindow::MainWindow() : m_userTC(new UserTestCase()),
                           m_currentTC(0)
{
    setupUi(this);

    /* I want to do this in Designer.. */
    testSuiteView->header()->setSortIndicator(0, Qt::AscendingOrder);
    testSuiteView->header()->setSortIndicatorShown(true);
    testSuiteView->header()->setClickable(true);

    setupActions();

    QStringList suiteHeaders;
    suiteHeaders << QLatin1String("Name")
                 << QLatin1String("Pass")
                 << QLatin1String("Fail")
                 << QLatin1String("Total");

    TreeSortFilter *const proxy = new TreeSortFilter(this);
    connect(searchInput,    SIGNAL(textChanged(const QString &)),
            proxy,          SLOT(setFilterFixedString(const QString &)));

    proxy->setSourceModel(new TreeModel(suiteHeaders, this));
    testSuiteView->setModel(proxy);

    /* --------- Test Result View ---------- */
    testResultView = new TestResultView(this);
    testResultView->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::RightDockWidgetArea, testResultView);
    /* ----------------------------------- */

    /* --------- Test Case View ---------- */
    testCaseView = new TestCaseView(this);
    testCaseView->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::LeftDockWidgetArea, testCaseView);

    connect(this,           SIGNAL(testCaseSelected(TestCase *const)),
            testCaseView,   SLOT(displayTestCase(TestCase *const)));
    connect(this,           SIGNAL(testCaseSelected(TestCase *const)),
            testResultView, SLOT(displayTestResult(TestCase *const)));
    connect(focusURI,       SIGNAL(textChanged(const QString &)),
            m_userTC,       SLOT(focusDocumentChanged(const QString &)));
    /* ----------------------------------- */

    /* ----- Function Signature View ----- */
    functionView = new FunctionSignaturesView(this);
    functionView->setAllowedAreas(Qt::AllDockWidgetAreas);
    addDockWidget(Qt::RightDockWidgetArea, functionView);
    /* ----------------------------------- */

    /* Appears here, because the menu uses actions in the QDockWidgets. */
    setupMenu();

    readSettings();

    /* Connect this after readSettings(), otherwise readSettings() triggers writeSettings(). */
    connect(sourceTab,      SIGNAL(currentChanged(int)),
                            SLOT(writeSettings()));
    connect(testSuiteView,  SIGNAL(clicked(const QModelIndex &)),
                            SLOT(writeSettings()));
    connect(sourceInput,    SIGNAL(textChanged()),
                            SLOT(writeSettings()));
}

MainWindow::~MainWindow()
{
    delete m_userTC;
}

QModelIndex MainWindow::sourceIndex(const QModelIndex &proxyIndex) const
{
    return static_cast<TreeSortFilter *>(testSuiteView->model())->mapToSource(proxyIndex);
}

TreeModel *MainWindow::sourceModel() const
{
    const TreeSortFilter *const proxy = static_cast<TreeSortFilter *>(testSuiteView->model());
    return static_cast<TreeModel *>(proxy->sourceModel());
}

void MainWindow::on_testSuiteView_clicked(const QModelIndex &index)
{
    if(index.isValid())
    {
        TestItem *const node = static_cast<TestItem *>(sourceIndex(index).internalPointer());
        Q_ASSERT(node);

        if(node->isFinalNode())
        {
            m_currentTC = static_cast<TestCase *>(node);
            testCaseSelected(m_currentTC);
            return;
        }
    }

    /* In all other cases: */
    m_currentTC = 0;
    testCaseSelected(0);
}

void MainWindow::on_sourceInput_textChanged()
{
    m_userTC->setSourceCode(sourceInput->toPlainText());
}

void MainWindow::on_actionOpen_triggered()
{
    const QString fileName(QFileDialog::getOpenFileName(this,
                                                        QLatin1String("Open Test Suite Catalog"),
                                                        m_previousOpenedCatalog.toLocalFile(),
                                                        QLatin1String("Test Suite Catalog file (*.xml)")));

    /* "If the user presses Cancel, it returns a null string." */
    if(fileName.isNull())
        return;

    m_currentSuiteType = TestSuite::XQuerySuite;
    openCatalog(QUrl::fromLocalFile(fileName), true, TestSuite::XQuerySuite);
}

void MainWindow::on_actionOpenXSLTSCatalog_triggered()
{
    const QString fileName(QFileDialog::getOpenFileName(this,
                                                        QLatin1String("Open Test Suite Catalog"),
                                                        m_previousOpenedCatalog.toLocalFile(),
                                                        QLatin1String("Test Suite Catalog file (*.xml)")));

    /* "If the user presses Cancel, it returns a null string." */
    if(fileName.isNull())
        return;

    m_currentSuiteType = TestSuite::XsltSuite;
    openCatalog(QUrl::fromLocalFile(fileName), true, TestSuite::XsltSuite);
}

void MainWindow::on_actionOpenXSDTSCatalog_triggered()
{
    const QString fileName(QFileDialog::getOpenFileName(this,
                                                        QLatin1String("Open Test Suite Catalog"),
                                                        m_previousOpenedCatalog.toLocalFile(),
                                                        QLatin1String("Test Suite Catalog file (*.xml)")));

    /* "If the user presses Cancel, it returns a null string." */
    if(fileName.isNull())
        return;

    m_currentSuiteType = TestSuite::XsdSuite;
    openCatalog(QUrl::fromLocalFile(fileName), true, TestSuite::XsdSuite);
}

void MainWindow::openCatalog(const QUrl &fileName,
                             const bool reportError,
                             const TestSuite::SuiteType suiteType)
{
    setCurrentFile(fileName);
    m_previousOpenedCatalog = fileName;

    QString errorMsg;
    TestSuite *const loadedSuite = TestSuite::openCatalog(fileName, errorMsg, false, suiteType);

    if(!loadedSuite)
    {
        if(reportError)
        {
            QMessageBox::information(this, QLatin1String("Failed to load catalog file"),
                                     errorMsg, QMessageBox::Ok);
        }

        return;
    }

    TreeModel *const prevModel = sourceModel();
    prevModel->setRoot(loadedSuite);
    m_currentTC = 0;

    testCaseCount->setText(QString::number(loadedSuite->resultSummary().second));
    /* Switch to the tab containing the loaded test suite. */
    sourceTab->setCurrentIndex(0);

    setWindowTitle(QCoreApplication::applicationName() +
                   QLatin1String(" -- ") +
                   QFileInfo(fileName.toLocalFile()).fileName());

    /* @p reportError is set when not auto-loading on startup, and
     * we only want to save when the user opens from the GUI. */
    if(reportError)
        writeSettings();
}

void MainWindow::on_sourceTab_currentChanged(int index)
{
    if(index == 1)
    {
        m_currentTC = m_userTC;
        testCaseSelected(m_userTC);
    }
    else
        on_testSuiteView_clicked(testSuiteView->currentIndex());
}

void MainWindow::on_actionExecute_triggered()
{
    Q_ASSERT(testCaseView);
    TestSuite *const ts = static_cast<TestSuite *>(sourceModel()->root());

    const TestItem::ExecutionStage stage = compileOnly->isChecked() ? TestItem::CompileOnly
                                                                    : TestItem::CompileAndRun;

    m_userTC->setLanguage(isXSLT20->isChecked() ? QXmlQuery::XSLT20 : QXmlQuery::XQuery10);

    if(m_currentTC)
    {
        const TestResult::List rlist(m_currentTC->execute(stage, ts));
        Q_ASSERT(rlist.count() == 1);
        const TestResult *const result = rlist.first();
        Q_ASSERT(result);
        testResultView->displayTestResult(result);
    }
    else
    {
        const QModelIndexList indexes = testSuiteView->selectionModel()->selectedIndexes();
        for (int i = 0; i < indexes.count(); ++i) {
            const QModelIndex source(sourceIndex(indexes.at(i)));

            TestItem *const ti = static_cast<TestItem *>(source.internalPointer());
            if(!ti)
                return;

            /* ti is a TestGroup. It now executes its children, changed(TreeItem *) signals is
             * emitted which the view receives, and thus updates. */
            ti->execute(stage, ts);
        }
    }
}

void MainWindow::readSettings()
{
    QSettings settings;

    settings.beginGroup(QLatin1String("MainWindow"));
    restoreState(settings.value(QLatin1String("state")).toByteArray(), Global::versionNumber);
    resize(settings.value(QLatin1String("size"), QSize(400, 400)).toSize());
    move(settings.value(QLatin1String("pos"), QPoint(200, 200)).toPoint());
    m_previousOpenedCatalog = settings.value(QLatin1String("PreviousOpenedCatalogFile")).toUrl();
    focusURI->setText(settings.value(QLatin1String("focusURI")).toString());
    isXSLT20->setChecked(settings.value(QLatin1String("isXSLT20")).toBool());
    compileOnly->setChecked(settings.value(QLatin1String("compileOnly")).toBool());
    m_currentSuiteType = (TestSuite::SuiteType)settings.value(QLatin1String("PreviousSuiteType"), isXSLT20->isChecked() ? TestSuite::XsltSuite : TestSuite::XQuerySuite).toInt();

    /* Open the previously opened catalog. */
    if(!m_previousOpenedCatalog.isEmpty())
    {
        openCatalog(m_previousOpenedCatalog, false, m_currentSuiteType);
    }

    sourceInput->setPlainText(settings.value(QLatin1String("sourceInput")).toString());
    testResultView->resultViewSelection->setCurrentIndex(
            settings.value(QLatin1String("ResultViewMethod"), 0).toInt());
    testResultView->outputStack->setCurrentIndex(settings.value(
            QLatin1String("ResultViewMethod"), 0).toInt());

    /* Restore the selected test case/group. */
    const QStringList rows(settings.value(QLatin1String("SelectedTestSuiteRow"),
                                          QString())
                           .toString().split(QLatin1Char(',')));

    if(!rows.isEmpty()) /* Ok, we have a selection. */
    {
        QAbstractItemModel *const model = testSuiteView->model();
        Q_ASSERT(model);
        QModelIndex p;

        for(int i = rows.count() - 1; i >= 0;  --i)
        {
            const QModelIndex childIndex(model->index(rows.at(i).toInt(), 0 , p));

            if(childIndex.isValid())
            {
                testSuiteView->scrollTo(p); /* Work around for Qt issue #87575. */
                p = childIndex;
            }
        }

        testSuiteView->scrollTo(p); /* Scrolls to it. */
        testSuiteView->setCurrentIndex(p); /* Selects it. */
        on_testSuiteView_clicked(p); /* Loads the test case in the Test Case View. */
    }

    /* Do it here. In this way the user-entered test case gets selected, if that tab
     * was previously used. */
    sourceTab->setCurrentIndex(settings.value(QLatin1String("SelectedTab"), 0).toInt());
    on_sourceTab_currentChanged(sourceTab->currentIndex());

    settings.endGroup();
}

void MainWindow::writeSettings()
{
    QSettings settings;

    settings.beginGroup(QLatin1String("MainWindow"));
    settings.setValue(QLatin1String("state"), saveState(Global::versionNumber));
    settings.setValue(QLatin1String("pos"), pos());
    settings.setValue(QLatin1String("size"), size());
    settings.setValue(QLatin1String("sourceInput"), sourceInput->toPlainText());
    settings.setValue(QLatin1String("PreviousOpenedCatalogFile"), m_previousOpenedCatalog);
    settings.setValue(QLatin1String("PreviousSuiteType"), m_currentSuiteType);
    settings.setValue(QLatin1String("SelectedTab"), sourceTab->currentIndex());
    settings.setValue(QLatin1String("ResultViewMethod"),
                      testResultView->resultViewSelection->currentIndex());
    settings.setValue(QLatin1String("focusURI"),
                      focusURI->text());
    settings.setValue(QLatin1String("isXSLT20"),
                      isXSLT20->isChecked());
    settings.setValue(QLatin1String("compileOnly"),
                      compileOnly->isChecked());

    /* Store the selected test case/group. */
    QModelIndex selected(sourceIndex(testSuiteView->currentIndex()));
    if(selected.isValid())
    {
        QString result;

        do
        {
            result.append(QString::number(selected.row()));
            selected = selected.parent();

            if(selected.isValid())
                result.append(QLatin1Char(','));
            else
                break;
        }
        while(true);

        settings.setValue(QLatin1String("SelectedTestSuiteRow"), result);
    }

    settings.endGroup();
}

void MainWindow::setCurrentFile(const QUrl &f)
{
    const QString fileName(f.toLocalFile());
    QSettings settings;
    settings.beginGroup(QLatin1String("MainWindow"));
    QStringList files(settings.value(QLatin1String("RecentFileList")).toStringList());

    files.removeAll(fileName);
    files.prepend(fileName);
    while(files.size() > MaximumRecentFiles)
        files.removeLast();

    settings.setValue(QLatin1String("RecentFileList"), files);
    settings.endGroup();

    updateRecentFileActions();
}

void MainWindow::updateRecentFileActions()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("MainWindow"));
    const QStringList files(settings.value(QLatin1String("RecentFileList")).toStringList());
    settings.endGroup();

    const int numRecentFiles = qMin(files.size(), static_cast<int>(MaximumRecentFiles));

    for(int i = 0; i < numRecentFiles; ++i)
    {
        const QString text(QString::fromLatin1("&%1 %2").arg(i + 1).arg(QFileInfo(files[i]).filePath()));
        m_recentFileActs[i]->setText(text);
        m_recentFileActs[i]->setData(QUrl::fromLocalFile(files[i]));
        m_recentFileActs[i]->setVisible(true);
    }

    for(int j = numRecentFiles; j < MaximumRecentFiles; ++j)
        m_recentFileActs[j]->setVisible(false);
}

void MainWindow::openRecentFile()
{
    const QAction *const action = qobject_cast<QAction *>(sender());
    if(action)
        openCatalog(action->data().toUrl(), true, TestSuite::XQuerySuite);
}

void MainWindow::closeEvent(QCloseEvent *ev)
{
    writeSettings();
    ev->accept();
}

void MainWindow::setupActions()
{
    connect(actionQuit, SIGNAL(triggered()), qApp, SLOT(closeAllWindows()));

    for(int i = 0; i < MaximumRecentFiles; ++i)
    {
        m_recentFileActs[i] = new QAction(this);
        m_recentFileActs[i]->setVisible(false);
        connect(m_recentFileActs[i], SIGNAL(triggered()),
                this, SLOT(openRecentFile()));
    }
}

void MainWindow::setupMenu()
{
    QMenu *const menFile = findChild<QMenu *>(QLatin1String("menuFile"));
    Q_ASSERT(menFile);
    QAction *const actOpen = findChild<QAction *>(QLatin1String("actionExecute"));
    Q_ASSERT(actOpen);
    QMenu *const recent = new QMenu(QLatin1String("O&pen Recent"), this);

    menFile->insertMenu(actOpen, recent);
    menFile->insertSeparator(actOpen);

    for(int i = 0; i < MaximumRecentFiles; ++i)
        recent->addAction(m_recentFileActs[i]);

    updateRecentFileActions();

    QMenu *const menWindows = findChild<QMenu *>(QLatin1String("menuWindows"));
    Q_ASSERT(menWindows);

    menWindows->addAction(testCaseView->toggleViewAction());
    menWindows->addAction(testResultView->toggleViewAction());
    menWindows->addAction(functionView->toggleViewAction());
}

void MainWindow::on_actionRestart_triggered()
{
    if(QProcess::startDetached(QCoreApplication::applicationFilePath()))
        QApplication::closeAllWindows();
    else
    {
        QTextStream err(stderr);
        err << "Failed to start " << qPrintable(QCoreApplication::applicationFilePath()) << endl;
    }
}


// vim: et:ts=4:sw=4:sts=4
