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

#include "qwebengineview.h"
#include "qwebengineview_p.h"

#include "qwebenginepage_p.h"
#include "web_contents_adapter.h"

#include <QAction>
#include <QMenu>
#include <QContextMenuEvent>
#include <QStackedLayout>

QT_BEGIN_NAMESPACE

void QWebEngineViewPrivate::bind(QWebEngineView *view, QWebEnginePage *page)
{
    if (view && page == view->d_func()->page)
        return;

    if (page) {
        // Un-bind page from its current view.
        if (QWebEngineView *oldView = page->d_func()->view) {
            page->disconnect(oldView);
            oldView->d_func()->page = 0;
        }
        page->d_func()->view = view;
        page->d_func()->adapter->reattachRWHV();
    }

    if (view) {
        // Un-bind view from its current page.
        if (QWebEnginePage *oldPage = view->d_func()->page) {
            oldPage->disconnect(view);
            oldPage->d_func()->view = 0;
            oldPage->d_func()->adapter->reattachRWHV();
            if (oldPage->parent() == view)
                delete oldPage;
        }
        view->d_func()->page = page;
    }

    if (view && page) {
        QObject::connect(page, &QWebEnginePage::titleChanged, view, &QWebEngineView::titleChanged);
        QObject::connect(page, &QWebEnginePage::urlChanged, view, &QWebEngineView::urlChanged);
        QObject::connect(page, &QWebEnginePage::iconUrlChanged, view, &QWebEngineView::iconUrlChanged);
        QObject::connect(page, &QWebEnginePage::loadStarted, view, &QWebEngineView::loadStarted);
        QObject::connect(page, &QWebEnginePage::loadProgress, view, &QWebEngineView::loadProgress);
        QObject::connect(page, &QWebEnginePage::loadFinished, view, &QWebEngineView::loadFinished);
        QObject::connect(page, &QWebEnginePage::selectionChanged, view, &QWebEngineView::selectionChanged);
    }
}


static QAccessibleInterface *webAccessibleFactory(const QString &, QObject *object)
{
    if (QWebEngineView *v = qobject_cast<QWebEngineView*>(object))
        return new QWebEngineViewAccessible(v);
    return Q_NULLPTR;
}

QWebEngineViewPrivate::QWebEngineViewPrivate()
    : page(0)
    , m_pendingContextMenuEvent(false)
{
    QAccessible::installFactory(&webAccessibleFactory);
}

QWebEngineView::QWebEngineView(QWidget *parent)
    : QWidget(parent)
    , d_ptr(new QWebEngineViewPrivate)
{
    Q_D(QWebEngineView);
    d->q_ptr = this;

    // This causes the child RenderWidgetHostViewQtDelegateWidgets to fill this widget.
    setLayout(new QStackedLayout);
}

QWebEngineView::~QWebEngineView()
{
    Q_D(QWebEngineView);
    QWebEngineViewPrivate::bind(0, d->page);
}

QWebEnginePage* QWebEngineView::page() const
{
    Q_D(const QWebEngineView);
    if (!d->page) {
        QWebEngineView *that = const_cast<QWebEngineView*>(this);
        that->setPage(new QWebEnginePage(that));
    }
    return d->page;
}

void QWebEngineView::setPage(QWebEnginePage* page)
{
    QWebEngineViewPrivate::bind(this, page);
}

void QWebEngineView::load(const QUrl& url)
{
    page()->load(url);
}

void QWebEngineView::setHtml(const QString& html, const QUrl& baseUrl)
{
    page()->setHtml(html, baseUrl);
}

void QWebEngineView::setContent(const QByteArray& data, const QString& mimeType, const QUrl& baseUrl)
{
    page()->setContent(data, mimeType, baseUrl);
}

QWebEngineHistory* QWebEngineView::history() const
{
    return page()->history();
}

QString QWebEngineView::title() const
{
    return page()->title();
}

void QWebEngineView::setUrl(const QUrl &url)
{
    page()->setUrl(url);
}

QUrl QWebEngineView::url() const
{
    return page()->url();
}

QUrl QWebEngineView::iconUrl() const
{
    return page()->iconUrl();
}

bool QWebEngineView::hasSelection() const
{
    return page()->hasSelection();
}

QString QWebEngineView::selectedText() const
{
    return page()->selectedText();
}

#ifndef QT_NO_ACTION
QAction* QWebEngineView::pageAction(QWebEnginePage::WebAction action) const
{
    return page()->action(action);
}
#endif

void QWebEngineView::triggerPageAction(QWebEnginePage::WebAction action, bool checked)
{
    page()->triggerAction(action, checked);
}

void QWebEngineView::findText(const QString &subString, QWebEnginePage::FindFlags options, const QWebEngineCallback<bool> &resultCallback)
{
    page()->findText(subString, options, resultCallback);
}

/*!
 * \reimp
 */
QSize QWebEngineView::sizeHint() const
{
    return QSize(800, 600);
}

QWebEngineSettings *QWebEngineView::settings() const
{
    return page()->settings();
}

void QWebEngineView::stop()
{
    page()->triggerAction(QWebEnginePage::Stop);
}

void QWebEngineView::back()
{
    page()->triggerAction(QWebEnginePage::Back);
}

void QWebEngineView::forward()
{
    page()->triggerAction(QWebEnginePage::Forward);
}

void QWebEngineView::reload()
{
    page()->triggerAction(QWebEnginePage::Reload);
}

QWebEngineView *QWebEngineView::createWindow(QWebEnginePage::WebWindowType type)
{
    Q_UNUSED(type)
    return 0;
}


qreal QWebEngineView::zoomFactor() const
{
    return page()->zoomFactor();
}

void QWebEngineView::setZoomFactor(qreal factor)
{
    page()->setZoomFactor(factor);
}

/*!
 * \reimp
 */
bool QWebEngineView::event(QEvent *ev)
{
    Q_D(QWebEngineView);
    // We swallow spontaneous contextMenu events and synthethize those back later on when we get the
    // HandleContextMenu callback from chromium
    if (ev->type() == QEvent::ContextMenu) {
        ev->accept();
        d->m_pendingContextMenuEvent = true;
        return true;
    }
    return QWidget::event(ev);
}

/*!
 * \reimp
 */
void QWebEngineView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = page()->createStandardContextMenu();
    menu->popup(event->globalPos());
}

int QWebEngineViewAccessible::childCount() const
{
    if (view() && child(0))
        return 1;
    return 0;
}

QAccessibleInterface *QWebEngineViewAccessible::child(int index) const
{
    if (index == 0 && view() && view()->page())
        return view()->page()->d_func()->adapter->browserAccessible();
    return Q_NULLPTR;
}

int QWebEngineViewAccessible::indexOfChild(const QAccessibleInterface *c) const
{
    if (c == child(0))
        return 0;
    return -1;
}

QT_END_NAMESPACE

#include "moc_qwebengineview.cpp"
