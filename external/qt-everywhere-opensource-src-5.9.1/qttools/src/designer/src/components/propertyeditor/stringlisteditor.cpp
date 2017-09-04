/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#include "stringlisteditor.h"
#include <iconloader_p.h>
#include <QtCore/QStringListModel>

QT_BEGIN_NAMESPACE

using namespace qdesigner_internal;

StringListEditor::StringListEditor(QWidget *parent)
    : QDialog(parent), m_model(new QStringListModel(this))
{
    setupUi(this);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    listView->setModel(m_model);

    connect(listView->selectionModel(),
            &QItemSelectionModel::currentChanged,
            this, &StringListEditor::currentIndexChanged);
    connect(listView->itemDelegate(),
            &QAbstractItemDelegate::closeEditor,
            this, &StringListEditor::currentValueChanged);

    QIcon upIcon = createIconSet(QString::fromUtf8("up.png"));
    QIcon downIcon = createIconSet(QString::fromUtf8("down.png"));
    QIcon minusIcon = createIconSet(QString::fromUtf8("minus.png"));
    QIcon plusIcon = createIconSet(QString::fromUtf8("plus.png"));
    upButton->setIcon(upIcon);
    downButton->setIcon(downIcon);
    newButton->setIcon(plusIcon);
    deleteButton->setIcon(minusIcon);

    updateUi();
}

StringListEditor::~StringListEditor()
{
}

QStringList StringListEditor::getStringList(QWidget *parent, const QStringList &init, int *result)
{
    StringListEditor dlg(parent);
    dlg.setStringList(init);
    int res = dlg.exec();
    if (result)
        *result = res;
    return (res == QDialog::Accepted) ? dlg.stringList() : init;
}

void StringListEditor::setStringList(const QStringList &stringList)
{
    m_model->setStringList(stringList);
    updateUi();
}

QStringList StringListEditor::stringList() const
{
    return m_model->stringList();
}

void StringListEditor::currentIndexChanged(const QModelIndex &current, const QModelIndex &previous)
{
    Q_UNUSED(previous);
    setCurrentIndex(current.row());
    updateUi();
}

void StringListEditor::currentValueChanged()
{
    setCurrentIndex(currentIndex());
    updateUi();
}

void StringListEditor::on_upButton_clicked()
{
    int from = currentIndex();
    int to = currentIndex() - 1;
    QString value = stringAt(from);
    removeString(from);
    insertString(to, value);
    setCurrentIndex(to);
    updateUi();
}

void StringListEditor::on_downButton_clicked()
{
    int from = currentIndex();
    int to = currentIndex() + 1;
    QString value = stringAt(from);
    removeString(from);
    insertString(to, value);
    setCurrentIndex(to);
    updateUi();
}

void StringListEditor::on_newButton_clicked()
{
    int to = currentIndex();
    if (to == -1)
        to = count() - 1;
    ++to;
    insertString(to, QString());
    setCurrentIndex(to);
    updateUi();
    editString(to);
}

void StringListEditor::on_deleteButton_clicked()
{
    removeString(currentIndex());
    setCurrentIndex(currentIndex());
    updateUi();
}

void StringListEditor::on_valueEdit_textEdited(const QString &text)
{
    setStringAt(currentIndex(), text);
}

void StringListEditor::updateUi()
{
    upButton->setEnabled((count() > 1) && (currentIndex() > 0));
    downButton->setEnabled((count() > 1) && (currentIndex() >= 0) && (currentIndex() < (count() - 1)));
    deleteButton->setEnabled(currentIndex() != -1);
    valueEdit->setEnabled(currentIndex() != -1);
}

int StringListEditor::currentIndex() const
{
    return listView->currentIndex().row();
}

void StringListEditor::setCurrentIndex(int index)
{
    QModelIndex modelIndex = m_model->index(index, 0);
    if (listView->currentIndex() != modelIndex)
        listView->setCurrentIndex(modelIndex);
    valueEdit->setText(stringAt(index));
}

int StringListEditor::count() const
{
    return m_model->rowCount();
}

QString StringListEditor::stringAt(int index) const
{
    return qvariant_cast<QString>(m_model->data(m_model->index(index, 0), Qt::DisplayRole));
}

void StringListEditor::setStringAt(int index, const QString &value)
{
    m_model->setData(m_model->index(index, 0), value);
}

void StringListEditor::removeString(int index)
{
    m_model->removeRows(index, 1);
}

void StringListEditor::insertString(int index, const QString &value)
{
    m_model->insertRows(index, 1);
    m_model->setData(m_model->index(index, 0), value);
}

void StringListEditor::editString(int index)
{
    listView->edit(m_model->index(index, 0));
}

QT_END_NAMESPACE
