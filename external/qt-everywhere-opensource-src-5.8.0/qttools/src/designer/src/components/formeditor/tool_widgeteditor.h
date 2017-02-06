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

#ifndef TOOL_WIDGETEDITOR_H
#define TOOL_WIDGETEDITOR_H

#include <QtDesigner/QDesignerFormWindowToolInterface>

#include <QtGui/qevent.h>
#include <QtCore/QPointer>

QT_BEGIN_NAMESPACE

class QAction;
class QMainWindow;

namespace qdesigner_internal {

class FormWindow;
class QDesignerMimeData;

class WidgetEditorTool: public QDesignerFormWindowToolInterface
{
    Q_OBJECT
public:
    explicit WidgetEditorTool(FormWindow *formWindow);
    virtual ~WidgetEditorTool();

    QDesignerFormEditorInterface *core() const Q_DECL_OVERRIDE;
    QDesignerFormWindowInterface *formWindow() const Q_DECL_OVERRIDE;
    QWidget *editor() const Q_DECL_OVERRIDE;
    QAction *action() const Q_DECL_OVERRIDE;

    void activated() Q_DECL_OVERRIDE;
    void deactivated() Q_DECL_OVERRIDE;

    bool handleEvent(QWidget *widget, QWidget *managedWidget, QEvent *event) Q_DECL_OVERRIDE;

    bool handleContextMenu(QWidget *widget, QWidget *managedWidget, QContextMenuEvent *e);
    bool handleMouseButtonDblClickEvent(QWidget *widget, QWidget *managedWidget, QMouseEvent *e);
    bool handleMousePressEvent(QWidget *widget, QWidget *managedWidget, QMouseEvent *e);
    bool handleMouseMoveEvent(QWidget *widget, QWidget *managedWidget, QMouseEvent *e);
    bool handleMouseReleaseEvent(QWidget *widget, QWidget *managedWidget, QMouseEvent *e);
    bool handleKeyPressEvent(QWidget *widget, QWidget *managedWidget, QKeyEvent *e);
    bool handleKeyReleaseEvent(QWidget *widget, QWidget *managedWidget, QKeyEvent *e);
    bool handlePaintEvent(QWidget *widget, QWidget *managedWidget, QPaintEvent *e);

    bool handleDragEnterMoveEvent(QWidget *widget, QWidget *managedWidget, QDragMoveEvent *e, bool isEnter);
    bool handleDragLeaveEvent(QWidget *widget, QWidget *managedWidget, QDragLeaveEvent *e);
    bool handleDropEvent(QWidget *widget, QWidget *managedWidget, QDropEvent *e);

private:
    bool restoreDropHighlighting();
    void detectDockDrag(const QDesignerMimeData *mimeData);

    FormWindow *m_formWindow;
    QAction *m_action;

    bool mainWindowSeparatorEvent(QWidget *widget, QEvent *event);
    QPointer<QMainWindow> m_separator_drag_mw;
    QPointer<QWidget> m_lastDropTarget;
    bool m_specialDockDrag;
};

}  // namespace qdesigner_internal

QT_END_NAMESPACE

#endif // TOOL_WIDGETEDITOR_H
