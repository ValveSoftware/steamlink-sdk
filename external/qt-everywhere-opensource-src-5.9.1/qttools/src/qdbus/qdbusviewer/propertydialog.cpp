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

#include "propertydialog.h"

#include <QHeaderView>
#include <QLayout>
#include <QDebug>

PropertyDialog::PropertyDialog(QWidget *parent, Qt::WindowFlags f)
    : QDialog(parent, f)
{
    buttonBox = new QDialogButtonBox;
    propertyTable = new QTableWidget;
    label = new QLabel;

    buttonBox->setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    propertyTable->setColumnCount(2);
    const QStringList labels = QStringList() << QLatin1String("Name") << QLatin1String("Value");
    propertyTable->setHorizontalHeaderLabels(labels);
    propertyTable->horizontalHeader()->setStretchLastSection(true);
    propertyTable->setEditTriggers(QAbstractItemView::AllEditTriggers);

    connect(buttonBox, SIGNAL(accepted()), SLOT(accept()), Qt::QueuedConnection);
    connect(buttonBox, SIGNAL(rejected()), SLOT(reject()), Qt::QueuedConnection);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(label);
    layout->addWidget(propertyTable);
    layout->addWidget(buttonBox);
}

void PropertyDialog::setInfo(const QString &caption)
{
    label->setText(caption);
}

void PropertyDialog::addProperty(const QString &aname, int type)
{
    int rowCount = propertyTable->rowCount();
    propertyTable->setRowCount(rowCount + 1);

    QString name = aname;
    if (name.isEmpty())
        name = QLatin1String("argument ") + QString::number(rowCount + 1);
    name += QLatin1String(" (");
    name += QLatin1String(QMetaType::typeName(type));
    name += QLatin1String(")");
    QTableWidgetItem *nameItem = new QTableWidgetItem(name);
    nameItem->setFlags(nameItem->flags() &
            ~(Qt::ItemIsEditable | Qt::ItemIsSelectable));
    propertyTable->setItem(rowCount, 0, nameItem);

    QTableWidgetItem *valueItem = new QTableWidgetItem;
    valueItem->setData(Qt::DisplayRole, QVariant(type, /* copy */ 0));
    propertyTable->setItem(rowCount, 1, valueItem);
}

int PropertyDialog::exec()
{
    propertyTable->resizeColumnToContents(0);
    propertyTable->setFocus();
    propertyTable->setCurrentCell(0, 1);
    return QDialog::exec();
}

QList<QVariant> PropertyDialog::values() const
{
    QList<QVariant> result;

    for (int i = 0; i < propertyTable->rowCount(); ++i)
        result << propertyTable->item(i, 1)->data(Qt::EditRole);

    return result;
}

