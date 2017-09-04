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

#ifndef TABLEWIDGETEDITOR_H
#define TABLEWIDGETEDITOR_H

#include "ui_tablewidgeteditor.h"

#include "listwidgeteditor.h"

#include <QtWidgets/QDialog>

QT_BEGIN_NAMESPACE

class QTableWidget;
class QDesignerFormWindowInterface;

namespace qdesigner_internal {

class FormWindowBase;
class PropertySheetIconValue;

class TableWidgetEditor: public AbstractItemEditor
{
    Q_OBJECT
public:
    explicit TableWidgetEditor(QDesignerFormWindowInterface *form, QDialog *dialog);

    TableWidgetContents fillContentsFromTableWidget(QTableWidget *tableWidget);
    TableWidgetContents contents() const;

private slots:

    void on_tableWidget_currentCellChanged(int currentRow, int currnetCol, int, int);
    void on_tableWidget_itemChanged(QTableWidgetItem *item);

    void on_columnEditor_indexChanged(int idx);
    void on_columnEditor_itemChanged(int idx, int role, const QVariant &v);

    void on_columnEditor_itemInserted(int idx);
    void on_columnEditor_itemDeleted(int idx);
    void on_columnEditor_itemMovedUp(int idx);
    void on_columnEditor_itemMovedDown(int idx);

    void on_rowEditor_indexChanged(int idx);
    void on_rowEditor_itemChanged(int idx, int role, const QVariant &v);

    void on_rowEditor_itemInserted(int idx);
    void on_rowEditor_itemDeleted(int idx);
    void on_rowEditor_itemMovedUp(int idx);
    void on_rowEditor_itemMovedDown(int idx);

    void togglePropertyBrowser();

    void cacheReloaded();

protected:
    void setItemData(int role, const QVariant &v) Q_DECL_OVERRIDE;
    QVariant getItemData(int role) const Q_DECL_OVERRIDE;

private:
    void setPropertyBrowserVisible(bool v);
    void updateEditor();
    void moveColumnsLeft(int fromColumn, int toColumn);
    void moveColumnsRight(int fromColumn, int toColumn);
    void moveRowsUp(int fromRow, int toRow);
    void moveRowsDown(int fromRow, int toRow);

    Ui::TableWidgetEditor ui;
    ItemListEditor *m_rowEditor;
    ItemListEditor *m_columnEditor;
    bool m_updatingBrowser;
};

class TableWidgetEditorDialog : public QDialog
{
    Q_OBJECT
public:
    explicit TableWidgetEditorDialog(QDesignerFormWindowInterface *form, QWidget *parent);

    TableWidgetContents fillContentsFromTableWidget(QTableWidget *tableWidget);
    TableWidgetContents contents() const;

private:
    TableWidgetEditor m_editor;
};

}  // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // TABLEWIDGETEDITOR_H
