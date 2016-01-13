/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef BROWSER_ACCESSIBILITY_QT_H
#define BROWSER_ACCESSIBILITY_QT_H

#include <QtGui/qaccessible.h>
#include "content/browser/accessibility/browser_accessibility.h"

namespace content {

class BrowserAccessibilityQt
    : public BrowserAccessibility
    , public QAccessibleInterface
    , public QAccessibleActionInterface
    , public QAccessibleTextInterface
    , public QAccessibleValueInterface
    , public QAccessibleTableInterface
    , public QAccessibleTableCellInterface
{
public:
    BrowserAccessibilityQt();

    // BrowserAccessibility
    virtual void OnDataChanged() Q_DECL_OVERRIDE;

    // QAccessibleInterface
    virtual bool isValid() const Q_DECL_OVERRIDE;
    virtual QObject *object() const Q_DECL_OVERRIDE;
    virtual QAccessibleInterface *childAt(int x, int y) const Q_DECL_OVERRIDE;
    virtual void *interface_cast(QAccessible::InterfaceType type) Q_DECL_OVERRIDE;

    // navigation, hierarchy
    virtual QAccessibleInterface *parent() const Q_DECL_OVERRIDE;
    virtual QAccessibleInterface *child(int index) const Q_DECL_OVERRIDE;
    virtual int childCount() const Q_DECL_OVERRIDE;
    virtual int indexOfChild(const QAccessibleInterface *) const Q_DECL_OVERRIDE;

    // properties and state
    virtual QString text(QAccessible::Text t) const Q_DECL_OVERRIDE;
    virtual void setText(QAccessible::Text t, const QString &text) Q_DECL_OVERRIDE;
    virtual QRect rect() const Q_DECL_OVERRIDE;
    virtual QAccessible::Role role() const Q_DECL_OVERRIDE;
    virtual QAccessible::State state() const Q_DECL_OVERRIDE;

    // BrowserAccessible
    void NativeAddReference() Q_DECL_OVERRIDE;
    void NativeReleaseReference() Q_DECL_OVERRIDE;
    bool IsNative() const Q_DECL_OVERRIDE { return true; }

    // QAccessibleActionInterface
    QStringList actionNames() const Q_DECL_OVERRIDE;
    void doAction(const QString &actionName) Q_DECL_OVERRIDE;
    QStringList keyBindingsForAction(const QString &actionName) const Q_DECL_OVERRIDE;

    // QAccessibleTextInterface
    void addSelection(int startOffset, int endOffset) Q_DECL_OVERRIDE;
    QString attributes(int offset, int *startOffset, int *endOffset) const Q_DECL_OVERRIDE;
    int cursorPosition() const Q_DECL_OVERRIDE;
    QRect characterRect(int offset) const Q_DECL_OVERRIDE;
    int selectionCount() const Q_DECL_OVERRIDE;
    int offsetAtPoint(const QPoint &point) const Q_DECL_OVERRIDE;
    void selection(int selectionIndex, int *startOffset, int *endOffset) const Q_DECL_OVERRIDE;
    QString text(int startOffset, int endOffset) const Q_DECL_OVERRIDE;
    void removeSelection(int selectionIndex) Q_DECL_OVERRIDE;
    void setCursorPosition(int position) Q_DECL_OVERRIDE;
    void setSelection(int selectionIndex, int startOffset, int endOffset) Q_DECL_OVERRIDE;
    int characterCount() const Q_DECL_OVERRIDE;
    void scrollToSubstring(int startIndex, int endIndex) Q_DECL_OVERRIDE;

    // QAccessibleValueInterface
    QVariant currentValue() const Q_DECL_OVERRIDE;
    void setCurrentValue(const QVariant &value) Q_DECL_OVERRIDE;
    QVariant maximumValue() const Q_DECL_OVERRIDE;
    QVariant minimumValue() const Q_DECL_OVERRIDE;
    QVariant minimumStepSize() const Q_DECL_OVERRIDE;

    // QAccessibleTableInterface
    virtual QAccessibleInterface *cellAt(int row, int column) const Q_DECL_OVERRIDE;
    virtual QAccessibleInterface *caption() const Q_DECL_OVERRIDE;
    virtual QAccessibleInterface *summary() const Q_DECL_OVERRIDE;
    virtual QString columnDescription(int column) const Q_DECL_OVERRIDE;
    virtual QString rowDescription(int row) const Q_DECL_OVERRIDE;
    virtual int columnCount() const Q_DECL_OVERRIDE;
    virtual int rowCount() const Q_DECL_OVERRIDE;
    // selection
    virtual int selectedCellCount() const Q_DECL_OVERRIDE;
    virtual int selectedColumnCount() const Q_DECL_OVERRIDE;
    virtual int selectedRowCount() const Q_DECL_OVERRIDE;
    virtual QList<QAccessibleInterface*> selectedCells() const Q_DECL_OVERRIDE;
    virtual QList<int> selectedColumns() const Q_DECL_OVERRIDE;
    virtual QList<int> selectedRows() const Q_DECL_OVERRIDE;
    virtual bool isColumnSelected(int column) const Q_DECL_OVERRIDE;
    virtual bool isRowSelected(int row) const Q_DECL_OVERRIDE;
    virtual bool selectRow(int row) Q_DECL_OVERRIDE;
    virtual bool selectColumn(int column) Q_DECL_OVERRIDE;
    virtual bool unselectRow(int row) Q_DECL_OVERRIDE;
    virtual bool unselectColumn(int column) Q_DECL_OVERRIDE;

    // QAccessibleTableCellInterface
    virtual int columnExtent() const Q_DECL_OVERRIDE;
    virtual QList<QAccessibleInterface*> columnHeaderCells() const Q_DECL_OVERRIDE;
    virtual int columnIndex() const Q_DECL_OVERRIDE;
    virtual int rowExtent() const Q_DECL_OVERRIDE;
    virtual QList<QAccessibleInterface*> rowHeaderCells() const Q_DECL_OVERRIDE;
    virtual int rowIndex() const Q_DECL_OVERRIDE;
    virtual bool isSelected() const Q_DECL_OVERRIDE;
    virtual QAccessibleInterface* table() const Q_DECL_OVERRIDE;

    virtual void modelChange(QAccessibleTableModelChangeEvent *event) Q_DECL_OVERRIDE;
};

}

#endif
