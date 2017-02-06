/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Assistant module of the Qt Toolkit.
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

#include "openpageswidget.h"

#include "centralwidget.h"
#include "openpagesmodel.h"
#include "tracer.h"

#include <QtWidgets/QApplication>
#include <QtWidgets/QHeaderView>
#include <QtGui/QKeyEvent>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QMenu>
#include <QtGui/QPainter>

QT_BEGIN_NAMESPACE

OpenPagesDelegate::OpenPagesDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
    TRACE_OBJ
}

void OpenPagesDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
           const QModelIndex &index) const
{
    TRACE_OBJ
    if (option.state & QStyle::State_MouseOver) {
        if ((QApplication::mouseButtons() & Qt::LeftButton) == 0)
            pressedIndex = QModelIndex();
        QBrush brush = option.palette.alternateBase();
        if (index == pressedIndex)
            brush = option.palette.dark();
        painter->fillRect(option.rect, brush);
    }

    QStyledItemDelegate::paint(painter, option, index);

    if (index.column() == 1 && index.model()->rowCount() > 1
        && option.state & QStyle::State_MouseOver) {
        QIcon icon((option.state & QStyle::State_Selected)
            ? ":/qt-project.org/assistant/images/closebutton.png"
            : ":/qt-project.org/assistant/images/darkclosebutton.png");

        const QRect iconRect(option.rect.right() - option.rect.height(),
            option.rect.top(), option.rect.height(), option.rect.height());
        icon.paint(painter, iconRect, Qt::AlignRight | Qt::AlignVCenter);
    }
}

// -- OpenPagesWidget

OpenPagesWidget::OpenPagesWidget(OpenPagesModel *model)
    : m_allowContextMenu(true)
{
    TRACE_OBJ
    setModel(model);
    setIndentation(0);
    setItemDelegate((m_delegate = new OpenPagesDelegate(this)));

    setTextElideMode(Qt::ElideMiddle);
    setAttribute(Qt::WA_MacShowFocusRect, false);

    viewport()->setAttribute(Qt::WA_Hover);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);

    header()->hide();
    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(0, QHeaderView::Stretch);
    header()->setSectionResizeMode(1, QHeaderView::Fixed);
    header()->resizeSection(1, 18);

    installEventFilter(this);
    setUniformRowHeights(true);
    setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this, SIGNAL(clicked(QModelIndex)), this,
        SLOT(handleClicked(QModelIndex)));
    connect(this, SIGNAL(pressed(QModelIndex)), this,
        SLOT(handlePressed(QModelIndex)));
    connect(this, SIGNAL(customContextMenuRequested(QPoint)), this,
        SLOT(contextMenuRequested(QPoint)));
}

OpenPagesWidget::~OpenPagesWidget()
{
    TRACE_OBJ
}

void OpenPagesWidget::selectCurrentPage()
{
    TRACE_OBJ
    const QModelIndex &current =
        model()->index(CentralWidget::instance()->currentIndex(), 0);

    QItemSelectionModel * const selModel = selectionModel();
    selModel->select(current,
        QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    selModel->clearSelection();

    setCurrentIndex(current);
    scrollTo(currentIndex());
}

void OpenPagesWidget::allowContextMenu(bool ok)
{
    TRACE_OBJ
    m_allowContextMenu = ok;
}

void OpenPagesWidget::contextMenuRequested(QPoint pos)
{
    TRACE_OBJ
    QModelIndex index = indexAt(pos);
    if (!index.isValid() || !m_allowContextMenu)
        return;

    if (index.column() == 1)
        index = index.sibling(index.row(), 0);
    QMenu contextMenu;
    QAction *closeEditor = contextMenu.addAction(tr("Close %1").arg(index.data()
        .toString()));
    QAction *closeOtherEditors = contextMenu.addAction(tr("Close All Except %1")
        .arg(index.data().toString()));

    if (model()->rowCount() == 1) {
        closeEditor->setEnabled(false);
        closeOtherEditors->setEnabled(false);
    }

    QAction *action = contextMenu.exec(mapToGlobal(pos));
    if (action == closeEditor)
        emit closePage(index);
    else if (action == closeOtherEditors)
        emit closePagesExcept(index);
}

void OpenPagesWidget::handlePressed(const QModelIndex &index)
{
    TRACE_OBJ
    if (index.column() == 0)
        emit setCurrentPage(index);

    if (index.column() == 1)
        m_delegate->pressedIndex = index;
}

void OpenPagesWidget::handleClicked(const QModelIndex &index)
{
    TRACE_OBJ
    // implemented here to handle the funky close button and to work around a
    // bug in item views where the delegate wouldn't get the QStyle::State_MouseOver
    if (index.column() == 1) {
        if (model()->rowCount() > 1)
            emit closePage(index);

        QWidget *vp = viewport();
        const QPoint &cursorPos = QCursor::pos();
        QMouseEvent e(QEvent::MouseMove, vp->mapFromGlobal(cursorPos), cursorPos,
            Qt::NoButton, 0, 0);
        QCoreApplication::sendEvent(vp, &e);
    }
}

bool OpenPagesWidget::eventFilter(QObject *obj, QEvent *event)
{
    TRACE_OBJ
    if (obj != this)
        return QWidget::eventFilter(obj, event);

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if (currentIndex().isValid() && ke->modifiers() == 0) {
            const int key = ke->key();
            if (key == Qt::Key_Return || key == Qt::Key_Enter
                || key == Qt::Key_Space) {
                emit setCurrentPage(currentIndex());
            } else if ((key == Qt::Key_Delete || key == Qt::Key_Backspace)
                && model()->rowCount() > 1) {
                emit closePage(currentIndex());
            }
        }
    } else if (event->type() == QEvent::KeyRelease) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if (ke->modifiers() == 0
            && (ke->key() == Qt::Key_Up || ke->key() == Qt::Key_Down)) {
                emit setCurrentPage(currentIndex());
        }
    }
    return QWidget::eventFilter(obj, event);
}

QT_END_NAMESPACE
