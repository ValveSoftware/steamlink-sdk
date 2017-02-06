/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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
#include "bookmarkmanagerwidget.h"
#include "bookmarkitem.h"
#include "bookmarkmodel.h"
#include "tracer.h"
#include "xbelsupport.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QFile>
#include <QtCore/QUrl>

#include <QtGui/QCloseEvent>
#include <QtWidgets/QFileDialog>
#include <QtGui/QKeySequence>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QShortcut>

QT_BEGIN_NAMESPACE

BookmarkManagerWidget::BookmarkManagerWidget(BookmarkModel *sourceModel,
        QWidget *parent)
    : QWidget(parent)
    , bookmarkModel(sourceModel)
{
    TRACE_OBJ
    ui.setupUi(this);

    ui.treeView->setModel(bookmarkModel);

    ui.treeView->expandAll();
    ui.treeView->installEventFilter(this);
    ui.treeView->viewport()->installEventFilter(this);
    ui.treeView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui.treeView, SIGNAL(customContextMenuRequested(QPoint)), this,
        SLOT(customContextMenuRequested(QPoint)));

    connect(ui.remove, SIGNAL(clicked()), this, SLOT(removeItem()));
    connect(ui.lineEdit, SIGNAL(textChanged(QString)), this,
        SLOT(textChanged(QString)));
    new QShortcut(QKeySequence::Find, ui.lineEdit, SLOT(setFocus()));

    importExportMenu.addAction(tr("Import..."), this, SLOT(importBookmarks()));
    importExportMenu.addAction(tr("Export..."), this, SLOT(exportBookmarks()));
    ui.importExport->setMenu(&importExportMenu);

    new QShortcut(QKeySequence::FindNext, this, SLOT(findNext()));
    new QShortcut(QKeySequence::FindPrevious, this, SLOT(findPrevious()));

    connect(bookmarkModel, SIGNAL(rowsRemoved(QModelIndex,int,int)), this,
        SLOT(refeshBookmarkCache()));
    connect(bookmarkModel, SIGNAL(rowsInserted(QModelIndex,int,int)), this,
        SLOT(refeshBookmarkCache()));
    connect(bookmarkModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this,
        SLOT(refeshBookmarkCache()));

    ui.treeView->setCurrentIndex(ui.treeView->indexAt(QPoint(2, 2)));
}

BookmarkManagerWidget::~BookmarkManagerWidget()
{
    TRACE_OBJ
}

void BookmarkManagerWidget::closeEvent(QCloseEvent *event)
{
    TRACE_OBJ
    event->accept();
    emit managerWidgetAboutToClose();
}

void BookmarkManagerWidget::renameItem(const QModelIndex &index)
{
    TRACE_OBJ
    // check if we should rename the "Bookmarks Menu", bail
    if (!bookmarkModel->parent(index).isValid())
        return;

    bookmarkModel->setItemsEditable(true);
    ui.treeView->edit(index);
    bookmarkModel->setItemsEditable(false);
}

static int nextIndex(int current, int count, bool forward)
{
    TRACE_OBJ
    if (current >= 0)
        return (forward ? (current + 1) : ((current - 1) + count)) % count;
    return 0;
}

void BookmarkManagerWidget::selectNextIndex(bool direction) const
{
    QModelIndex current = ui.treeView->currentIndex();
    if (current.isValid() && !cache.isEmpty()) {
        current = cache.at(nextIndex(cache.indexOf(current), cache.count(),
            direction));
    }
    ui.treeView->setCurrentIndex(current);
}

bool BookmarkManagerWidget::eventFilter(QObject *object, QEvent *event)
{
    TRACE_OBJ
    if (object != ui.treeView && object != ui.treeView->viewport())
        return QWidget::eventFilter(object, event);

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        switch (ke->key()) {
            case Qt::Key_F2: {
                renameItem(ui.treeView->currentIndex());
            }   break;

            case Qt::Key_Delete: {
                removeItem(ui.treeView->currentIndex());
            }   break;

            default: break;
        }
    }

    if (event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *me = static_cast<QMouseEvent*>(event);
        switch (me->button()) {
            case Qt::LeftButton: {
                if (me->modifiers() & Qt::ControlModifier)
                    setSourceFromIndex(ui.treeView->currentIndex(), true);
            }   break;

            case Qt::MidButton: {
                setSourceFromIndex(ui.treeView->currentIndex(), true);
            }   break;

            default: break;
        }
    }
    return QObject::eventFilter(object, event);
}

void BookmarkManagerWidget::findNext()
{
    TRACE_OBJ
    selectNextIndex(true);
}

void BookmarkManagerWidget::findPrevious()
{
    TRACE_OBJ
    selectNextIndex(false);
}

void BookmarkManagerWidget::importBookmarks()
{
    TRACE_OBJ
    const QString &fileName = QFileDialog::getOpenFileName(0, tr("Open File"),
        QDir::currentPath(), tr("Files (*.xbel)"));

    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly)) {
        XbelReader reader(bookmarkModel);
        reader.readFromFile(&file);
    }
}

void BookmarkManagerWidget::exportBookmarks()
{
    TRACE_OBJ
    QString fileName = QFileDialog::getSaveFileName(0, tr("Save File"),
        QLatin1String("untitled.xbel"), tr("Files (*.xbel)"));

    const QLatin1String suffix(".xbel");
    if (!fileName.endsWith(suffix))
        fileName.append(suffix);

    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly)) {
        XbelWriter writer(bookmarkModel);
        writer.writeToFile(&file);
    } else {
        QMessageBox::information(this, tr("Qt Assistant"),
            tr("Unable to save bookmarks."), tr("OK"));
    }
}

void BookmarkManagerWidget::refeshBookmarkCache()
{
    TRACE_OBJ
    cache.clear();

    const QString &text = ui.lineEdit->text();
    if (!text.isEmpty())
        cache = bookmarkModel->indexListFor(text);
}

void BookmarkManagerWidget::textChanged(const QString &/*text*/)
{
    TRACE_OBJ
    refeshBookmarkCache();
    if (!cache.isEmpty())
        ui.treeView->setCurrentIndex(cache.at(0));
}

void BookmarkManagerWidget::removeItem(const QModelIndex &index)
{
    TRACE_OBJ
    QModelIndex current = index.isValid() ? index : ui.treeView->currentIndex();
    if (!current.parent().isValid() && current.row() < 2)
        return;  // check if we should delete the "Bookmarks Menu", bail

    if (bookmarkModel->hasChildren(current)) {
        int value = QMessageBox::question(this, tr("Remove"), tr("You are going"
            "to delete a Folder, this will also<br> remove it's content. Are "
            "you sure to continue?"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (value == QMessageBox::Cancel)
            return;
    }
    bookmarkModel->removeItem(current);
}

void BookmarkManagerWidget::customContextMenuRequested(const QPoint &point)
{
    TRACE_OBJ
    const QModelIndex &index = ui.treeView->indexAt(point);
    if (!index.isValid())
        return;

    // check if we should open the menu on "Bookmarks Menu", bail
    if (!bookmarkModel->parent(index).isValid())
        return;

    QAction *remove = 0;
    QAction *rename = 0;
    QAction *showItem = 0;
    QAction *showItemInNewTab = 0;

    QMenu menu(QLatin1String(""));
    if (bookmarkModel->data(index, UserRoleFolder).toBool()) {
        remove = menu.addAction(tr("Delete Folder"));
        rename = menu.addAction(tr("Rename Folder"));
    } else {
        showItem = menu.addAction(tr("Show Bookmark"));
        showItemInNewTab = menu.addAction(tr("Show Bookmark in New Tab"));
        menu.addSeparator();
        remove = menu.addAction(tr("Delete Bookmark"));
        rename = menu.addAction(tr("Rename Bookmark"));
    }

    QAction *pickedAction = menu.exec(ui.treeView->mapToGlobal(point));
    if (pickedAction == rename)
        renameItem(index);
    else if (pickedAction == remove)
        removeItem(index);
    else if (pickedAction == showItem || pickedAction == showItemInNewTab)
        setSourceFromIndex(index, pickedAction == showItemInNewTab);
}

void
BookmarkManagerWidget::setSourceFromIndex(const QModelIndex &index, bool newTab)
{
    TRACE_OBJ
    if (bookmarkModel->data(index, UserRoleFolder).toBool())
        return;

    const QVariant &data = bookmarkModel->data(index, UserRoleUrl);
    if (data.canConvert<QUrl>()) {
        if (newTab)
            emit setSourceInNewTab(data.toUrl());
        else
            emit setSource(data.toUrl());
    }
}

QT_END_NAMESPACE
