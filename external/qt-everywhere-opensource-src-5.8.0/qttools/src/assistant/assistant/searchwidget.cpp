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
#include "tracer.h"

#include "mainwindow.h"
#include "searchwidget.h"

#include <QtCore/QMap>
#include <QtCore/QMimeData>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <QtWidgets/QMenu>
#include <QtWidgets/QLayout>
#include <QtGui/QKeyEvent>
#ifndef QT_NO_CLIPBOARD
#include <QtGui/QClipboard>
#endif
#include <QtWidgets/QApplication>
#include <QtWidgets/QTextBrowser>

#include <QtHelp/QHelpSearchEngine>
#include <QtHelp/QHelpSearchQueryWidget>
#include <QtHelp/QHelpSearchResultWidget>

QT_BEGIN_NAMESPACE

SearchWidget::SearchWidget(QHelpSearchEngine *engine, QWidget *parent)
    : QWidget(parent)
    , zoomCount(0)
    , searchEngine(engine)
{
    TRACE_OBJ
    QVBoxLayout *vLayout = new QVBoxLayout(this);

    resultWidget = searchEngine->resultWidget();
    QHelpSearchQueryWidget *queryWidget = searchEngine->queryWidget();

    vLayout->addWidget(queryWidget);
    vLayout->addWidget(resultWidget);

    setFocusProxy(queryWidget);

    connect(queryWidget, SIGNAL(search()), this, SLOT(search()));
    connect(resultWidget, SIGNAL(requestShowLink(QUrl)), this,
        SIGNAL(requestShowLink(QUrl)));

    connect(searchEngine, SIGNAL(searchingStarted()), this,
        SLOT(searchingStarted()));
    connect(searchEngine, SIGNAL(searchingFinished(int)), this,
        SLOT(searchingFinished(int)));

    QTextBrowser* browser = resultWidget->findChild<QTextBrowser*>();
    if (browser) // Will be null if lib was configured not to use CLucene.
        browser->viewport()->installEventFilter(this);
}

SearchWidget::~SearchWidget()
{
    TRACE_OBJ
    // nothing todo
}

void SearchWidget::zoomIn()
{
    TRACE_OBJ
    QTextBrowser* browser = resultWidget->findChild<QTextBrowser*>();
    if (browser && zoomCount != 10) {
        zoomCount++;
        browser->zoomIn();
    }
}

void SearchWidget::zoomOut()
{
    TRACE_OBJ
    QTextBrowser* browser = resultWidget->findChild<QTextBrowser*>();
    if (browser && zoomCount != -5) {
        zoomCount--;
        browser->zoomOut();
    }
}

void SearchWidget::resetZoom()
{
    TRACE_OBJ
    if (zoomCount == 0)
        return;

    QTextBrowser* browser = resultWidget->findChild<QTextBrowser*>();
    if (browser) {
        browser->zoomOut(zoomCount);
        zoomCount = 0;
    }
}

void SearchWidget::search() const
{
    TRACE_OBJ
    QList<QHelpSearchQuery> query = searchEngine->queryWidget()->query();
    searchEngine->search(query);
}

void SearchWidget::searchingStarted()
{
    TRACE_OBJ
    qApp->setOverrideCursor(QCursor(Qt::WaitCursor));
}

void SearchWidget::searchingFinished(int hits)
{
    TRACE_OBJ
    Q_UNUSED(hits)
    qApp->restoreOverrideCursor();
}

bool SearchWidget::eventFilter(QObject* o, QEvent *e)
{
    TRACE_OBJ
    QTextBrowser* browser = resultWidget->findChild<QTextBrowser*>();
    if (browser && o == browser->viewport()
        && e->type() == QEvent::MouseButtonRelease){
        QMouseEvent *me = static_cast<QMouseEvent*>(e);
        QUrl link = resultWidget->linkAt(me->pos());
        if (!link.isEmpty() || link.isValid()) {
            bool controlPressed = me->modifiers() & Qt::ControlModifier;
            if((me->button() == Qt::LeftButton && controlPressed)
                || (me->button() == Qt::MidButton)) {
                    emit requestShowLinkInNewTab(link);
            }
        }
    }
    return QWidget::eventFilter(o,e);
}

void SearchWidget::keyPressEvent(QKeyEvent *keyEvent)
{
    TRACE_OBJ
    if (keyEvent->key() == Qt::Key_Escape)
        MainWindow::activateCurrentBrowser();
    else
        keyEvent->ignore();
}

void SearchWidget::contextMenuEvent(QContextMenuEvent *contextMenuEvent)
{
    TRACE_OBJ
    QMenu menu;
    QPoint point = contextMenuEvent->globalPos();

    QTextBrowser* browser = resultWidget->findChild<QTextBrowser*>();
    if (!browser)
        return;

    point = browser->mapFromGlobal(point);
    if (!browser->rect().contains(point, true))
        return;

    QUrl link = browser->anchorAt(point);

    QKeySequence keySeq;
#ifndef QT_NO_CLIPBOARD
    keySeq = QKeySequence::Copy;
    QAction *copyAction = menu.addAction(tr("&Copy") + QLatin1String("\t") +
        keySeq.toString(QKeySequence::NativeText));
    copyAction->setEnabled(QTextCursor(browser->textCursor()).hasSelection());
#endif

    QAction *copyAnchorAction = menu.addAction(tr("Copy &Link Location"));
    copyAnchorAction->setEnabled(!link.isEmpty() && link.isValid());

    keySeq = QKeySequence(Qt::CTRL);
    QAction *newTabAction = menu.addAction(tr("Open Link in New Tab") +
        QLatin1String("\t") + keySeq.toString(QKeySequence::NativeText) +
        QLatin1String("LMB"));
    newTabAction->setEnabled(!link.isEmpty() && link.isValid());

    menu.addSeparator();

    keySeq = QKeySequence::SelectAll;
    QAction *selectAllAction = menu.addAction(tr("Select All") +
        QLatin1String("\t") + keySeq.toString(QKeySequence::NativeText));

    QAction *usedAction = menu.exec(mapToGlobal(contextMenuEvent->pos()));
#ifndef QT_NO_CLIPBOARD
    if (usedAction == copyAction) {
        QTextCursor cursor = browser->textCursor();
        if (!cursor.isNull() && cursor.hasSelection()) {
            QString selectedText = cursor.selectedText();
            QMimeData *data = new QMimeData();
            data->setText(selectedText);
            QApplication::clipboard()->setMimeData(data);
        }
    }
    else if (usedAction == copyAnchorAction) {
        QApplication::clipboard()->setText(link.toString());
    }
    else
#endif
        if (usedAction == newTabAction) {
        emit requestShowLinkInNewTab(link);
    }
    else if (usedAction == selectAllAction) {
        browser->selectAll();
    }
}

QT_END_NAMESPACE
