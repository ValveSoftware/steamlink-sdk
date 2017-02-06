/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Data Visualization module of the Qt Toolkit.
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

#include "scatterchart.h"

#include <QApplication>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QSlider>
#include <QComboBox>
#include <QFontComboBox>
#include <QLabel>
#include <QScreen>
#include <QFontDatabase>
#include <QLinearGradient>
#include <QPainter>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    //QCoreApplication::setAttribute(Qt::AA_UseOpenGLES);

    QWidget *widget = new QWidget;
    QHBoxLayout *hLayout = new QHBoxLayout(widget);
    QVBoxLayout *vLayout = new QVBoxLayout();
    QVBoxLayout *vLayout2 = new QVBoxLayout();
    QVBoxLayout *vLayout3 = new QVBoxLayout();

    Q3DScatter *chart = new Q3DScatter();
    QSize screenSize = chart->screen()->size();

    QWidget *container = QWidget::createWindowContainer(chart);
    container->setMinimumSize(QSize(screenSize.width() / 2, screenSize.height() / 2));
    container->setMaximumSize(screenSize);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    container->setFocusPolicy(Qt::StrongFocus);

    widget->setWindowTitle(QStringLiteral("values of some things in something"));

    hLayout->addWidget(container, 1);
    hLayout->addLayout(vLayout);
    hLayout->addLayout(vLayout2);
    hLayout->addLayout(vLayout3);

    QPushButton *themeButton = new QPushButton(widget);
    themeButton->setText(QStringLiteral("Change theme"));

    QPushButton *labelButton = new QPushButton(widget);
    labelButton->setText(QStringLiteral("Change label style"));

    QPushButton *styleButton = new QPushButton(widget);
    styleButton->setText(QStringLiteral("Change item style"));

    QPushButton *cameraButton = new QPushButton(widget);
    cameraButton->setText(QStringLiteral("Change camera preset"));

    QPushButton *clearButton = new QPushButton(widget);
    clearButton->setText(QStringLiteral("Clear chart"));

    QPushButton *resetButton = new QPushButton(widget);
    resetButton->setText(QStringLiteral("Reset axes"));

    QPushButton *addOneButton = new QPushButton(widget);
    addOneButton->setText(QStringLiteral("Add item"));

    QPushButton *addBunchButton = new QPushButton(widget);
    addBunchButton->setText(QStringLiteral("Add bunch of items"));

    QPushButton *insertOneButton = new QPushButton(widget);
    insertOneButton->setText(QStringLiteral("Insert item"));

    QPushButton *insertBunchButton = new QPushButton(widget);
    insertBunchButton->setText(QStringLiteral("Insert bunch of items"));

    QPushButton *changeOneButton = new QPushButton(widget);
    changeOneButton->setText(QStringLiteral("Change selected item"));

    QPushButton *changeBunchButton = new QPushButton(widget);
    changeBunchButton->setText(QStringLiteral("Change bunch of items"));

    QPushButton *removeOneButton = new QPushButton(widget);
    removeOneButton->setText(QStringLiteral("Remove selected item"));

    QPushButton *removeBunchButton = new QPushButton(widget);
    removeBunchButton->setText(QStringLiteral("Remove bunch of items"));

    QPushButton *setSelectedItemButton = new QPushButton(widget);
    setSelectedItemButton->setText(QStringLiteral("Select/deselect item 3"));

    QPushButton *clearSeriesDataButton = new QPushButton(widget);
    clearSeriesDataButton->setText(QStringLiteral("Clear series data"));

    QPushButton *addSeriesButton = new QPushButton(widget);
    addSeriesButton->setText(QStringLiteral("Add Series"));

    QPushButton *removeSeriesButton = new QPushButton(widget);
    removeSeriesButton->setText(QStringLiteral("Remove Series"));

    QPushButton *toggleSeriesVisibilityButton = new QPushButton(widget);
    toggleSeriesVisibilityButton->setText(QStringLiteral("Toggle visibility"));

    QPushButton *changeSeriesNameButton = new QPushButton(widget);
    changeSeriesNameButton->setText(QStringLiteral("Series name"));

    QPushButton *startTimerButton = new QPushButton(widget);
    startTimerButton->setText(QStringLiteral("Start/stop timer"));

    QPushButton *massiveDataTestButton = new QPushButton(widget);
    massiveDataTestButton->setText(QStringLiteral("Massive data test"));

    QPushButton *testItemChangesButton = new QPushButton(widget);
    testItemChangesButton->setText(QStringLiteral("Test Item changing"));

    QPushButton *testReverseButton = new QPushButton(widget);
    testReverseButton->setText(QStringLiteral("Test Axis Reversing"));

    QPushButton *renderToImageButton = new QPushButton(widget);
    renderToImageButton->setText(QStringLiteral("Render the graph to an image"));

    QLinearGradient grBtoY(0, 0, 100, 0);
    grBtoY.setColorAt(1.0, Qt::black);
    grBtoY.setColorAt(0.67, Qt::blue);
    grBtoY.setColorAt(0.33, Qt::red);
    grBtoY.setColorAt(0.0, Qt::yellow);
    QPixmap pm(100, 24);
    QPainter pmp(&pm);
    pmp.setBrush(QBrush(grBtoY));
    pmp.setPen(Qt::NoPen);
    pmp.drawRect(0, 0, 100, 24);
    QPushButton *gradientBtoYPB = new QPushButton(widget);
    gradientBtoYPB->setIcon(QIcon(pm));
    gradientBtoYPB->setIconSize(QSize(100, 24));

    QLabel *fpsLabel = new QLabel(QStringLiteral(""));

    QCheckBox *fpsCheckBox = new QCheckBox(widget);
    fpsCheckBox->setText(QStringLiteral("Measure Fps"));
    fpsCheckBox->setChecked(false);

    QCheckBox *backgroundCheckBox = new QCheckBox(widget);
    backgroundCheckBox->setText(QStringLiteral("Show background"));
    backgroundCheckBox->setChecked(true);

    QCheckBox *gridCheckBox = new QCheckBox(widget);
    gridCheckBox->setText(QStringLiteral("Show grid"));
    gridCheckBox->setChecked(true);

    QComboBox *shadowQuality = new QComboBox(widget);
    shadowQuality->addItem(QStringLiteral("None"));
    shadowQuality->addItem(QStringLiteral("Low"));
    shadowQuality->addItem(QStringLiteral("Medium"));
    shadowQuality->addItem(QStringLiteral("High"));
    shadowQuality->addItem(QStringLiteral("Low Soft"));
    shadowQuality->addItem(QStringLiteral("Medium Soft"));
    shadowQuality->addItem(QStringLiteral("High Soft"));
    shadowQuality->setCurrentIndex(0);

    QFontComboBox *fontList = new QFontComboBox(widget);

    QSlider *fontSizeSlider = new QSlider(Qt::Horizontal, widget);
    fontSizeSlider->setTickInterval(15);
    fontSizeSlider->setTickPosition(QSlider::TicksBelow);
    fontSizeSlider->setMinimum(1);
    fontSizeSlider->setValue(30);
    fontSizeSlider->setMaximum(200);

    QSlider *pointSizeSlider = new QSlider(Qt::Horizontal, widget);
    pointSizeSlider->setTickInterval(15);
    pointSizeSlider->setTickPosition(QSlider::TicksBelow);
    pointSizeSlider->setMinimum(1);
    pointSizeSlider->setValue(30);
    pointSizeSlider->setMaximum(100);

    QSlider *minSliderX = new QSlider(Qt::Horizontal, widget);
    minSliderX->setTickInterval(50);
    minSliderX->setTickPosition(QSlider::TicksBelow);
    minSliderX->setMinimum(-100);
    minSliderX->setValue(-50);
    minSliderX->setMaximum(100);

    QSlider *minSliderY = new QSlider(Qt::Horizontal, widget);
    minSliderY->setTickInterval(100);
    minSliderY->setTickPosition(QSlider::TicksBelow);
    minSliderY->setMinimum(-200);
    minSliderY->setValue(-100);
    minSliderY->setMaximum(200);

    QSlider *minSliderZ = new QSlider(Qt::Horizontal, widget);
    minSliderZ->setTickInterval(50);
    minSliderZ->setTickPosition(QSlider::TicksBelow);
    minSliderZ->setMinimum(-100);
    minSliderZ->setValue(-50);
    minSliderZ->setMaximum(100);

    QSlider *maxSliderX = new QSlider(Qt::Horizontal, widget);
    maxSliderX->setTickInterval(50);
    maxSliderX->setTickPosition(QSlider::TicksAbove);
    maxSliderX->setMinimum(-100);
    maxSliderX->setValue(50);
    maxSliderX->setMaximum(100);

    QSlider *maxSliderY = new QSlider(Qt::Horizontal, widget);
    maxSliderY->setTickInterval(100);
    maxSliderY->setTickPosition(QSlider::TicksAbove);
    maxSliderY->setMinimum(-200);
    maxSliderY->setValue(120);
    maxSliderY->setMaximum(200);

    QSlider *maxSliderZ = new QSlider(Qt::Horizontal, widget);
    maxSliderZ->setTickInterval(50);
    maxSliderZ->setTickPosition(QSlider::TicksAbove);
    maxSliderZ->setMinimum(-100);
    maxSliderZ->setValue(50);
    maxSliderZ->setMaximum(100);

    QSlider *aspectRatioSlider = new QSlider(Qt::Horizontal, widget);
    aspectRatioSlider->setTickInterval(10);
    aspectRatioSlider->setTickPosition(QSlider::TicksBelow);
    aspectRatioSlider->setMinimum(1);
    aspectRatioSlider->setValue(20);
    aspectRatioSlider->setMaximum(100);

    QSlider *horizontalAspectRatioSlider = new QSlider(Qt::Horizontal, widget);
    horizontalAspectRatioSlider->setTickInterval(30);
    horizontalAspectRatioSlider->setTickPosition(QSlider::TicksBelow);
    horizontalAspectRatioSlider->setMinimum(0);
    horizontalAspectRatioSlider->setValue(0);
    horizontalAspectRatioSlider->setMaximum(300);

    QCheckBox *optimizationStaticCB = new QCheckBox(widget);
    optimizationStaticCB->setText(QStringLiteral("Static optimization"));
    optimizationStaticCB->setChecked(false);

    QCheckBox *orthoCB = new QCheckBox(widget);
    orthoCB->setText(QStringLiteral("Orthographic projection"));
    orthoCB->setChecked(false);

    QCheckBox *polarCB = new QCheckBox(widget);
    polarCB->setText(QStringLiteral("Polar graph"));
    polarCB->setChecked(false);

    QCheckBox *axisTitlesVisibleCB = new QCheckBox(widget);
    axisTitlesVisibleCB->setText(QStringLiteral("Axis titles visible"));
    axisTitlesVisibleCB->setChecked(false);

    QCheckBox *axisTitlesFixedCB = new QCheckBox(widget);
    axisTitlesFixedCB->setText(QStringLiteral("Axis titles fixed"));
    axisTitlesFixedCB->setChecked(true);

    QSlider *axisLabelRotationSlider = new QSlider(Qt::Horizontal, widget);
    axisLabelRotationSlider->setTickInterval(10);
    axisLabelRotationSlider->setTickPosition(QSlider::TicksBelow);
    axisLabelRotationSlider->setMinimum(0);
    axisLabelRotationSlider->setValue(0);
    axisLabelRotationSlider->setMaximum(90);

    QSlider *radialLabelSlider = new QSlider(Qt::Horizontal, widget);
    radialLabelSlider->setTickInterval(10);
    radialLabelSlider->setTickPosition(QSlider::TicksBelow);
    radialLabelSlider->setMinimum(0);
    radialLabelSlider->setValue(100);
    radialLabelSlider->setMaximum(150);

    QSlider *cameraTargetSliderX = new QSlider(Qt::Horizontal, widget);
    cameraTargetSliderX->setTickInterval(1);
    cameraTargetSliderX->setMinimum(-100);
    cameraTargetSliderX->setValue(0);
    cameraTargetSliderX->setMaximum(100);
    QSlider *cameraTargetSliderY = new QSlider(Qt::Horizontal, widget);
    cameraTargetSliderY->setTickInterval(1);
    cameraTargetSliderY->setMinimum(-100);
    cameraTargetSliderY->setValue(0);
    cameraTargetSliderY->setMaximum(100);
    QSlider *cameraTargetSliderZ = new QSlider(Qt::Horizontal, widget);
    cameraTargetSliderZ->setTickInterval(1);
    cameraTargetSliderZ->setMinimum(-100);
    cameraTargetSliderZ->setValue(0);
    cameraTargetSliderZ->setMaximum(100);

    QSlider *marginSlider = new QSlider(Qt::Horizontal, widget);
    marginSlider->setMinimum(-1);
    marginSlider->setValue(-1);
    marginSlider->setMaximum(100);

    vLayout->addWidget(themeButton, 0, Qt::AlignTop);
    vLayout->addWidget(labelButton, 0, Qt::AlignTop);
    vLayout->addWidget(styleButton, 0, Qt::AlignTop);
    vLayout->addWidget(cameraButton, 0, Qt::AlignTop);
    vLayout->addWidget(clearButton, 0, Qt::AlignTop);
    vLayout->addWidget(resetButton, 0, Qt::AlignTop);
    vLayout->addWidget(addOneButton, 0, Qt::AlignTop);
    vLayout->addWidget(addBunchButton, 0, Qt::AlignTop);
    vLayout->addWidget(insertOneButton, 0, Qt::AlignTop);
    vLayout->addWidget(insertBunchButton, 0, Qt::AlignTop);
    vLayout->addWidget(changeOneButton, 0, Qt::AlignTop);
    vLayout->addWidget(changeBunchButton, 0, Qt::AlignTop);
    vLayout->addWidget(removeOneButton, 0, Qt::AlignTop);
    vLayout->addWidget(removeBunchButton, 0, Qt::AlignTop);
    vLayout->addWidget(setSelectedItemButton, 0, Qt::AlignTop);
    vLayout->addWidget(clearSeriesDataButton, 0, Qt::AlignTop);
    vLayout->addWidget(addSeriesButton, 0, Qt::AlignTop);
    vLayout->addWidget(removeSeriesButton, 0, Qt::AlignTop);
    vLayout->addWidget(toggleSeriesVisibilityButton, 0, Qt::AlignTop);
    vLayout->addWidget(changeSeriesNameButton, 0, Qt::AlignTop);
    vLayout->addWidget(startTimerButton, 0, Qt::AlignTop);
    vLayout->addWidget(massiveDataTestButton, 0, Qt::AlignTop);
    vLayout->addWidget(testItemChangesButton, 0, Qt::AlignTop);
    vLayout->addWidget(testReverseButton, 0, Qt::AlignTop);
    vLayout->addWidget(renderToImageButton, 1, Qt::AlignTop);

    vLayout2->addWidget(gradientBtoYPB, 0, Qt::AlignTop);
    vLayout2->addWidget(fpsLabel, 0, Qt::AlignTop);
    vLayout2->addWidget(fpsCheckBox, 0, Qt::AlignTop);
    vLayout2->addWidget(backgroundCheckBox);
    vLayout2->addWidget(gridCheckBox);
    vLayout2->addWidget(new QLabel(QStringLiteral("Adjust shadow quality")));
    vLayout2->addWidget(shadowQuality, 0, Qt::AlignTop);
    vLayout2->addWidget(new QLabel(QStringLiteral("Adjust point size")));
    vLayout2->addWidget(pointSizeSlider, 0, Qt::AlignTop);
    vLayout2->addWidget(new QLabel(QStringLiteral("Adjust data window")));
    vLayout2->addWidget(minSliderX, 0, Qt::AlignTop);
    vLayout2->addWidget(maxSliderX, 0, Qt::AlignTop);
    vLayout2->addWidget(minSliderY, 0, Qt::AlignTop);
    vLayout2->addWidget(maxSliderY, 0, Qt::AlignTop);
    vLayout2->addWidget(minSliderZ, 0, Qt::AlignTop);
    vLayout2->addWidget(maxSliderZ, 0, Qt::AlignTop);
    vLayout2->addWidget(new QLabel(QStringLiteral("Change font")));
    vLayout2->addWidget(fontList);
    vLayout2->addWidget(new QLabel(QStringLiteral("Adjust font size")));
    vLayout2->addWidget(fontSizeSlider);
    vLayout2->addWidget(new QLabel(QStringLiteral("Adjust vertical aspect ratio")));
    vLayout2->addWidget(aspectRatioSlider);
    vLayout2->addWidget(new QLabel(QStringLiteral("Adjust horizontal aspect ratio")));
    vLayout2->addWidget(horizontalAspectRatioSlider, 1, Qt::AlignTop);

    vLayout3->addWidget(optimizationStaticCB);
    vLayout3->addWidget(orthoCB);
    vLayout3->addWidget(polarCB);
    vLayout3->addWidget(axisTitlesVisibleCB);
    vLayout3->addWidget(axisTitlesFixedCB);
    vLayout3->addWidget(new QLabel(QStringLiteral("Axis label rotation")));
    vLayout3->addWidget(axisLabelRotationSlider);
    vLayout3->addWidget(new QLabel(QStringLiteral("Radial label offset")));
    vLayout3->addWidget(radialLabelSlider, 0, Qt::AlignTop);
    vLayout3->addWidget(new QLabel(QStringLiteral("Camera target")), 0, Qt::AlignTop);
    vLayout3->addWidget(cameraTargetSliderX, 0, Qt::AlignTop);
    vLayout3->addWidget(cameraTargetSliderY, 0, Qt::AlignTop);
    vLayout3->addWidget(cameraTargetSliderZ, 0, Qt::AlignTop);
    vLayout3->addWidget(new QLabel(QStringLiteral("Adjust margin")), 0, Qt::AlignTop);
    vLayout3->addWidget(marginSlider, 1, Qt::AlignTop);

    ScatterDataModifier *modifier = new ScatterDataModifier(chart);

    QObject::connect(fontSizeSlider, &QSlider::valueChanged, modifier,
                     &ScatterDataModifier::changeFontSize);
    QObject::connect(pointSizeSlider, &QSlider::valueChanged, modifier,
                     &ScatterDataModifier::changePointSize);

    QObject::connect(styleButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::changeStyle);
    QObject::connect(cameraButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::changePresetCamera);
    QObject::connect(clearButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::clear);
    QObject::connect(resetButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::resetAxes);
    QObject::connect(addOneButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::addOne);
    QObject::connect(addBunchButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::addBunch);
    QObject::connect(insertOneButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::insertOne);
    QObject::connect(insertBunchButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::insertBunch);
    QObject::connect(changeOneButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::changeOne);
    QObject::connect(changeBunchButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::changeBunch);
    QObject::connect(removeOneButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::removeOne);
    QObject::connect(removeBunchButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::removeBunch);
    QObject::connect(setSelectedItemButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::selectItem);
    QObject::connect(clearSeriesDataButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::clearSeriesData);
    QObject::connect(addSeriesButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::addSeries);
    QObject::connect(removeSeriesButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::removeSeries);
    QObject::connect(toggleSeriesVisibilityButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::toggleSeriesVisibility);
    QObject::connect(changeSeriesNameButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::changeSeriesName);
    QObject::connect(startTimerButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::startStopTimer);
    QObject::connect(massiveDataTestButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::massiveDataTest);
    QObject::connect(testItemChangesButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::testItemChanges);
    QObject::connect(testReverseButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::testAxisReverse);
    QObject::connect(renderToImageButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::renderToImage);
    QObject::connect(gradientBtoYPB, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::setGradient);
    QObject::connect(themeButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::changeTheme);
    QObject::connect(labelButton, &QPushButton::clicked, modifier,
                     &ScatterDataModifier::changeLabelStyle);

    QObject::connect(shadowQuality, SIGNAL(currentIndexChanged(int)), modifier,
                     SLOT(changeShadowQuality(int)));
    QObject::connect(modifier, &ScatterDataModifier::shadowQualityChanged, shadowQuality,
                     &QComboBox::setCurrentIndex);
    QObject::connect(fontList, &QFontComboBox::currentFontChanged, modifier,
                     &ScatterDataModifier::changeFont);

    QObject::connect(fpsCheckBox, &QCheckBox::stateChanged, modifier,
                     &ScatterDataModifier::setFpsMeasurement);
    QObject::connect(backgroundCheckBox, &QCheckBox::stateChanged, modifier,
                     &ScatterDataModifier::setBackgroundEnabled);
    QObject::connect(gridCheckBox, &QCheckBox::stateChanged, modifier,
                     &ScatterDataModifier::setGridEnabled);

    QObject::connect(minSliderX, &QSlider::valueChanged, modifier,
                     &ScatterDataModifier::setMinX);
    QObject::connect(minSliderY, &QSlider::valueChanged, modifier,
                     &ScatterDataModifier::setMinY);
    QObject::connect(minSliderZ, &QSlider::valueChanged, modifier,
                     &ScatterDataModifier::setMinZ);
    QObject::connect(maxSliderX, &QSlider::valueChanged, modifier,
                     &ScatterDataModifier::setMaxX);
    QObject::connect(maxSliderY, &QSlider::valueChanged, modifier,
                     &ScatterDataModifier::setMaxY);
    QObject::connect(maxSliderZ, &QSlider::valueChanged, modifier,
                     &ScatterDataModifier::setMaxZ);
    QObject::connect(optimizationStaticCB, &QCheckBox::stateChanged, modifier,
                     &ScatterDataModifier::toggleStatic);
    QObject::connect(orthoCB, &QCheckBox::stateChanged, modifier,
                     &ScatterDataModifier::toggleOrtho);
    QObject::connect(polarCB, &QCheckBox::stateChanged, modifier,
                     &ScatterDataModifier::togglePolar);
    QObject::connect(axisTitlesVisibleCB, &QCheckBox::stateChanged, modifier,
                     &ScatterDataModifier::toggleAxisTitleVisibility);
    QObject::connect(axisTitlesFixedCB, &QCheckBox::stateChanged, modifier,
                     &ScatterDataModifier::toggleAxisTitleFixed);
    QObject::connect(axisLabelRotationSlider, &QSlider::valueChanged, modifier,
                     &ScatterDataModifier::changeLabelRotation);
    QObject::connect(aspectRatioSlider, &QSlider::valueChanged, modifier,
                     &ScatterDataModifier::setAspectRatio);
    QObject::connect(horizontalAspectRatioSlider, &QSlider::valueChanged, modifier,
                     &ScatterDataModifier::setHorizontalAspectRatio);
    QObject::connect(radialLabelSlider, &QSlider::valueChanged, modifier,
                     &ScatterDataModifier::changeRadialLabelOffset);
    QObject::connect(cameraTargetSliderX, &QSlider::valueChanged, modifier,
                     &ScatterDataModifier::setCameraTargetX);
    QObject::connect(cameraTargetSliderY, &QSlider::valueChanged, modifier,
                     &ScatterDataModifier::setCameraTargetY);
    QObject::connect(cameraTargetSliderZ, &QSlider::valueChanged, modifier,
                     &ScatterDataModifier::setCameraTargetZ);
    QObject::connect(marginSlider, &QSlider::valueChanged, modifier,
                     &ScatterDataModifier::setGraphMargin);

    modifier->setFpsLabel(fpsLabel);

    chart->setGeometry(QRect(0, 0, 800, 800));

    modifier->start();
    //modifier->renderToImage(); // Initial hidden render

    widget->show();

    return app.exec();
}
