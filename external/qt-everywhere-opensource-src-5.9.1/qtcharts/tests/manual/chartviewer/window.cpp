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

#include "window.h"
#include "view.h"
#include "grid.h"
#include "charts.h"
#include <QtCharts/QChartView>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QLegend>
#include <QtCharts/QValueAxis>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsLinearLayout>
#include <QtWidgets/QGraphicsProxyWidget>
#include <QtWidgets/QOpenGLWidget>
#include <QtWidgets/QApplication>
#include <QtCore/QDebug>
#include <QtWidgets/QMenu>
#include <QtWidgets/QPushButton>

Window::Window(const QVariantHash &parameters, QWidget *parent)
    : QMainWindow(parent),
      m_scene(new QGraphicsScene(this)),
      m_view(0),
      m_form(0),
      m_themeComboBox(0),
      m_antialiasCheckBox(0),
      m_animatedComboBox(0),
      m_legendComboBox(0),
      m_templateComboBox(0),
      m_viewComboBox(0),
      m_xTickSpinBox(0),
      m_yTickSpinBox(0),
      m_minorXTickSpinBox(0),
      m_minorYTickSpinBox(0),
      m_openGLCheckBox(0),
      m_zoomCheckBox(0),
      m_scrollCheckBox(0),
      m_baseLayout(new QGraphicsLinearLayout()),
      m_menu(createMenu()),
      m_template(0),
      m_grid(new Grid(-1))
{
    createProxyWidgets();
    // create layout
    QGraphicsLinearLayout *settingsLayout = new QGraphicsLinearLayout();

    settingsLayout->setOrientation(Qt::Vertical);
    settingsLayout->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    settingsLayout->addItem(m_widgetHash["openGLCheckBox"]);
    settingsLayout->addItem(m_widgetHash["antialiasCheckBox"]);
    settingsLayout->addItem(m_widgetHash["viewLabel"]);
    settingsLayout->addItem(m_widgetHash["viewComboBox"]);
    settingsLayout->addItem(m_widgetHash["themeLabel"]);
    settingsLayout->addItem(m_widgetHash["themeComboBox"]);
    settingsLayout->addItem(m_widgetHash["animationsLabel"]);
    settingsLayout->addItem(m_widgetHash["animatedComboBox"]);
    settingsLayout->addItem(m_widgetHash["legendLabel"]);
    settingsLayout->addItem(m_widgetHash["legendComboBox"]);
    settingsLayout->addItem(m_widgetHash["templateLabel"]);
    settingsLayout->addItem(m_widgetHash["templateComboBox"]);
    settingsLayout->addItem(m_widgetHash["scrollCheckBox"]);
    settingsLayout->addItem(m_widgetHash["zoomCheckBox"]);
    settingsLayout->addItem(m_widgetHash["xTickLabel"]);
    settingsLayout->addItem(m_widgetHash["xTickSpinBox"]);
    settingsLayout->addItem(m_widgetHash["yTickLabel"]);
    settingsLayout->addItem(m_widgetHash["yTickSpinBox"]);
    settingsLayout->addItem(m_widgetHash["minorXTickLabel"]);
    settingsLayout->addItem(m_widgetHash["minorXTickSpinBox"]);
    settingsLayout->addItem(m_widgetHash["minorYTickLabel"]);
    settingsLayout->addItem(m_widgetHash["minorYTickSpinBox"]);
    settingsLayout->addStretch();

    m_baseLayout->setOrientation(Qt::Horizontal);
    m_baseLayout->addItem(m_grid);
    m_baseLayout->addItem(settingsLayout);

    m_form = new QGraphicsWidget();
    m_form->setLayout(m_baseLayout);
    m_scene->addItem(m_form);

    m_view = new View(m_scene, m_form);
    m_view->setMinimumSize(m_form->minimumSize().toSize());

    // Set defaults
    m_antialiasCheckBox->setChecked(true);
    initializeFromParamaters(parameters);
    updateUI();
    if(!m_category.isEmpty() && !m_subcategory.isEmpty() && !m_name.isEmpty())
        m_grid->createCharts(m_category,m_subcategory,m_name);


    handleGeometryChanged();
    setCentralWidget(m_view);

    connectSignals();
}

Window::~Window()
{
}

void Window::connectSignals()
{
    QObject::connect(m_form, SIGNAL(geometryChanged()), this , SLOT(handleGeometryChanged()));
    QObject::connect(m_viewComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateUI()));
    QObject::connect(m_themeComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateUI()));
    QObject::connect(m_xTickSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateUI()));
    QObject::connect(m_yTickSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateUI()));
    QObject::connect(m_minorXTickSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateUI()));
    QObject::connect(m_minorYTickSpinBox, SIGNAL(valueChanged(int)), this, SLOT(updateUI()));
    QObject::connect(m_antialiasCheckBox, SIGNAL(toggled(bool)), this, SLOT(updateUI()));
    QObject::connect(m_openGLCheckBox, SIGNAL(toggled(bool)), this, SLOT(updateUI()));
    QObject::connect(m_zoomCheckBox, SIGNAL(toggled(bool)), this, SLOT(updateUI()));
    QObject::connect(m_scrollCheckBox, SIGNAL(toggled(bool)), this, SLOT(updateUI()));
    QObject::connect(m_animatedComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateUI()));
    QObject::connect(m_legendComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateUI()));
    QObject::connect(m_templateComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(updateUI()));
    QObject::connect(m_grid, SIGNAL(chartSelected(QChart*)), this, SLOT(handleChartSelected(QChart*)));
}

void Window::createProxyWidgets()
{
    m_themeComboBox = createThemeBox();
    m_viewComboBox = createViewBox();
    m_xTickSpinBox = new QSpinBox();
    m_xTickSpinBox->setMinimum(2);
    m_xTickSpinBox->setValue(5);
    m_yTickSpinBox = new QSpinBox();
    m_yTickSpinBox->setMinimum(2);
    m_yTickSpinBox->setValue(5);
    m_minorXTickSpinBox = new QSpinBox();
    m_minorYTickSpinBox = new QSpinBox();
    m_antialiasCheckBox = new QCheckBox(tr("Anti-aliasing"));
    m_animatedComboBox = createAnimationBox();
    m_legendComboBox = createLegendBox();
    m_openGLCheckBox = new QCheckBox(tr("OpenGL"));
    m_zoomCheckBox = new QCheckBox(tr("Zoom"));
    m_scrollCheckBox = new QCheckBox(tr("Scroll"));
    m_templateComboBox = createTempleteBox();
    m_widgetHash["viewLabel"] = m_scene->addWidget(new QLabel("View"));
    m_widgetHash["viewComboBox"] = m_scene->addWidget(m_viewComboBox);
    m_widgetHash["themeComboBox"] = m_scene->addWidget(m_themeComboBox);
    m_widgetHash["antialiasCheckBox"] = m_scene->addWidget(m_antialiasCheckBox);
    m_widgetHash["animatedComboBox"] = m_scene->addWidget(m_animatedComboBox);
    m_widgetHash["legendComboBox"] = m_scene->addWidget(m_legendComboBox);
    m_widgetHash["xTickLabel"] = m_scene->addWidget(new QLabel("X Tick"));
    m_widgetHash["xTickSpinBox"] = m_scene->addWidget(m_xTickSpinBox);
    m_widgetHash["yTickLabel"] = m_scene->addWidget(new QLabel("Y Tick"));
    m_widgetHash["yTickSpinBox"] = m_scene->addWidget(m_yTickSpinBox);
    m_widgetHash["minorXTickLabel"] = m_scene->addWidget(new QLabel("Minor X Tick"));
    m_widgetHash["minorXTickSpinBox"] = m_scene->addWidget(m_minorXTickSpinBox);
    m_widgetHash["minorYTickLabel"] = m_scene->addWidget(new QLabel("Minor Y Tick"));
    m_widgetHash["minorYTickSpinBox"] = m_scene->addWidget(m_minorYTickSpinBox);
    m_widgetHash["openGLCheckBox"] = m_scene->addWidget(m_openGLCheckBox);
    m_widgetHash["themeLabel"] = m_scene->addWidget(new QLabel("Theme"));
    m_widgetHash["animationsLabel"] = m_scene->addWidget(new QLabel("Animations"));
    m_widgetHash["legendLabel"] = m_scene->addWidget(new QLabel("Legend"));
    m_widgetHash["templateLabel"] = m_scene->addWidget(new QLabel("Chart template"));
    m_widgetHash["templateComboBox"] = m_scene->addWidget(m_templateComboBox);
    m_widgetHash["zoomCheckBox"] = m_scene->addWidget(m_zoomCheckBox);
    m_widgetHash["scrollCheckBox"] = m_scene->addWidget(m_scrollCheckBox);
}

QComboBox *Window::createThemeBox()
{
    QComboBox *themeComboBox = new ComboBox(this);
    themeComboBox->addItem("Light", QChart::ChartThemeLight);
    themeComboBox->addItem("Blue Cerulean", QChart::ChartThemeBlueCerulean);
    themeComboBox->addItem("Dark", QChart::ChartThemeDark);
    themeComboBox->addItem("Brown Sand", QChart::ChartThemeBrownSand);
    themeComboBox->addItem("Blue NCS", QChart::ChartThemeBlueNcs);
    themeComboBox->addItem("High Contrast", QChart::ChartThemeHighContrast);
    themeComboBox->addItem("Blue Icy", QChart::ChartThemeBlueIcy);
    themeComboBox->addItem("Qt", QChart::ChartThemeQt);
    return themeComboBox;
}

QComboBox *Window::createViewBox()
{
    QComboBox *viewComboBox = new ComboBox(this);
    viewComboBox->addItem("1 chart", 1);
    viewComboBox->addItem("4 charts", 2);
    viewComboBox->addItem("9 charts", 3);
    viewComboBox->addItem("16 charts", 4);
    return viewComboBox;
}

QComboBox *Window::createAnimationBox()
{
    QComboBox *animationComboBox = new ComboBox(this);
    animationComboBox->addItem("No Animations", QChart::NoAnimation);
    animationComboBox->addItem("GridAxis Animations", QChart::GridAxisAnimations);
    animationComboBox->addItem("Series Animations", QChart::SeriesAnimations);
    animationComboBox->addItem("All Animations", QChart::AllAnimations);
    return animationComboBox;
}

QComboBox *Window::createLegendBox()
{
    QComboBox *legendComboBox = new ComboBox(this);
    legendComboBox->addItem("No Legend ", 0);
    legendComboBox->addItem("Legend Top", Qt::AlignTop);
    legendComboBox->addItem("Legend Bottom", Qt::AlignBottom);
    legendComboBox->addItem("Legend Left", Qt::AlignLeft);
    legendComboBox->addItem("Legend Right", Qt::AlignRight);
    return legendComboBox;
}

QComboBox *Window::createTempleteBox()
{
    QComboBox *templateComboBox = new ComboBox(this);
    templateComboBox->addItem("No Template", 0);

    Charts::ChartList list = Charts::chartList();
    QMultiMap<QString, Chart *> categoryMap;

    foreach (Chart *chart, list)
        categoryMap.insertMulti(chart->category(), chart);

    foreach (const QString &category, categoryMap.uniqueKeys())
        templateComboBox->addItem(category, category);

    return templateComboBox;
}

void Window::initializeFromParamaters(const QVariantHash &parameters)
{
    if (parameters.contains("view")) {
           int t = parameters["view"].toInt();
           for (int i = 0; i < m_viewComboBox->count(); ++i) {
               if (m_viewComboBox->itemData(i).toInt() == t) {
                   m_viewComboBox->setCurrentIndex(i);
                   break;
               }
           }
       }

    if (parameters.contains("chart")) {
        QString t = parameters["chart"].toString();

        QRegExp rx("([a-zA-Z0-9_]*)::([a-zA-Z0-9_]*)::([a-zA-Z0-9_]*)");
        int pos = rx.indexIn(t);

        if (pos > -1) {
            m_category = rx.cap(1);
            m_subcategory = rx.cap(2);
            m_name = rx.cap(3);
            m_templateComboBox->setCurrentIndex(0);
        }
        else {
            for (int i = 0; i < m_templateComboBox->count(); ++i) {
                if (m_templateComboBox->itemText(i) == t) {
                    m_templateComboBox->setCurrentIndex(i);
                    break;
                }
            }
        }
    }
    if (parameters.contains("opengl")) {
        bool checked = parameters["opengl"].toBool();
        m_openGLCheckBox->setChecked(checked);
    }
    if (parameters.contains("theme")) {
        QString t = parameters["theme"].toString();
        for (int i = 0; i < m_themeComboBox->count(); ++i) {
            if (m_themeComboBox->itemText(i) == t) {
                m_themeComboBox->setCurrentIndex(i);
                break;
            }
        }
    }
    if (parameters.contains("animation")) {
        QString t = parameters["animation"].toString();
        for (int i = 0; i < m_animatedComboBox->count(); ++i) {
            if (m_animatedComboBox->itemText(i) == t) {
                m_animatedComboBox->setCurrentIndex(i);
                break;
            }
        }
    }
    if (parameters.contains("legend")) {
        QString t = parameters["legend"].toString();
        for (int i = 0; i < m_legendComboBox->count(); ++i) {
            if (m_legendComboBox->itemText(i) == t) {
                m_legendComboBox->setCurrentIndex(i);
                break;
            }
        }
    }
}

void Window::updateUI()
{
    checkView();
    checkTemplate();
    checkOpenGL();
    checkTheme();
    checkAnimationOptions();
    checkLegend();
    checkState();
    checkXTick();
    checkYTick();
    checkMinorXTick();
    checkMinorYTick();
}

void Window::checkView()
{
    int count(m_viewComboBox->itemData(m_viewComboBox->currentIndex()).toInt());
    if(m_grid->size()!=count){
        m_grid->setSize(count);
        m_template = 0;
    }
}

void Window::checkXTick()
{
    foreach (QChart *chart, m_grid->charts()) {
        if (qobject_cast<QValueAxis *>(chart->axisX())) {
            QValueAxis *valueAxis = qobject_cast<QValueAxis *>(chart->axisX());
            valueAxis->setGridLineVisible();
            valueAxis->setTickCount(m_xTickSpinBox->value());
        }
    }
}

void Window::checkYTick()
{
    foreach (QChart *chart, m_grid->charts()) {
        if (qobject_cast<QValueAxis *>(chart->axisY())) {
            QValueAxis *valueAxis = qobject_cast<QValueAxis *>(chart->axisY());
            valueAxis->setGridLineVisible();
            valueAxis->setTickCount(m_yTickSpinBox->value());
        }
    }
}

void Window::checkMinorXTick()
{
    foreach (QChart *chart, m_grid->charts()) {
        if (qobject_cast<QValueAxis *>(chart->axisX())) {
            QValueAxis *valueAxis = qobject_cast<QValueAxis *>(chart->axisX());
            valueAxis->setMinorGridLineVisible();
            valueAxis->setMinorTickCount(m_minorXTickSpinBox->value());
        }
    }
}

void Window::checkMinorYTick()
{
    foreach (QChart *chart, m_grid->charts()) {
        if (qobject_cast<QValueAxis *>(chart->axisY())) {
            QValueAxis *valueAxis = qobject_cast<QValueAxis *>(chart->axisY());
            valueAxis->setMinorGridLineVisible();
            valueAxis->setMinorTickCount(m_minorYTickSpinBox->value());
        }
    }
}

void Window::checkLegend()
{
    Qt::Alignment alignment(m_legendComboBox->itemData(m_legendComboBox->currentIndex()).toInt());

    if (!alignment) {
        foreach (QChart *chart, m_grid->charts())
            chart->legend()->hide();
    } else {
        foreach (QChart *chart, m_grid->charts()) {
            chart->legend()->setAlignment(alignment);
            chart->legend()->show();
        }
    }
}

void Window::checkOpenGL()
{
    bool opengl = m_openGLCheckBox->isChecked();
    bool isOpengl = qobject_cast<QOpenGLWidget *>(m_view->viewport());
    if ((isOpengl && !opengl) || (!isOpengl && opengl)) {
        m_view->deleteLater();
        m_view = new View(m_scene, m_form);
        m_view->setViewport(!opengl ? new QWidget() : new QOpenGLWidget());
        setCentralWidget(m_view);
    }

    bool antialias = m_antialiasCheckBox->isChecked();

    if (opengl)
        m_view->setRenderHint(QPainter::HighQualityAntialiasing, antialias);
    else
        m_view->setRenderHint(QPainter::Antialiasing, antialias);
}

void Window::checkAnimationOptions()
{
    QChart::AnimationOptions options(
        m_animatedComboBox->itemData(m_animatedComboBox->currentIndex()).toInt());

    QList<QChart *> charts = m_grid->charts();

    if (!charts.isEmpty() && charts.at(0)->animationOptions() != options) {
        foreach (QChart *chart, charts)
            chart->setAnimationOptions(options);
    }
}

void Window::checkState()
{
    bool scroll = m_scrollCheckBox->isChecked();


    if (m_grid->state() != Grid::ScrollState && scroll) {
        m_grid->setState(Grid::ScrollState);
        m_zoomCheckBox->setChecked(false);
    } else if (!scroll && m_grid->state() == Grid::ScrollState) {
        m_grid->setState(Grid::NoState);
    }

    bool zoom = m_zoomCheckBox->isChecked();

    if (m_grid->state() != Grid::ZoomState && zoom) {
        m_grid->setState(Grid::ZoomState);
        m_scrollCheckBox->setChecked(false);
    } else if (!zoom && m_grid->state() == Grid::ZoomState) {
        m_grid->setState(Grid::NoState);
    }
}

void Window::checkTemplate()
{
    int index = m_templateComboBox->currentIndex();
    if (m_template == index || index == 0)
        return;

    m_template = index;
    QString category = m_templateComboBox->itemData(index).toString();
    m_grid->createCharts(category);
}

void Window::checkTheme()
{
    QChart::ChartTheme theme = (QChart::ChartTheme) m_themeComboBox->itemData(
                                   m_themeComboBox->currentIndex()).toInt();

    foreach (QChart *chart, m_grid->charts())
        chart->setTheme(theme);

    QPalette pal = window()->palette();
    if (theme == QChart::ChartThemeLight) {
        pal.setColor(QPalette::Window, QRgb(0xf0f0f0));
        pal.setColor(QPalette::WindowText, QRgb(0x404044));
    } else if (theme == QChart::ChartThemeDark) {
        pal.setColor(QPalette::Window, QRgb(0x121218));
        pal.setColor(QPalette::WindowText, QRgb(0xd6d6d6));
    } else if (theme == QChart::ChartThemeBlueCerulean) {
        pal.setColor(QPalette::Window, QRgb(0x40434a));
        pal.setColor(QPalette::WindowText, QRgb(0xd6d6d6));
    } else if (theme == QChart::ChartThemeBrownSand) {
        pal.setColor(QPalette::Window, QRgb(0x9e8965));
        pal.setColor(QPalette::WindowText, QRgb(0x404044));
    } else if (theme == QChart::ChartThemeBlueNcs) {
        pal.setColor(QPalette::Window, QRgb(0x018bba));
        pal.setColor(QPalette::WindowText, QRgb(0x404044));
    } else if (theme == QChart::ChartThemeHighContrast) {
        pal.setColor(QPalette::Window, QRgb(0xffab03));
        pal.setColor(QPalette::WindowText, QRgb(0x181818));
    } else if (theme == QChart::ChartThemeBlueIcy) {
        pal.setColor(QPalette::Window, QRgb(0xcee7f0));
        pal.setColor(QPalette::WindowText, QRgb(0x404044));
    } else if (theme == QChart::ChartThemeQt) {
        pal.setColor(QPalette::Window, QRgb(0xf0f0f0));
        pal.setColor(QPalette::WindowText, QRgb(0x404044));
    } else {
        pal.setColor(QPalette::Window, QRgb(0xf0f0f0));
        pal.setColor(QPalette::WindowText, QRgb(0x404044));
    }
    foreach (QGraphicsProxyWidget *widget, m_widgetHash)
        widget->setPalette(pal);
    m_view->setBackgroundBrush(pal.color((QPalette::Window)));
    m_grid->setRubberPen(pal.color((QPalette::WindowText)));
}

void Window::comboBoxFocused(QComboBox *combobox)
{
    foreach (QGraphicsProxyWidget *widget , m_widgetHash) {
        if (widget->widget() == combobox)
            widget->setZValue(2.0);
        else
            widget->setZValue(0.0);
    }
}

void Window::handleChartSelected(QChart *qchart)
{
    if (m_templateComboBox->currentIndex() != 0)
        return;

    QAction *chosen = m_menu->exec(QCursor::pos());

    if (chosen) {
        Chart *chart = (Chart *) chosen->data().value<void *>();
        m_grid->replaceChart(qchart, chart);
        updateUI();
    }
}

QMenu *Window::createMenu()
{
    Charts::ChartList list = Charts::chartList();
    QMultiMap<QString, Chart *> categoryMap;

    QMenu *result = new QMenu(this);

    foreach (Chart *chart, list)
        categoryMap.insertMulti(chart->category(), chart);

    foreach (const QString &category, categoryMap.uniqueKeys()) {
        QMenu *menu(0);
        QMultiMap<QString, Chart *> subCategoryMap;
        if (category.isEmpty()) {
            menu = result;
        } else {
            menu = new QMenu(category, this);
            result->addMenu(menu);
        }

        foreach (Chart *chart, categoryMap.values(category))
            subCategoryMap.insert(chart->subCategory(), chart);

        foreach (const QString &subCategory, subCategoryMap.uniqueKeys()) {
            QMenu *subMenu(0);
            if (subCategory.isEmpty()) {
                subMenu = menu;
            } else {
                subMenu = new QMenu(subCategory, this);
                menu->addMenu(subMenu);
            }

            foreach (Chart *chart, subCategoryMap.values(subCategory)) {
                createMenuAction(subMenu, QIcon(), chart->name(),
                                 qVariantFromValue((void *) chart));
            }
        }
    }
    return result;
}

QAction *Window::createMenuAction(QMenu *menu, const QIcon &icon, const QString &text,
                                  const QVariant &data)
{
    QAction *action = menu->addAction(icon, text);
    action->setCheckable(false);
    action->setData(data);
    return action;
}

void Window::handleGeometryChanged()
{
    QSizeF size = m_baseLayout->sizeHint(Qt::MinimumSize);
    m_view->scene()->setSceneRect(0, 0, this->width(), this->height());
    m_view->setMinimumSize(size.toSize());
}
