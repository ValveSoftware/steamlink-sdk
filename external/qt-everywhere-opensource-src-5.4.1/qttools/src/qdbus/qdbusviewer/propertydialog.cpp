/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the tools applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
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
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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

