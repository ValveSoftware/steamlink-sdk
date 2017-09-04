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

#include "chart.h"

#include <QApplication>
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QCheckBox>
#include <QSlider>
#include <QFontComboBox>
#include <QLabel>
#include <QScreen>
#include <QFontDatabase>
#include <QLinearGradient>
#include <QPainter>
#include <QColorDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QtGui/QOpenGLContext>
#include <QtDataVisualization/QCustom3DItem>
#include <QtDataVisualization/QCustom3DLabel>
#include <QtDataVisualization/QCustom3DVolume>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    // Test creating custom items before graph is created
    QCustom3DItem customItem;
    QCustom3DLabel customLabel;
    QCustom3DVolume customVolume;

    QWidget *widget = new QWidget;
    QHBoxLayout *hLayout = new QHBoxLayout(widget);
    QVBoxLayout *vLayout = new QVBoxLayout();
    QVBoxLayout *vLayout2 = new QVBoxLayout();
    QVBoxLayout *vLayout3 = new QVBoxLayout();

    // For testing custom surface format
    QSurfaceFormat surfaceFormat;
    surfaceFormat.setDepthBufferSize(24);
    surfaceFormat.setSamples(8);

    Q3DBars *widgetchart = new Q3DBars(&surfaceFormat);
    QSize screenSize = widgetchart->screen()->size();

    QWidget *container = QWidget::createWindowContainer(widgetchart);
    container->setMinimumSize(QSize(screenSize.width() / 3, screenSize.height() / 3));
    container->setMaximumSize(screenSize);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    container->setFocusPolicy(Qt::StrongFocus);

    widget->setWindowTitle(QStringLiteral("Average temperatures in Oulu, Finland (2006-2012)"));

    hLayout->addWidget(container, 1);
    hLayout->addLayout(vLayout);
    hLayout->addLayout(vLayout2);
    hLayout->addLayout(vLayout3);

    QPushButton *addSeriesButton = new QPushButton(widget);
    addSeriesButton->setText(QStringLiteral("Add / Remove a series"));
    addSeriesButton->setEnabled(true);

    QPushButton *addDataButton = new QPushButton(widget);
    addDataButton->setText(QStringLiteral("Add a row of data"));
    addDataButton->setEnabled(false);

    QPushButton *addMultiDataButton = new QPushButton(widget);
    addMultiDataButton->setText(QStringLiteral("Add many rows of data"));
    addMultiDataButton->setEnabled(false);

    QPushButton *insertDataButton = new QPushButton(widget);
    insertDataButton->setText(QStringLiteral("Insert a row of data"));
    insertDataButton->setEnabled(false);

    QPushButton *insertMultiDataButton = new QPushButton(widget);
    insertMultiDataButton->setText(QStringLiteral("Insert many rows of data"));
    insertMultiDataButton->setEnabled(false);

    QPushButton *changeSingleDataButton = new QPushButton(widget);
    changeSingleDataButton->setText(QStringLiteral("Change selected bar value"));
    changeSingleDataButton->setEnabled(false);

    QPushButton *changeRowButton = new QPushButton(widget);
    changeRowButton->setText(QStringLiteral("Change selected row values"));
    changeRowButton->setEnabled(false);

    QPushButton *changeRowsButton = new QPushButton(widget);
    changeRowsButton->setText(QStringLiteral("Change three rows from selected"));
    changeRowsButton->setEnabled(false);

    QPushButton *removeRowButton = new QPushButton(widget);
    removeRowButton->setText(QStringLiteral("Remove selected row"));
    removeRowButton->setEnabled(false);

    QPushButton *removeRowsButton = new QPushButton(widget);
    removeRowsButton->setText(QStringLiteral("Remove three rows before selected"));
    removeRowsButton->setEnabled(false);

    QPushButton *massiveArrayButton = new QPushButton(widget);
    massiveArrayButton->setText(QStringLiteral("Create massive array"));
    massiveArrayButton->setEnabled(false);

    QPushButton *themeButton = new QPushButton(widget);
    themeButton->setText(QStringLiteral("Change theme"));

    QPushButton *labelButton = new QPushButton(widget);
    labelButton->setText(QStringLiteral("Change label style"));

    QPushButton *multiScaleButton = new QPushButton(widget);
    multiScaleButton->setText(QStringLiteral("Change multiseries scaling"));

    QPushButton *styleButton = new QPushButton(widget);
    styleButton->setText(QStringLiteral("Change bar style"));

    QPushButton *cameraButton = new QPushButton(widget);
    cameraButton->setText(QStringLiteral("Change camera preset"));

    QPushButton *selectionButton = new QPushButton(widget);
    selectionButton->setText(QStringLiteral("Change selection mode"));

    QPushButton *setSelectedBarButton = new QPushButton(widget);
    setSelectedBarButton->setText(QStringLiteral("Select/deselect bar at (5,5)"));

    QPushButton *swapAxisButton = new QPushButton(widget);
    swapAxisButton->setText(QStringLiteral("Swap value axis"));
    swapAxisButton->setEnabled(false);

    QPushButton *insertRemoveTestButton = new QPushButton(widget);
    insertRemoveTestButton->setText(QStringLiteral("Toggle insert/remove cycle"));
    insertRemoveTestButton->setEnabled(true);

    QPushButton *releaseAxesButton = new QPushButton(widget);
    releaseAxesButton->setText(QStringLiteral("Release all axes"));
    releaseAxesButton->setEnabled(true);

    QPushButton *releaseProxiesButton = new QPushButton(widget);
    releaseProxiesButton->setText(QStringLiteral("Release all proxies"));
    releaseProxiesButton->setEnabled(true);

    QPushButton *flipViewsButton = new QPushButton(widget);
    flipViewsButton->setText(QStringLiteral("Flip views"));
    flipViewsButton->setEnabled(true);

    QPushButton *showFiveSeriesButton = new QPushButton(widget);
    showFiveSeriesButton->setText(QStringLiteral("Try 5 series"));
    showFiveSeriesButton->setEnabled(true);

    QPushButton *changeColorStyleButton = new QPushButton(widget);
    changeColorStyleButton->setText(QStringLiteral("Change color style"));
    changeColorStyleButton->setEnabled(true);

    QPushButton *ownThemeButton = new QPushButton(widget);
    ownThemeButton->setText(QStringLiteral("Use own theme"));
    ownThemeButton->setEnabled(true);

    QPushButton *primarySeriesTestsButton = new QPushButton(widget);
    primarySeriesTestsButton->setText(QStringLiteral("Test primary series"));
    primarySeriesTestsButton->setEnabled(true);

    QPushButton *toggleRotationButton = new QPushButton(widget);
    toggleRotationButton->setText(QStringLiteral("Toggle rotation"));
    toggleRotationButton->setEnabled(true);

    QPushButton *logAxisButton = new QPushButton(widget);
    logAxisButton->setText(QStringLiteral("Use Log Axis"));
    logAxisButton->setEnabled(true);

    QPushButton *testItemAndRowChangesButton = new QPushButton(widget);
    testItemAndRowChangesButton->setText(QStringLiteral("Test Item/Row changing"));
    testItemAndRowChangesButton->setEnabled(true);

    QColorDialog *colorDialog = new QColorDialog(widget);

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

    QCheckBox *reverseValueAxisCheckBox = new QCheckBox(widget);
    reverseValueAxisCheckBox->setText(QStringLiteral("Reverse value axis"));
    reverseValueAxisCheckBox->setChecked(false);

    QCheckBox *backgroundCheckBox = new QCheckBox(widget);
    backgroundCheckBox->setText(QStringLiteral("Show background"));
    backgroundCheckBox->setChecked(true);

    QCheckBox *gridCheckBox = new QCheckBox(widget);
    gridCheckBox->setText(QStringLiteral("Show grid"));
    gridCheckBox->setChecked(true);

    QCheckBox *rotationCheckBox = new QCheckBox(widget);
    rotationCheckBox->setText("Rotate with slider");

    QCheckBox *staticCheckBox = new QCheckBox(widget);
    staticCheckBox->setText("Use dynamic data");
    staticCheckBox->setChecked(false);

    QCheckBox *inputHandlerRotationCheckBox = new QCheckBox(widget);
    inputHandlerRotationCheckBox->setText("IH: Allow rotation");
    inputHandlerRotationCheckBox->setChecked(true);

    QCheckBox *inputHandlerZoomCheckBox = new QCheckBox(widget);
    inputHandlerZoomCheckBox->setText("IH: Allow zoom");
    inputHandlerZoomCheckBox->setChecked(true);

    QCheckBox *inputHandlerSelectionCheckBox = new QCheckBox(widget);
    inputHandlerSelectionCheckBox->setText("IH: Allow selection");
    inputHandlerSelectionCheckBox->setChecked(true);

    QCheckBox *inputHandlerZoomAtTargetCheckBox = new QCheckBox(widget);
    inputHandlerZoomAtTargetCheckBox->setText("IH: setZoomAtTarget");
    inputHandlerZoomAtTargetCheckBox->setChecked(true);

    QSlider *rotationSliderX = new QSlider(Qt::Horizontal, widget);
    rotationSliderX->setTickInterval(1);
    rotationSliderX->setMinimum(-180);
    rotationSliderX->setValue(0);
    rotationSliderX->setMaximum(180);
    rotationSliderX->setEnabled(false);
    QSlider *rotationSliderY = new QSlider(Qt::Horizontal, widget);
    rotationSliderY->setTickInterval(1);
    rotationSliderY->setMinimum(0);
    rotationSliderY->setValue(0);
    rotationSliderY->setMaximum(90);
    rotationSliderY->setEnabled(false);

    QSlider *ratioSlider = new QSlider(Qt::Horizontal, widget);
    ratioSlider->setTickInterval(1);
    ratioSlider->setMinimum(10);
    ratioSlider->setValue(30);
    ratioSlider->setMaximum(100);

    QCheckBox *reflectionCheckBox = new QCheckBox(widget);
    reflectionCheckBox->setText(QStringLiteral("Show reflections"));
    reflectionCheckBox->setChecked(false);

    QSlider *reflectivitySlider = new QSlider(Qt::Horizontal, widget);
    reflectivitySlider->setMinimum(0);
    reflectivitySlider->setValue(50);
    reflectivitySlider->setMaximum(100);

    QSlider *floorLevelSlider = new QSlider(Qt::Horizontal, widget);
    floorLevelSlider->setMinimum(-50);
    floorLevelSlider->setValue(0);
    floorLevelSlider->setMaximum(50);

    QPushButton *toggleCustomItemButton = new QPushButton(widget);
    toggleCustomItemButton->setText(QStringLiteral("Toggle Custom Item"));

    QSlider *spacingSliderX = new QSlider(Qt::Horizontal, widget);
    spacingSliderX->setTickInterval(1);
    spacingSliderX->setMinimum(0);
    spacingSliderX->setValue(10);
    spacingSliderX->setMaximum(200);
    QSlider *spacingSliderZ = new QSlider(Qt::Horizontal, widget);
    spacingSliderZ->setTickInterval(1);
    spacingSliderZ->setMinimum(0);
    spacingSliderZ->setValue(10);
    spacingSliderZ->setMaximum(200);

    QSlider *sampleSliderX = new QSlider(Qt::Horizontal, widget);
    sampleSliderX->setTickInterval(1);
    sampleSliderX->setMinimum(1);
    sampleSliderX->setValue(21);
    sampleSliderX->setMaximum(200);
    sampleSliderX->setEnabled(false);
    QSlider *sampleSliderZ = new QSlider(Qt::Horizontal, widget);
    sampleSliderZ->setTickInterval(1);
    sampleSliderZ->setMinimum(1);
    sampleSliderZ->setValue(21);
    sampleSliderZ->setMaximum(200);
    sampleSliderZ->setEnabled(false);

    QSlider *minSliderX = new QSlider(Qt::Horizontal, widget);
    minSliderX->setTickInterval(10);
    minSliderX->setTickPosition(QSlider::TicksBelow);
    minSliderX->setMinimum(0);
    minSliderX->setValue(0);
    minSliderX->setMaximum(200);
    minSliderX->setEnabled(false);
    QSlider *minSliderZ = new QSlider(Qt::Horizontal, widget);
    minSliderZ->setTickInterval(10);
    minSliderZ->setTickPosition(QSlider::TicksAbove);
    minSliderZ->setMinimum(0);
    minSliderZ->setValue(0);
    minSliderZ->setMaximum(200);
    minSliderZ->setEnabled(false);
    QSlider *minSliderY = new QSlider(Qt::Horizontal, widget);
    minSliderY->setTickInterval(10);
    minSliderY->setTickPosition(QSlider::TicksBelow);
    minSliderY->setMinimum(-100);
    minSliderY->setValue(0);
    minSliderY->setMaximum(100);
    minSliderY->setEnabled(false);
    QSlider *maxSliderY = new QSlider(Qt::Horizontal, widget);
    maxSliderY->setTickInterval(10);
    maxSliderY->setTickPosition(QSlider::TicksAbove);
    maxSliderY->setMinimum(-50);
    maxSliderY->setValue(100);
    maxSliderY->setMaximum(200);
    maxSliderY->setEnabled(false);

    QSlider *fontSizeSlider = new QSlider(Qt::Horizontal, widget);
    fontSizeSlider->setTickInterval(1);
    fontSizeSlider->setMinimum(1);
    fontSizeSlider->setValue(20);
    fontSizeSlider->setMaximum(100);

    QFontComboBox *fontList = new QFontComboBox(widget);

    QComboBox *shadowQuality = new QComboBox(widget);
    shadowQuality->addItem(QStringLiteral("None"));
    shadowQuality->addItem(QStringLiteral("Low"));
    shadowQuality->addItem(QStringLiteral("Medium"));
    shadowQuality->addItem(QStringLiteral("High"));
    shadowQuality->addItem(QStringLiteral("Low Soft"));
    shadowQuality->addItem(QStringLiteral("Medium Soft"));
    shadowQuality->addItem(QStringLiteral("High Soft"));
    shadowQuality->setCurrentIndex(5);

    QLineEdit *valueAxisFormatEdit = new QLineEdit(widget);
    QLineEdit *logBaseEdit = new QLineEdit(widget);
    QSpinBox *valueAxisSegmentsSpin = new QSpinBox(widget);
    valueAxisSegmentsSpin->setMinimum(1);
    valueAxisSegmentsSpin->setMaximum(100);
    valueAxisSegmentsSpin->setValue(10);

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

    vLayout->addWidget(addSeriesButton, 0, Qt::AlignTop);
    vLayout->addWidget(addDataButton, 0, Qt::AlignTop);
    vLayout->addWidget(addMultiDataButton, 0, Qt::AlignTop);
    vLayout->addWidget(insertDataButton, 0, Qt::AlignTop);
    vLayout->addWidget(insertMultiDataButton, 0, Qt::AlignTop);
    vLayout->addWidget(changeSingleDataButton, 0, Qt::AlignTop);
    vLayout->addWidget(changeRowButton, 0, Qt::AlignTop);
    vLayout->addWidget(changeRowsButton, 0, Qt::AlignTop);
    vLayout->addWidget(removeRowButton, 0, Qt::AlignTop);
    vLayout->addWidget(removeRowsButton, 0, Qt::AlignTop);
    vLayout->addWidget(massiveArrayButton, 0, Qt::AlignTop);
    vLayout->addWidget(showFiveSeriesButton, 0, Qt::AlignTop);
    vLayout->addWidget(themeButton, 0, Qt::AlignTop);
    vLayout->addWidget(labelButton, 0, Qt::AlignTop);
    vLayout->addWidget(multiScaleButton, 0, Qt::AlignTop);
    vLayout->addWidget(styleButton, 0, Qt::AlignTop);
    vLayout->addWidget(cameraButton, 0, Qt::AlignTop);
    vLayout->addWidget(selectionButton, 0, Qt::AlignTop);
    vLayout->addWidget(setSelectedBarButton, 0, Qt::AlignTop);
    vLayout->addWidget(swapAxisButton, 0, Qt::AlignTop);
    vLayout->addWidget(insertRemoveTestButton, 0, Qt::AlignTop);
    vLayout->addWidget(releaseAxesButton, 0, Qt::AlignTop);
    vLayout->addWidget(releaseProxiesButton, 1, Qt::AlignTop);

    vLayout2->addWidget(flipViewsButton, 0, Qt::AlignTop);
    vLayout2->addWidget(changeColorStyleButton, 0, Qt::AlignTop);
    vLayout2->addWidget(ownThemeButton, 0, Qt::AlignTop);
    vLayout2->addWidget(primarySeriesTestsButton, 0, Qt::AlignTop);
    vLayout2->addWidget(toggleRotationButton, 0, Qt::AlignTop);
    vLayout2->addWidget(gradientBtoYPB, 0, Qt::AlignTop);
    vLayout2->addWidget(logAxisButton, 0, Qt::AlignTop);
    vLayout2->addWidget(testItemAndRowChangesButton, 0, Qt::AlignTop);
    vLayout2->addWidget(staticCheckBox, 0, Qt::AlignTop);
    vLayout2->addWidget(rotationCheckBox, 0, Qt::AlignTop);
    vLayout2->addWidget(rotationSliderX, 0, Qt::AlignTop);
    vLayout2->addWidget(rotationSliderY, 0, Qt::AlignTop);
    vLayout2->addWidget(new QLabel(QStringLiteral("Adjust relative bar size")), 0, Qt::AlignTop);
    vLayout2->addWidget(ratioSlider, 0, Qt::AlignTop);
    vLayout2->addWidget(new QLabel(QStringLiteral("Adjust relative bar spacing")), 0, Qt::AlignTop);
    vLayout2->addWidget(spacingSliderX, 0, Qt::AlignTop);
    vLayout2->addWidget(spacingSliderZ, 0, Qt::AlignTop);
    vLayout2->addWidget(new QLabel(QStringLiteral("Adjust sample count")), 0, Qt::AlignTop);
    vLayout2->addWidget(sampleSliderX, 0, Qt::AlignTop);
    vLayout2->addWidget(sampleSliderZ, 0, Qt::AlignTop);
    vLayout2->addWidget(new QLabel(QStringLiteral("Adjust data window minimums")), 0, Qt::AlignTop);
    vLayout2->addWidget(minSliderX, 0, Qt::AlignTop);
    vLayout2->addWidget(minSliderZ, 0, Qt::AlignTop);
    vLayout2->addWidget(minSliderY, 0, Qt::AlignTop);
    vLayout2->addWidget(maxSliderY, 1, Qt::AlignTop);

    vLayout3->addWidget(fpsLabel, 0, Qt::AlignTop);
    vLayout3->addWidget(fpsCheckBox, 0, Qt::AlignTop);
    vLayout3->addWidget(reverseValueAxisCheckBox, 0, Qt::AlignTop);
    vLayout3->addWidget(backgroundCheckBox, 0, Qt::AlignTop);
    vLayout3->addWidget(gridCheckBox, 0, Qt::AlignTop);
    vLayout3->addWidget(inputHandlerRotationCheckBox, 0, Qt::AlignTop);
    vLayout3->addWidget(inputHandlerZoomCheckBox, 0, Qt::AlignTop);
    vLayout3->addWidget(inputHandlerSelectionCheckBox, 0, Qt::AlignTop);
    vLayout3->addWidget(inputHandlerZoomAtTargetCheckBox, 0, Qt::AlignTop);
    vLayout3->addWidget(new QLabel(QStringLiteral("Adjust shadow quality")), 0, Qt::AlignTop);
    vLayout3->addWidget(shadowQuality, 0, Qt::AlignTop);
    vLayout3->addWidget(new QLabel(QStringLiteral("Change font")), 0, Qt::AlignTop);
    vLayout3->addWidget(fontList, 0, Qt::AlignTop);
    vLayout3->addWidget(new QLabel(QStringLiteral("Adjust font size")), 0, Qt::AlignTop);
    vLayout3->addWidget(fontSizeSlider, 0, Qt::AlignTop);
    vLayout3->addWidget(new QLabel(QStringLiteral("Value axis format")), 0, Qt::AlignTop);
    vLayout3->addWidget(valueAxisFormatEdit, 0, Qt::AlignTop);
    vLayout3->addWidget(new QLabel(QStringLiteral("Log axis base")), 0, Qt::AlignTop);
    vLayout3->addWidget(logBaseEdit, 0, Qt::AlignTop);
    vLayout3->addWidget(new QLabel(QStringLiteral("Value axis segments")), 0, Qt::AlignTop);
    vLayout3->addWidget(valueAxisSegmentsSpin, 0, Qt::AlignTop);
    vLayout3->addWidget(new QLabel(QStringLiteral("Camera target")), 0, Qt::AlignTop);
    vLayout3->addWidget(cameraTargetSliderX, 0, Qt::AlignTop);
    vLayout3->addWidget(cameraTargetSliderY, 0, Qt::AlignTop);
    vLayout3->addWidget(cameraTargetSliderZ, 0, Qt::AlignTop);
    vLayout3->addWidget(reflectionCheckBox, 0, Qt::AlignTop);
    vLayout3->addWidget(reflectivitySlider, 0, Qt::AlignTop);
    vLayout3->addWidget(toggleCustomItemButton, 0, Qt::AlignTop);
    vLayout3->addWidget(new QLabel(QStringLiteral("Adjust floor level")), 0, Qt::AlignTop);
    vLayout3->addWidget(floorLevelSlider, 0, Qt::AlignTop);
    vLayout3->addWidget(new QLabel(QStringLiteral("Adjust margin")), 0, Qt::AlignTop);
    vLayout3->addWidget(marginSlider, 1, Qt::AlignTop);

    widget->show();

    GraphModifier *modifier = new GraphModifier(widgetchart, colorDialog);

    QObject::connect(rotationSliderX, &QSlider::valueChanged, modifier, &GraphModifier::rotateX);
    QObject::connect(rotationSliderY, &QSlider::valueChanged, modifier, &GraphModifier::rotateY);

    QObject::connect(ratioSlider, &QSlider::valueChanged, modifier, &GraphModifier::setSpecsRatio);

    QObject::connect(spacingSliderX, &QSlider::valueChanged, modifier,
                     &GraphModifier::setSpacingSpecsX);
    QObject::connect(spacingSliderZ, &QSlider::valueChanged, modifier,
                     &GraphModifier::setSpacingSpecsZ);

    QObject::connect(sampleSliderX, &QSlider::valueChanged, modifier,
                     &GraphModifier::setSampleCountX);
    QObject::connect(sampleSliderZ, &QSlider::valueChanged, modifier,
                     &GraphModifier::setSampleCountZ);
    QObject::connect(minSliderX, &QSlider::valueChanged, modifier,
                     &GraphModifier::setMinX);
    QObject::connect(minSliderZ, &QSlider::valueChanged, modifier,
                     &GraphModifier::setMinZ);
    QObject::connect(minSliderY, &QSlider::valueChanged, modifier,
                     &GraphModifier::setMinY);
    QObject::connect(maxSliderY, &QSlider::valueChanged, modifier,
                     &GraphModifier::setMaxY);
    QObject::connect(cameraTargetSliderX, &QSlider::valueChanged, modifier,
                     &GraphModifier::setCameraTargetX);
    QObject::connect(cameraTargetSliderY, &QSlider::valueChanged, modifier,
                     &GraphModifier::setCameraTargetY);
    QObject::connect(cameraTargetSliderZ, &QSlider::valueChanged, modifier,
                     &GraphModifier::setCameraTargetZ);

    QObject::connect(shadowQuality, SIGNAL(currentIndexChanged(int)), modifier,
                     SLOT(changeShadowQuality(int)));
    QObject::connect(modifier, &GraphModifier::shadowQualityChanged, shadowQuality,
                     &QComboBox::setCurrentIndex);
    QObject::connect(fontSizeSlider, &QSlider::valueChanged, modifier,
                     &GraphModifier::changeFontSize);
    QObject::connect(valueAxisFormatEdit, &QLineEdit::textEdited, modifier,
                     &GraphModifier::changeValueAxisFormat);
    QObject::connect(logBaseEdit, &QLineEdit::textEdited, modifier,
                     &GraphModifier::changeLogBase);
    QObject::connect(valueAxisSegmentsSpin, SIGNAL(valueChanged(int)), modifier,
                     SLOT(changeValueAxisSegments(int)));

    QObject::connect(multiScaleButton, &QPushButton::clicked, modifier,
                     &GraphModifier::toggleMultiseriesScaling);
    QObject::connect(styleButton, &QPushButton::clicked, modifier, &GraphModifier::changeStyle);
    QObject::connect(cameraButton, &QPushButton::clicked, modifier,
                     &GraphModifier::changePresetCamera);
    QObject::connect(themeButton, &QPushButton::clicked, modifier, &GraphModifier::changeTheme);
    QObject::connect(labelButton, &QPushButton::clicked, modifier,
                     &GraphModifier::changeLabelStyle);
    QObject::connect(addDataButton, &QPushButton::clicked, modifier, &GraphModifier::addRow);
    QObject::connect(addSeriesButton, &QPushButton::clicked, modifier, &GraphModifier::addRemoveSeries);
    QObject::connect(addMultiDataButton, &QPushButton::clicked, modifier, &GraphModifier::addRows);
    QObject::connect(insertDataButton, &QPushButton::clicked, modifier, &GraphModifier::insertRow);
    QObject::connect(insertMultiDataButton, &QPushButton::clicked, modifier, &GraphModifier::insertRows);
    QObject::connect(changeSingleDataButton, &QPushButton::clicked, modifier, &GraphModifier::changeItem);
    QObject::connect(changeRowButton, &QPushButton::clicked, modifier, &GraphModifier::changeRow);
    QObject::connect(changeRowsButton, &QPushButton::clicked, modifier, &GraphModifier::changeRows);
    QObject::connect(removeRowButton, &QPushButton::clicked, modifier, &GraphModifier::removeRow);
    QObject::connect(removeRowsButton, &QPushButton::clicked, modifier, &GraphModifier::removeRows);
    QObject::connect(massiveArrayButton, &QPushButton::clicked, modifier, &GraphModifier::createMassiveArray);
    QObject::connect(showFiveSeriesButton, &QPushButton::clicked, modifier, &GraphModifier::showFiveSeries);
    QObject::connect(selectionButton, &QPushButton::clicked, modifier,
                     &GraphModifier::changeSelectionMode);
    QObject::connect(setSelectedBarButton, &QPushButton::clicked, modifier,
                     &GraphModifier::selectBar);
    QObject::connect(swapAxisButton, &QPushButton::clicked, modifier,
                     &GraphModifier::swapAxis);
    QObject::connect(insertRemoveTestButton, &QPushButton::clicked, modifier,
                     &GraphModifier::insertRemoveTestToggle);
    QObject::connect(releaseAxesButton, &QPushButton::clicked, modifier,
                     &GraphModifier::releaseAxes);
    QObject::connect(releaseProxiesButton, &QPushButton::clicked, modifier,
                     &GraphModifier::releaseSeries);

    QObject::connect(flipViewsButton, &QPushButton::clicked, modifier,
                     &GraphModifier::flipViews);
    QObject::connect(changeColorStyleButton, &QPushButton::clicked, modifier,
                     &GraphModifier::changeColorStyle);
    QObject::connect(ownThemeButton, &QPushButton::clicked, modifier,
                     &GraphModifier::useOwnTheme);
    QObject::connect(primarySeriesTestsButton, &QPushButton::clicked, modifier,
                     &GraphModifier::primarySeriesTest);
    QObject::connect(toggleRotationButton, &QPushButton::clicked, modifier,
                     &GraphModifier::toggleRotation);
    QObject::connect(logAxisButton, &QPushButton::clicked, modifier,
                     &GraphModifier::useLogAxis);
    QObject::connect(testItemAndRowChangesButton, &QPushButton::clicked, modifier,
                     &GraphModifier::testItemAndRowChanges);
    QObject::connect(colorDialog, &QColorDialog::currentColorChanged, modifier,
                     &GraphModifier::changeBaseColor);
    QObject::connect(gradientBtoYPB, &QPushButton::clicked, modifier,
                     &GraphModifier::setGradient);

    QObject::connect(fontList, &QFontComboBox::currentFontChanged, modifier,
                     &GraphModifier::changeFont);

    QObject::connect(fpsCheckBox, &QCheckBox::stateChanged, modifier,
                     &GraphModifier::setFpsMeasurement);
    QObject::connect(reverseValueAxisCheckBox, &QCheckBox::stateChanged, modifier,
                     &GraphModifier::reverseValueAxis);
    QObject::connect(backgroundCheckBox, &QCheckBox::stateChanged, modifier,
                     &GraphModifier::setBackgroundEnabled);
    QObject::connect(gridCheckBox, &QCheckBox::stateChanged, modifier,
                     &GraphModifier::setGridEnabled);
    QObject::connect(inputHandlerRotationCheckBox, &QCheckBox::stateChanged, modifier,
                     &GraphModifier::setInputHandlerRotationEnabled);
    QObject::connect(inputHandlerZoomCheckBox, &QCheckBox::stateChanged, modifier,
                     &GraphModifier::setInputHandlerZoomEnabled);
    QObject::connect(inputHandlerSelectionCheckBox, &QCheckBox::stateChanged, modifier,
                     &GraphModifier::setInputHandlerSelectionEnabled);
    QObject::connect(inputHandlerZoomAtTargetCheckBox, &QCheckBox::stateChanged, modifier,
                     &GraphModifier::setInputHandlerZoomAtTargetEnabled);
    QObject::connect(rotationCheckBox, &QCheckBox::stateChanged, modifier,
                     &GraphModifier::setUseNullInputHandler);

    QObject::connect(rotationCheckBox, &QCheckBox::stateChanged, rotationSliderX,
                     &QSlider::setEnabled);
    QObject::connect(rotationCheckBox, &QCheckBox::stateChanged, rotationSliderX,
                     &QSlider::setValue);
    QObject::connect(rotationCheckBox, &QCheckBox::stateChanged, rotationSliderY,
                     &QSlider::setEnabled);
    QObject::connect(rotationCheckBox, &QCheckBox::stateChanged, rotationSliderY,
                     &QSlider::setValue);

    QObject::connect(reflectionCheckBox, &QCheckBox::stateChanged, modifier,
                     &GraphModifier::setReflection);
    QObject::connect(reflectivitySlider, &QSlider::valueChanged, modifier,
                     &GraphModifier::setReflectivity);
    QObject::connect(floorLevelSlider, &QSlider::valueChanged, modifier,
                     &GraphModifier::setFloorLevel);
    QObject::connect(marginSlider, &QSlider::valueChanged, modifier,
                     &GraphModifier::setGraphMargin);
    QObject::connect(toggleCustomItemButton, &QPushButton::clicked, modifier,
                     &GraphModifier::toggleCustomItem);

    QObject::connect(staticCheckBox, &QCheckBox::stateChanged, addDataButton,
                     &QPushButton::setEnabled);
    QObject::connect(staticCheckBox, &QCheckBox::stateChanged, addMultiDataButton,
                     &QPushButton::setEnabled);
    QObject::connect(staticCheckBox, &QCheckBox::stateChanged, insertDataButton,
                     &QPushButton::setEnabled);
    QObject::connect(staticCheckBox, &QCheckBox::stateChanged, insertMultiDataButton,
                     &QPushButton::setEnabled);
    QObject::connect(staticCheckBox, &QCheckBox::stateChanged, changeSingleDataButton,
                     &QPushButton::setEnabled);
    QObject::connect(staticCheckBox, &QCheckBox::stateChanged, changeRowButton,
                     &QPushButton::setEnabled);
    QObject::connect(staticCheckBox, &QCheckBox::stateChanged, changeRowsButton,
                     &QPushButton::setEnabled);
    QObject::connect(staticCheckBox, &QCheckBox::stateChanged, removeRowButton,
                     &QPushButton::setEnabled);
    QObject::connect(staticCheckBox, &QCheckBox::stateChanged, removeRowsButton,
                     &QPushButton::setEnabled);
    QObject::connect(staticCheckBox, &QCheckBox::stateChanged, massiveArrayButton,
                     &QPushButton::setEnabled);
    QObject::connect(staticCheckBox, &QCheckBox::stateChanged, sampleSliderX,
                     &QSlider::setEnabled);
    QObject::connect(staticCheckBox, &QCheckBox::stateChanged, sampleSliderZ,
                     &QSlider::setEnabled);
    QObject::connect(staticCheckBox, &QCheckBox::stateChanged, minSliderX,
                     &QSlider::setEnabled);
    QObject::connect(staticCheckBox, &QCheckBox::stateChanged, minSliderZ,
                     &QSlider::setEnabled);
    QObject::connect(staticCheckBox, &QCheckBox::stateChanged, minSliderY,
                     &QSlider::setEnabled);
    QObject::connect(staticCheckBox, &QCheckBox::stateChanged, maxSliderY,
                     &QSlider::setEnabled);
    QObject::connect(staticCheckBox, &QCheckBox::stateChanged, swapAxisButton,
                     &QSlider::setEnabled);
    QObject::connect(staticCheckBox, &QCheckBox::stateChanged, modifier, &GraphModifier::restart);

    modifier->setFpsLabel(fpsLabel);

    modifier->start();

    return app.exec();
}
