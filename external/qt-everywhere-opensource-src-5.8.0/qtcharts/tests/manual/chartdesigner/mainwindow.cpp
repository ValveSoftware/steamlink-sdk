/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Charts module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "mainwindow.h"
#include "objectinspectorwidget.h"
#include "penwidget.h"
#include "brushwidget.h"
#include "engine.h"
#include <QtCore/QSettings>
#include <QtCharts/QChartView>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QSplitter>
#include <QtCore/QMetaEnum>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QHeaderView>
#include <QtCore/QDebug>
#include <QtWidgets/QMessageBox>

static const QString FILENAME_SETTING("filename");
static const QString GEOMETRY_SETTING("geometry");

MainWindow::MainWindow() :
    m_engine(new Engine(this)),
    m_view(new QChartView(m_engine->chart())),
    m_scene(m_view->scene()),
    m_table(new QTableView()),
    m_addSeriesMenu(0),
    m_seriesMenu(0),
    m_chartMenu(0),
    m_themeMenu(0),
    m_animationMenu(0),
    m_zoomMenu(0),
    m_removeAllAction(0),
    m_legendAction(0),
    m_antialiasingAction(0)
{
    createMenus();
    createDockWidgets();
    createTable();
    createLayout();

    QSettings settings;
    restoreGeometry(settings.value(GEOMETRY_SETTING).toByteArray());
    m_filename = settings.value(FILENAME_SETTING).toString();
    if (m_filename.isEmpty())
        m_filename = "untitled";

    setWindowTitle(m_filename);
    updateUI();
}

MainWindow::~MainWindow()
{
    delete m_engine;
}

void MainWindow::createMenus()
{
    QMenu *file = menuBar()->addMenu(tr("File"));
    QMenu *edit = menuBar()->addMenu(tr("Edit"));
    m_seriesMenu = menuBar()->addMenu(tr("Series"));
    m_chartMenu = menuBar()->addMenu(tr("Chart"));

    m_addSeriesMenu = new QMenu(tr("Add series"));
    m_themeMenu = new QMenu(tr("Apply theme"));
    m_animationMenu = new QMenu(tr("Animations"));
    m_zoomMenu = new QMenu(tr("Zoom"));

    file->addAction(tr("New"), this, SLOT(handleNewAction()));
    file->addAction(tr("Load"), this, SLOT(handleLoadAction()));
    file->addAction(tr("Save"), this, SLOT(handleSaveAction()));
    file->addAction(tr("Save As"), this, SLOT(handleSaveAsAction()));

    //seriesMenu
    m_seriesMenu->addMenu(m_addSeriesMenu);
    m_removeAllAction = new QAction(tr("Remove all series"), this);
    QObject::connect(m_removeAllAction, SIGNAL(triggered()), this, SLOT(handleRemoveAllSeriesAction()));
    m_seriesMenu->addAction(m_removeAllAction);
    m_seriesMenu->addSeparator();

    //seriesMenu /addSeriesMenu
    {
        int index = QAbstractSeries::staticMetaObject.indexOfEnumerator("SeriesType");
        QMetaEnum metaEnum = QAbstractSeries::staticMetaObject.enumerator(index);

        int count = metaEnum.keyCount();

        for (int i = 0; i < count; ++i) {
            QAction* action = new QAction(metaEnum.key(i), this);
            action->setData(metaEnum.value(i));
            m_addSeriesMenu->addAction(action);
            QObject::connect(action, SIGNAL(triggered()), this, SLOT(handleAddSeriesMenu()));
        }
    }

    //chartMenu / themeMenu
    {
        m_chartMenu->addMenu(m_themeMenu);
        int index = QChart::staticMetaObject.indexOfEnumerator("ChartTheme");
        QMetaEnum metaEnum = QChart::staticMetaObject.enumerator(index);

        int count = metaEnum.keyCount();

        for (int i = 0; i < count; ++i) {
            QAction* action = new QAction(metaEnum.key(i), this);
            action->setData(metaEnum.value(i));
            action->setCheckable(true);
            m_themeMenu->addAction(action);
            QObject::connect(action, SIGNAL(triggered()), this, SLOT(handleThemeMenu()));
        }
    }

    //chartMenu / animationMenu
    {
        m_chartMenu->addMenu(m_animationMenu);
        int index = QChart::staticMetaObject.indexOfEnumerator("AnimationOption");
        QMetaEnum metaEnum = QChart::staticMetaObject.enumerator(index);

        int count = metaEnum.keyCount();

        for (int i = 0; i < count; ++i) {
            QAction* action = new QAction(metaEnum.key(i), this);
            action->setData(metaEnum.value(i));
            action->setCheckable(true);
            m_animationMenu->addAction(action);
            QObject::connect(action, SIGNAL(triggered()), this, SLOT(handleAnimationMenu()));
        }
    }

    //chartMenu / zoomMenu
    {
        m_chartMenu->addMenu(m_zoomMenu);
        int index = QChartView::staticMetaObject.indexOfEnumerator("RubberBand");
        QMetaEnum metaEnum = QChartView::staticMetaObject.enumerator(index);

        int count = metaEnum.keyCount();

        for (int i = 0; i < count; ++i) {
            QAction* action = new QAction(metaEnum.key(i), this);
            action->setData(metaEnum.value(i));
            action->setCheckable(true);
            m_zoomMenu->addAction(action);
            QObject::connect(action, SIGNAL(triggered()), this, SLOT(handleZoomMenu()));
        }
    }

    //chartMenu / legend
    m_legendAction = new QAction(tr("Legend"), this);
    m_legendAction->setCheckable(true);
    m_chartMenu->addAction(m_legendAction);
    QObject::connect(m_legendAction, SIGNAL(triggered()), this, SLOT(handleLegendAction()));

    //chartMenu / Anti-aliasing
    m_antialiasingAction = new QAction(tr("Anti-aliasing"), this);
    m_antialiasingAction->setCheckable(true);
    m_chartMenu->addAction(m_antialiasingAction);
    QObject::connect(m_antialiasingAction, SIGNAL(triggered()), this, SLOT(handleAntialiasingAction()));

}

void MainWindow::createDockWidgets()
{
    m_brushWidget = new BrushWidget();
    QDockWidget *brushDockWidget = new QDockWidget(tr("Brush"), this);
    brushDockWidget->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    brushDockWidget->setWidget(m_brushWidget);
    addDockWidget(Qt::RightDockWidgetArea, brushDockWidget);

    m_penWidget = new PenWidget();
    QDockWidget *penDockWidget = new QDockWidget(tr("Pen"), this);
    penDockWidget->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    penDockWidget->setWidget(m_penWidget);
    addDockWidget(Qt::RightDockWidgetArea, penDockWidget);

    m_inspectorWidget = new InspectorWidget();
    QDockWidget *inspectorDockWidget = new QDockWidget(tr("Object Inspector"), this);
    inspectorDockWidget->setFeatures(
        QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    inspectorDockWidget->setWidget(m_inspectorWidget);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDockWidget);

    setDockOptions(QMainWindow::AnimatedDocks);
}

void MainWindow::createLayout()
{
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    int rowHeight = m_table->rowHeight(0);
    int tableHeight = (m_engine->modelCount() * rowHeight) + m_table->horizontalHeader()->height() + 2 * m_table->frameWidth();

    m_table->setMinimumHeight(tableHeight);
    m_table->setMaximumHeight(tableHeight);

    QSplitter *splitter = new QSplitter(this);
    splitter->setOrientation(Qt::Vertical);
    splitter->addWidget(m_table);
    splitter->addWidget(m_view);
    setCentralWidget(splitter);
    m_view->hide();
}

void MainWindow::createTable()
{
    m_table->setModel(m_engine->model());
    m_table->setSelectionModel(m_engine->selectionModel());
    QObject::connect(m_table->selectionModel(),SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this,SLOT(updateUI()));
}

void MainWindow::updateUI()
{

    QItemSelectionModel* selection = m_table->selectionModel();
    const QModelIndexList& list = selection->selectedIndexes();

    QMap<int, QModelIndex> columns;

    foreach (const QModelIndex& index, list) {
        columns.insertMulti(index.column(), index);
    }

    QList<int> keys = columns.uniqueKeys();

    bool seriesEnabled = false;

    foreach (QAction* action, m_addSeriesMenu->actions()) {
        switch (action->data().toInt()) {
        case QAbstractSeries::SeriesTypeLine:
        case QAbstractSeries::SeriesTypeSpline:
        case QAbstractSeries::SeriesTypeScatter:
            action->setEnabled(list.count() > 0 && keys.count() >= 2);
            seriesEnabled |= action->isEnabled();
            break;
        case QAbstractSeries::SeriesTypeBar:
        case QAbstractSeries::SeriesTypePercentBar:
        case QAbstractSeries::SeriesTypeStackedBar:
            action->setEnabled(list.count() > 0 && keys.count() >= 2);
            seriesEnabled |= action->isEnabled();
            break;
        case QAbstractSeries::SeriesTypePie:
            action->setEnabled(list.count() > 0 && keys.count() == 2);
            seriesEnabled |= action->isEnabled();
            break;
        case QAbstractSeries::SeriesTypeArea:
            action->setEnabled(list.count() > 0 && keys.count() == 3);
            seriesEnabled |= action->isEnabled();
            break;
        }
    }

    m_chartMenu->setEnabled(m_engine->chart()->series().count() > 0);
    m_seriesMenu->setEnabled(seriesEnabled || m_engine->chart()->series().count() > 0);
    m_removeAllAction->setEnabled(m_engine->chart()->series().count() > 0);

    int theme = m_engine->chart()->theme();
    foreach (QAction* action, m_themeMenu->actions()) {
        action->setChecked(action->data().toInt() == theme);
    }

    int animation = m_engine->chart()->animationOptions();
    foreach (QAction* action, m_animationMenu->actions()) {
        action->setChecked(action->data().toInt() == animation);
    }

    int zoom = m_view->rubberBand();
    foreach (QAction* action, m_zoomMenu->actions()) {
        action->setChecked(action->data().toInt() == zoom);
    }

    m_legendAction->setChecked(m_engine->chart()->legend()->isVisible());
    m_antialiasingAction->setChecked(m_view->renderHints().testFlag(QPainter::Antialiasing));

    foreach (QAction *action, m_seriesMenu->actions()) {
        //TODO: visibility handling
        //if (m_series.value(action->text()))
        //    ;
        //action->setChecked(false);
    }
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings;
    settings.setValue(GEOMETRY_SETTING, saveGeometry());
    settings.setValue(FILENAME_SETTING, m_filename);
    QMainWindow::closeEvent(event);
}

//handlers

void MainWindow::handleNewAction()
{
    m_engine->chart()->removeAllSeries();
    m_view->hide();
    m_engine->clearModels();
    createTable();
    m_filename = "untitled";
    setWindowTitle(m_filename);
    updateUI();
}

void MainWindow::handleAddSeriesMenu()
{
    m_view->show();
    QAction* action = qobject_cast<QAction*>(sender());
    QList<QAbstractSeries*> series = m_engine->addSeries(QAbstractSeries::SeriesType(action->data().toInt()));

    foreach (QAbstractSeries* s , series)
    {
        QAction *newAction = new QAction(s->name(),this);
        //newAction->setCheckable(true);
        m_series.insert(s->name(),s);
        m_seriesMenu->addAction(newAction);
    }

    updateUI();
}

void MainWindow::handleRemoveAllSeriesAction()
{

    foreach (QAction* action, m_seriesMenu->actions()){
        if(m_series.contains(action->text())){
            m_seriesMenu->removeAction(action);
            m_engine->removeSeries(m_series.value(action->text()));
            delete action;
        }
    }

    m_series.clear();

    m_view->hide();
    updateUI();
}

void MainWindow::handleThemeMenu()
{
    QAction* action = qobject_cast<QAction*>(sender());
    m_engine->chart()->setTheme(QChart::ChartTheme(action->data().toInt()));
    updateUI();
}

void MainWindow::handleAnimationMenu()
{
    QAction* action = qobject_cast<QAction*>(sender());
    m_engine->chart()->setAnimationOptions(QChart::AnimationOption(action->data().toInt()));
    updateUI();
}

void MainWindow::handleZoomMenu()
{
    QAction* action = qobject_cast<QAction*>(sender());
    m_view->setRubberBand(QChartView::RubberBand(action->data().toInt()));
    updateUI();
}

void MainWindow::handleAntialiasingAction()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (action->isChecked()) {
        m_view->setRenderHint(QPainter::Antialiasing, true);
    }
    else {
        m_view->setRenderHint(QPainter::Antialiasing, false);
    }
}

void MainWindow::handleLegendAction()
{
    QAction* action = qobject_cast<QAction*>(sender());
    if (action->isChecked()) {
        m_engine->chart()->legend()->setVisible(true);
    }
    else {
        m_engine->chart()->legend()->setVisible(false);
    }
}

void MainWindow::handleSaveAction()
{
    if(!m_engine->save(m_filename)) {

    QScopedPointer<QMessageBox> messageBox(new QMessageBox(this));
    messageBox->setIcon(QMessageBox::Warning);
    messageBox->setWindowModality(Qt::WindowModal);
    messageBox->setWindowTitle(QString(tr("Error")));
    messageBox->setText(tr("Could not write to ") + m_filename);
    messageBox->exec();
    }
}

void MainWindow::handleLoadAction()
{
    if(!m_engine->load(m_filename)) {

    QScopedPointer<QMessageBox> messageBox(new QMessageBox(this));
    messageBox->setIcon(QMessageBox::Warning);
    messageBox->setWindowModality(Qt::WindowModal);
    messageBox->setWindowTitle(QString(tr("Error")));
    messageBox->setText(tr("Could not open ") + m_filename);
    messageBox->exec();

    }else createTable();
}
