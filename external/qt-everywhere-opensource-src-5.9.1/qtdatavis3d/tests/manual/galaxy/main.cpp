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

#include "galaxydata.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QWidget>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFontComboBox>
#include <QtWidgets/QLabel>
#include <QtGui/QScreen>
#include <QtGui/QFontDatabase>

int main(int argc, char **argv)
{
    //! [0]
    QApplication app(argc, argv);
    Q3DScatter *graph = new Q3DScatter();
    QWidget *container = QWidget::createWindowContainer(graph);
    //! [0]

    QSize screenSize = graph->screen()->size();
    container->setMinimumSize(QSize(screenSize.width() / 2, screenSize.height() / 1.5));
    container->setMaximumSize(screenSize);
    container->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    container->setFocusPolicy(Qt::StrongFocus);

    QWidget *widget = new QWidget;
    QHBoxLayout *hLayout = new QHBoxLayout(widget);
    QVBoxLayout *vLayout = new QVBoxLayout();
    hLayout->addWidget(container, 1);
    hLayout->addLayout(vLayout);
    vLayout->setAlignment(Qt::AlignTop);

    widget->setWindowTitle(QStringLiteral("A Galaxy"));

    QSlider *radiusGalaxySlider = new QSlider(Qt::Horizontal, widget);
    radiusGalaxySlider->setMinimum(1000);
    radiusGalaxySlider->setMaximum(40000);
    radiusGalaxySlider->setValue(15000);
    radiusGalaxySlider->setEnabled(true);

    QSlider *radiusCoreSlider = new QSlider(Qt::Horizontal, widget);
    radiusCoreSlider->setMinimum(1000);
    radiusCoreSlider->setMaximum(30000);
    radiusCoreSlider->setValue(4000);
    radiusCoreSlider->setEnabled(true);

    QSlider *angleOffsetSlider = new QSlider(Qt::Horizontal, widget);
    angleOffsetSlider->setMinimum(10);
    angleOffsetSlider->setMaximum(70);
    angleOffsetSlider->setValue(40);
    angleOffsetSlider->setEnabled(true);

    QSlider *eccentricityInnerSlider = new QSlider(Qt::Horizontal, widget);
    eccentricityInnerSlider->setMinimum(10);
    eccentricityInnerSlider->setMaximum(200);
    eccentricityInnerSlider->setValue(90);
    eccentricityInnerSlider->setEnabled(true);

    QSlider *eccentricityOuterSlider = new QSlider(Qt::Horizontal, widget);
    eccentricityOuterSlider->setMinimum(10);
    eccentricityOuterSlider->setMaximum(200);
    eccentricityOuterSlider->setValue(90);
    eccentricityOuterSlider->setEnabled(true);

    QCheckBox *staticCheckBox = new QCheckBox(widget);
    staticCheckBox->setText(QStringLiteral("Static"));
    staticCheckBox->setChecked(false);

    QCheckBox *starsCheckBox = new QCheckBox(widget);
    starsCheckBox->setText(QStringLiteral("Stars"));
    starsCheckBox->setChecked(true);

    QCheckBox *dustCheckBox = new QCheckBox(widget);
    dustCheckBox->setText(QStringLiteral("Dust"));
    dustCheckBox->setChecked(true);

    QCheckBox *H2CheckBox = new QCheckBox(widget);
    H2CheckBox->setText(QStringLiteral("H2"));
    H2CheckBox->setChecked(true);

    QPushButton *resetButton = new QPushButton(widget);
    resetButton->setText(QStringLiteral("Reset values"));

    QCheckBox *filteredCheckBox = new QCheckBox(widget);
    filteredCheckBox->setText(QStringLiteral("Filtered"));
    filteredCheckBox->setChecked(false);

    QLabel *fpsLabel = new QLabel(QStringLiteral(""));

    vLayout->addWidget(new QLabel(QStringLiteral("Galaxy radius")));
    vLayout->addWidget(radiusGalaxySlider);
    vLayout->addWidget(new QLabel(QStringLiteral("Core radius")));
    vLayout->addWidget(radiusCoreSlider);
    vLayout->addWidget(new QLabel(QStringLiteral("Angle offset")));
    vLayout->addWidget(angleOffsetSlider);
    vLayout->addWidget(new QLabel(QStringLiteral("Eccentricity inner")));
    vLayout->addWidget(eccentricityInnerSlider);
    vLayout->addWidget(new QLabel(QStringLiteral("Eccentricity outer")));
    vLayout->addWidget(eccentricityOuterSlider);
    vLayout->addWidget(staticCheckBox);
    vLayout->addWidget(starsCheckBox);
    vLayout->addWidget(dustCheckBox);
    vLayout->addWidget(H2CheckBox);
    vLayout->addWidget(resetButton);
    vLayout->addWidget(filteredCheckBox);
    vLayout->addWidget(fpsLabel);

    GalaxyData *modifier = new GalaxyData(graph);

    QObject::connect(radiusGalaxySlider, &QSlider::valueChanged,
                     modifier, &GalaxyData::radiusGalaxyChanged);
    QObject::connect(radiusCoreSlider, &QSlider::valueChanged,
                     modifier, &GalaxyData::radiusCoreChanged);
    QObject::connect(angleOffsetSlider, &QSlider::valueChanged,
                     modifier, &GalaxyData::angleOffsetChanged);
    QObject::connect(eccentricityInnerSlider, &QSlider::valueChanged,
                     modifier, &GalaxyData::eccentricityInnerChanged);
    QObject::connect(eccentricityOuterSlider, &QSlider::valueChanged,
                     modifier, &GalaxyData::eccentricityOuterChanged);
    QObject::connect(resetButton, &QPushButton::clicked,
                     modifier, &GalaxyData::resetValues);
    QObject::connect(filteredCheckBox, &QCheckBox::stateChanged,
                     modifier, &GalaxyData::setFilteredEnabled);
    QObject::connect(staticCheckBox, &QCheckBox::stateChanged,
                     modifier, &GalaxyData::setStaticEnabled);
    QObject::connect(starsCheckBox, &QCheckBox::stateChanged,
                     modifier, &GalaxyData::setStarsVisible);
    QObject::connect(dustCheckBox, &QCheckBox::stateChanged,
                     modifier, &GalaxyData::setDustVisible);
    QObject::connect(H2CheckBox, &QCheckBox::stateChanged,
                     modifier, &GalaxyData::setH2Visible);

    modifier->setSliders(radiusGalaxySlider, radiusCoreSlider, angleOffsetSlider,
                         eccentricityInnerSlider, eccentricityOuterSlider);
    modifier->setFpsLabel(fpsLabel);

    widget->show();
    return app.exec();
}
