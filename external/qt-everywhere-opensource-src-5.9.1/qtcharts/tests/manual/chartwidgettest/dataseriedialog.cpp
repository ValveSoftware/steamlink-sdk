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

#include "dataseriedialog.h"
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QLabel>
#include <QtCore/QDebug>

DataSerieDialog::DataSerieDialog(QWidget *parent) :
    QDialog(parent)
{
    QDialogButtonBox *addSeriesBox = new QDialogButtonBox(Qt::Horizontal);
    QPushButton *b = addSeriesBox->addButton(QDialogButtonBox::Ok);
    connect(b, SIGNAL(clicked()), this, SLOT(accept()));
    b = addSeriesBox->addButton(QDialogButtonBox::Cancel);
    connect(b, SIGNAL(clicked()), this, SLOT(reject()));

    QGridLayout *grid = new QGridLayout();

    m_seriesTypeSelector = seriesTypeSelector();
    m_columnCountSelector = columnCountSelector();
    m_rowCountSelector = rowCountSelector();
    m_dataCharacteristicsSelector = dataCharacteristicsSelector();

    grid->addWidget(m_seriesTypeSelector, 0, 0);
    grid->addWidget(m_columnCountSelector, 0, 1);
    grid->addWidget(m_rowCountSelector, 1, 1);
    grid->addWidget(m_dataCharacteristicsSelector, 1, 0);
    m_labelsSelector = new QCheckBox("Labels defined");
    m_labelsSelector->setChecked(true);
    grid->addWidget(m_labelsSelector, 2, 0);
    grid->addWidget(addSeriesBox, 3, 1);

    setLayout(grid);
}

QGroupBox *DataSerieDialog::seriesTypeSelector()
{
    QVBoxLayout *layout = new QVBoxLayout();

    QRadioButton *line = new QRadioButton("Line");
    line->setChecked(true);
    layout->addWidget(line);
    layout->addWidget(new QRadioButton("Area"));
    layout->addWidget(new QRadioButton("Pie"));
    layout->addWidget(new QRadioButton("Bar"));
    layout->addWidget(new QRadioButton("Stacked bar"));
    layout->addWidget(new QRadioButton("Percent bar"));
    layout->addWidget(new QRadioButton("Scatter"));
    layout->addWidget(new QRadioButton("Spline"));

    QGroupBox *groupBox = new QGroupBox("Series type");
    groupBox->setLayout(layout);
    selectRadio(groupBox, 0);

    return groupBox;
}

QGroupBox *DataSerieDialog::columnCountSelector()
{
    QVBoxLayout *layout = new QVBoxLayout();

    QRadioButton *radio = new QRadioButton("1");
    radio->setChecked(true);
    layout->addWidget(radio);
    layout->addWidget(new QRadioButton("2"));
    layout->addWidget(new QRadioButton("3"));
    layout->addWidget(new QRadioButton("4"));
    layout->addWidget(new QRadioButton("5"));
    layout->addWidget(new QRadioButton("8"));
    layout->addWidget(new QRadioButton("10"));
    layout->addWidget(new QRadioButton("100"));

    QGroupBox *groupBox = new QGroupBox("Column count");
    groupBox->setLayout(layout);
    selectRadio(groupBox, 0);

    return groupBox;
}

QGroupBox *DataSerieDialog::rowCountSelector()
{
    QVBoxLayout *layout = new QVBoxLayout();

    layout->addWidget(new QRadioButton("1"));
    QRadioButton *radio = new QRadioButton("10");
    radio->setChecked(true);
    layout->addWidget(radio);
    layout->addWidget(new QRadioButton("50"));
    layout->addWidget(new QRadioButton("100"));
    layout->addWidget(new QRadioButton("1000"));
    layout->addWidget(new QRadioButton("10000"));
    layout->addWidget(new QRadioButton("100000"));
    layout->addWidget(new QRadioButton("1000000"));

    QGroupBox *groupBox = new QGroupBox("Row count");
    groupBox->setLayout(layout);
    selectRadio(groupBox, 0);

    return groupBox;
}

QGroupBox *DataSerieDialog::dataCharacteristicsSelector()
{
    QVBoxLayout *layout = new QVBoxLayout();

    layout->addWidget(new QRadioButton("Linear"));
    layout->addWidget(new QRadioButton("Constant"));
    layout->addWidget(new QRadioButton("Random"));
    layout->addWidget(new QRadioButton("Sin"));
    layout->addWidget(new QRadioButton("Sin + random"));

    QGroupBox *groupBox = new QGroupBox("Data Characteristics");
    groupBox->setLayout(layout);
    selectRadio(groupBox, 0);

    return groupBox;
}

void DataSerieDialog::accept()
{
    accepted(radioSelection(m_seriesTypeSelector),
             radioSelection(m_columnCountSelector).toInt(),
             radioSelection(m_rowCountSelector).toInt(),
             radioSelection(m_dataCharacteristicsSelector),
             m_labelsSelector->isChecked());
    QDialog::accept();
}

void DataSerieDialog::selectRadio(QGroupBox *groupBox, int defaultSelection)
{
    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(groupBox->layout());
    Q_ASSERT(layout);
    Q_ASSERT(layout->count());

    QLayoutItem *item = 0;
    if (defaultSelection == -1) {
        item = layout->itemAt(0);
    } else if (layout->count() > defaultSelection) {
        item = layout->itemAt(defaultSelection);
    }
    Q_ASSERT(item);
    QRadioButton *radio = qobject_cast<QRadioButton *>(item->widget());
    Q_ASSERT(radio);
    radio->setChecked(true);
}

QString DataSerieDialog::radioSelection(QGroupBox *groupBox)
{
    QString selection;
    QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(groupBox->layout());
    Q_ASSERT(layout);

    for (int i(0); i < layout->count(); i++) {
        QLayoutItem *item = layout->itemAt(i);
        Q_ASSERT(item);
        QRadioButton *radio = qobject_cast<QRadioButton *>(item->widget());
        Q_ASSERT(radio);
        if (radio->isChecked()) {
            selection = radio->text();
            break;
        }
    }

    qDebug() << "radioSelection: " << selection;
    return selection;
}
