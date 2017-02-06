/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "tabwidget.h"

#include "browserapplication.h"
#include "browsermainwindow.h"
#include "downloadmanager.h"
#include "fullscreennotification.h"
#include "history.h"
#include "savepagedialog.h"
#include "urllineedit.h"
#include "webview.h"

#include <QWebEngineDownloadItem>
#include <QWebEngineProfile>
#include <QWebEngineFullScreenRequest>
#include <QtCore/QMimeData>
#include <QtGui/QClipboard>
#include <QtWidgets/QCompleter>
#include <QtWidgets/QListView>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMessageBox>
#include <QtGui/QDrag>
#include <QtGui/QMouseEvent>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStyle>
#include <QtWidgets/QToolButton>

#include <QtCore/QDebug>

TabBar::TabBar(QWidget *parent)
    : QTabBar(parent)
{
    setContextMenuPolicy(Qt::CustomContextMenu);
    setAcceptDrops(true);
    connect(this, SIGNAL(customContextMenuRequested(QPoint)),
            this, SLOT(contextMenuRequested(QPoint)));

    QString ctrl = QLatin1String("Ctrl+%1");
    for (int i = 1; i < 10; ++i) {
        QShortcut *shortCut = new QShortcut(ctrl.arg(i), this);
        m_tabShortcuts.append(shortCut);
        connect(shortCut, SIGNAL(activated()), this, SLOT(selectTabAction()));
    }
    setTabsClosable(true);
    connect(this, SIGNAL(tabCloseRequested(int)),
            this, SIGNAL(closeTab(int)));
    setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    setMovable(true);
}

TabWidget::~TabWidget()
{
    delete m_fullScreenNotification;
    delete m_fullScreenView;
}

void TabBar::selectTabAction()
{
    if (QShortcut *shortCut = qobject_cast<QShortcut*>(sender())) {
        int index = m_tabShortcuts.indexOf(shortCut);
        setCurrentIndex(index);
    }
}

void TabBar::contextMenuRequested(const QPoint &position)
{
    QMenu menu;
    menu.addAction(tr("New &Tab"), this, SIGNAL(newTab()), QKeySequence::AddTab);
    int index = tabAt(position);
    if (-1 != index) {
        QAction *action = menu.addAction(tr("Clone Tab"),
                this, SLOT(cloneTab()));
        action->setData(index);

        menu.addSeparator();

        action = menu.addAction(tr("&Close Tab"),
                this, SLOT(closeTab()), QKeySequence::Close);
        action->setData(index);

        action = menu.addAction(tr("Close &Other Tabs"),
                this, SLOT(closeOtherTabs()));
        action->setData(index);

        menu.addSeparator();

        action = menu.addAction(tr("Reload Tab"),
                this, SLOT(reloadTab()), QKeySequence::Refresh);
        action->setData(index);

        // Audio mute / unmute.
        action = menu.addAction(tr("Mute tab"),
                this, SLOT(muteTab()));
        action->setData(index);

        action = menu.addAction(tr("Unmute tab"),
                this, SLOT(unmuteTab()));
        action->setData(index);
    } else {
        menu.addSeparator();
    }
    menu.addAction(tr("Reload All Tabs"), this, SIGNAL(reloadAllTabs()));
    menu.exec(QCursor::pos());
}

void TabBar::cloneTab()
{
    if (QAction *action = qobject_cast<QAction*>(sender())) {
        int index = action->data().toInt();
        emit cloneTab(index);
    }
}

void TabBar::closeTab()
{
    if (QAction *action = qobject_cast<QAction*>(sender())) {
        int index = action->data().toInt();
        emit closeTab(index);
    }
}

void TabBar::closeOtherTabs()
{
    if (QAction *action = qobject_cast<QAction*>(sender())) {
        int index = action->data().toInt();
        emit closeOtherTabs(index);
    }
}

void TabBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        m_dragStartPos = event->pos();

    QTabBar::mousePressEvent(event);

    // Middle click on tab should close it.
    if (event->button() == Qt::MiddleButton) {
        const QPoint pos = event->pos();
        int index = tabAt(pos);
        if (index != -1) {
            emit closeTab(index);
        }
    }
}

void TabBar::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() == Qt::LeftButton) {
        int diffX = event->pos().x() - m_dragStartPos.x();
        int diffY = event->pos().y() - m_dragStartPos.y();
        if ((event->pos() - m_dragStartPos).manhattanLength() > QApplication::startDragDistance()
            && diffX < 3 && diffX > -3
            && diffY < -10) {
            QDrag *drag = new QDrag(this);
            QMimeData *mimeData = new QMimeData;
            QList<QUrl> urls;
            int index = tabAt(event->pos());
            QUrl url = tabData(index).toUrl();
            urls.append(url);
            mimeData->setUrls(urls);
            mimeData->setText(tabText(index));
            mimeData->setData(QLatin1String("action"), "tab-reordering");
            drag->setMimeData(mimeData);
            drag->exec();
        }
    }
    QTabBar::mouseMoveEvent(event);
}

// When index is -1 index chooses the current tab
void TabWidget::reloadTab(int index)
{
    if (index < 0)
        index = currentIndex();
    if (index < 0 || index >= count())
        return;

    QWidget *widget = this->widget(index);
    if (WebView *tab = qobject_cast<WebView*>(widget))
        tab->reload();
}

void TabBar::reloadTab()
{
    if (QAction *action = qobject_cast<QAction*>(sender())) {
        int index = action->data().toInt();
        emit reloadTab(index);
    }
}

void TabBar::muteTab()
{
    if (QAction *action = qobject_cast<QAction*>(sender())) {
        int index = action->data().toInt();
        emit muteTab(index, true);
    }
}

void TabBar::unmuteTab()
{
    if (QAction *action = qobject_cast<QAction*>(sender())) {
        int index = action->data().toInt();
        emit muteTab(index, false);
    }
}

TabWidget::TabWidget(QWidget *parent)
    : QTabWidget(parent)
    , m_recentlyClosedTabsAction(0)
    , m_newTabAction(0)
    , m_closeTabAction(0)
    , m_nextTabAction(0)
    , m_previousTabAction(0)
    , m_recentlyClosedTabsMenu(0)
    , m_lineEditCompleter(0)
    , m_lineEdits(0)
    , m_tabBar(new TabBar(this))
    , m_profile(QWebEngineProfile::defaultProfile())
    , m_fullScreenView(0)
    , m_fullScreenNotification(0)
{
    setElideMode(Qt::ElideRight);

    connect(m_tabBar, SIGNAL(newTab()), this, SLOT(newTab()));
    connect(m_tabBar, SIGNAL(closeTab(int)), this, SLOT(requestCloseTab(int)));
    connect(m_tabBar, SIGNAL(cloneTab(int)), this, SLOT(cloneTab(int)));
    connect(m_tabBar, SIGNAL(closeOtherTabs(int)), this, SLOT(closeOtherTabs(int)));
    connect(m_tabBar, SIGNAL(reloadTab(int)), this, SLOT(reloadTab(int)));
    connect(m_tabBar, SIGNAL(reloadAllTabs()), this, SLOT(reloadAllTabs()));
    connect(m_tabBar, SIGNAL(tabMoved(int,int)), this, SLOT(moveTab(int,int)));
    connect(m_tabBar, SIGNAL(tabBarDoubleClicked(int)), this, SLOT(handleTabBarDoubleClicked(int)));
    connect(m_tabBar, SIGNAL(muteTab(int,bool)), this, SLOT(setAudioMutedForTab(int,bool)));
    setTabBar(m_tabBar);
    setDocumentMode(true);

    // Actions
    m_newTabAction = new QAction(QIcon(QLatin1String(":addtab.png")), tr("New &Tab"), this);
    m_newTabAction->setShortcuts(QKeySequence::AddTab);
    m_newTabAction->setIconVisibleInMenu(false);
    connect(m_newTabAction, SIGNAL(triggered()), this, SLOT(newTab()));

    m_closeTabAction = new QAction(QIcon(QLatin1String(":closetab.png")), tr("&Close Tab"), this);
    m_closeTabAction->setShortcuts(QKeySequence::Close);
    m_closeTabAction->setIconVisibleInMenu(false);
    connect(m_closeTabAction, SIGNAL(triggered()), this, SLOT(requestCloseTab()));

    m_nextTabAction = new QAction(tr("Show Next Tab"), this);
    QList<QKeySequence> shortcuts;
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_BraceRight));
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_PageDown));
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_BracketRight));
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_Less));
    m_nextTabAction->setShortcuts(shortcuts);
    connect(m_nextTabAction, SIGNAL(triggered()), this, SLOT(nextTab()));

    m_previousTabAction = new QAction(tr("Show Previous Tab"), this);
    shortcuts.clear();
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_BraceLeft));
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_PageUp));
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_BracketLeft));
    shortcuts.append(QKeySequence(Qt::CTRL | Qt::Key_Greater));
    m_previousTabAction->setShortcuts(shortcuts);
    connect(m_previousTabAction, SIGNAL(triggered()), this, SLOT(previousTab()));

    m_recentlyClosedTabsMenu = new QMenu(this);
    connect(m_recentlyClosedTabsMenu, SIGNAL(aboutToShow()),
            this, SLOT(aboutToShowRecentTabsMenu()));
    connect(m_recentlyClosedTabsMenu, SIGNAL(triggered(QAction*)),
            this, SLOT(aboutToShowRecentTriggeredAction(QAction*)));
    m_recentlyClosedTabsAction = new QAction(tr("Recently Closed Tabs"), this);
    m_recentlyClosedTabsAction->setMenu(m_recentlyClosedTabsMenu);
    m_recentlyClosedTabsAction->setEnabled(false);

    connect(this, SIGNAL(currentChanged(int)),
            this, SLOT(currentChanged(int)));

    m_lineEdits = new QStackedWidget(this);
}

void TabWidget::clear()
{
    // clear the recently closed tabs
    m_recentlyClosedTabs.clear();
    // clear the line edit history
    for (int i = 0; i < m_lineEdits->count(); ++i) {
        QLineEdit *qLineEdit = lineEdit(i);
        qLineEdit->setText(qLineEdit->text());
    }
}

void TabWidget::moveTab(int fromIndex, int toIndex)
{
    QWidget *lineEdit = m_lineEdits->widget(fromIndex);
    m_lineEdits->removeWidget(lineEdit);
    m_lineEdits->insertWidget(toIndex, lineEdit);
}

void TabWidget::setAudioMutedForTab(int index, bool mute)
{
    if (index < 0)
        index = currentIndex();
    if (index < 0 || index >= count())
        return;

    QWidget *widget = this->widget(index);
    if (WebView *tab = qobject_cast<WebView*>(widget))
        tab->page()->setAudioMuted(mute);
}

void TabWidget::addWebAction(QAction *action, QWebEnginePage::WebAction webAction)
{
    if (!action)
        return;
    m_actions.append(new WebActionMapper(action, webAction, this));
}

void TabWidget::currentChanged(int index)
{
    WebView *webView = this->webView(index);
    if (!webView)
        return;

    Q_ASSERT(m_lineEdits->count() == count());

    WebView *oldWebView = this->webView(m_lineEdits->currentIndex());
    if (oldWebView) {
#if defined(QWEBENGINEVIEW_STATUSBARMESSAGE)
        disconnect(oldWebView, SIGNAL(statusBarMessage(QString)),
                this, SIGNAL(showStatusBarMessage(QString)));
#endif
        disconnect(oldWebView->page(), SIGNAL(linkHovered(const QString&)),
                this, SIGNAL(linkHovered(const QString&)));
        disconnect(oldWebView, SIGNAL(loadProgress(int)),
                this, SIGNAL(loadProgress(int)));
        disconnect(oldWebView->page()->profile(), SIGNAL(downloadRequested(QWebEngineDownloadItem*)),
                this, SLOT(downloadRequested(QWebEngineDownloadItem*)));
        disconnect(oldWebView->page(), SIGNAL(fullScreenRequested(QWebEngineFullScreenRequest)),
                this, SLOT(fullScreenRequested(QWebEngineFullScreenRequest)));
    }

#if defined(QWEBENGINEVIEW_STATUSBARMESSAGE)
    connect(webView, SIGNAL(statusBarMessage(QString)),
            this, SIGNAL(showStatusBarMessage(QString)));
#endif
    connect(webView->page(), SIGNAL(linkHovered(const QString&)),
            this, SIGNAL(linkHovered(const QString&)));
    connect(webView, SIGNAL(loadProgress(int)),
            this, SIGNAL(loadProgress(int)));
    connect(webView->page()->profile(), SIGNAL(downloadRequested(QWebEngineDownloadItem*)),
            this, SLOT(downloadRequested(QWebEngineDownloadItem*)));
    connect(webView->page(), SIGNAL(fullScreenRequested(QWebEngineFullScreenRequest)),
            this, SLOT(fullScreenRequested(QWebEngineFullScreenRequest)));

    for (int i = 0; i < m_actions.count(); ++i) {
        WebActionMapper *mapper = m_actions[i];
        mapper->updateCurrent(webView->page());
    }
    emit setCurrentTitle(webView->title());
    m_lineEdits->setCurrentIndex(index);
    emit loadProgress(webView->progress());
    emit showStatusBarMessage(webView->lastStatusBarText());
    if (webView->url().isEmpty())
        m_lineEdits->currentWidget()->setFocus();
    else
        webView->setFocus();
}

void TabWidget::fullScreenRequested(QWebEngineFullScreenRequest request)
{
    WebPage *webPage = qobject_cast<WebPage*>(sender());
    if (request.toggleOn()) {
        if (!m_fullScreenView) {
            m_fullScreenView = new QWebEngineView();
            m_fullScreenNotification = new FullScreenNotification(m_fullScreenView);

            QAction *exitFullScreenAction = new QAction(m_fullScreenView);
            exitFullScreenAction->setShortcut(Qt::Key_Escape);
            connect(exitFullScreenAction, &QAction::triggered, [webPage] {
                webPage->triggerAction(QWebEnginePage::ExitFullScreen);
            });
            m_fullScreenView->addAction(exitFullScreenAction);
        }
        webPage->setView(m_fullScreenView);
        request.accept();
        m_fullScreenView->showFullScreen();
        m_fullScreenView->raise();
        m_fullScreenNotification->show();
    } else {
        if (!m_fullScreenView)
            return;
        WebView *oldWebView = this->webView(m_lineEdits->currentIndex());
        webPage->setView(oldWebView);
        request.accept();
        // Change the delete and window hide/show back to a simple m_fullScreenView->hide()
        // once QTBUG-46701 gets fixed.
        delete m_fullScreenView;
        m_fullScreenView = 0;
        window()->hide();
        window()->show();
    }
}

void TabWidget::handleTabBarDoubleClicked(int index)
{
    if (index != -1) return;
    newTab();
}

QAction *TabWidget::newTabAction() const
{
    return m_newTabAction;
}

QAction *TabWidget::closeTabAction() const
{
    return m_closeTabAction;
}

QAction *TabWidget::recentlyClosedTabsAction() const
{
    return m_recentlyClosedTabsAction;
}

QAction *TabWidget::nextTabAction() const
{
    return m_nextTabAction;
}

QAction *TabWidget::previousTabAction() const
{
    return m_previousTabAction;
}

QWidget *TabWidget::lineEditStack() const
{
    return m_lineEdits;
}

QLineEdit *TabWidget::currentLineEdit() const
{
    return lineEdit(m_lineEdits->currentIndex());
}

WebView *TabWidget::currentWebView() const
{
    return webView(currentIndex());
}

QLineEdit *TabWidget::lineEdit(int index) const
{
    UrlLineEdit *urlLineEdit = qobject_cast<UrlLineEdit*>(m_lineEdits->widget(index));
    if (urlLineEdit)
        return urlLineEdit->lineEdit();
    return 0;
}

WebView *TabWidget::webView(int index) const
{
    QWidget *widget = this->widget(index);
    if (WebView *webView = qobject_cast<WebView*>(widget)) {
        return webView;
    } else {
        // optimization to delay creating the first webview
        if (count() == 1) {
            TabWidget *that = const_cast<TabWidget*>(this);
            that->setUpdatesEnabled(false);
            that->newTab();
            that->closeTab(0);
            that->setUpdatesEnabled(true);
            return currentWebView();
        }
    }
    return 0;
}

int TabWidget::webViewIndex(WebView *webView) const
{
    int index = indexOf(webView);
    return index;
}

void TabWidget::setupPage(QWebEnginePage* page)
{
    connect(page, SIGNAL(windowCloseRequested()),
            this, SLOT(windowCloseRequested()));
    connect(page, SIGNAL(geometryChangeRequested(QRect)),
            this, SIGNAL(geometryChangeRequested(QRect)));
#if defined(QWEBENGINEPAGE_PRINTREQUESTED)
    connect(page, SIGNAL(printRequested(QWebEngineFrame*)),
            this, SIGNAL(printRequested(QWebEngineFrame*)));
#endif
#if defined(QWEBENGINEPAGE_MENUBARVISIBILITYCHANGEREQUESTED)
    connect(page, SIGNAL(menuBarVisibilityChangeRequested(bool)),
            this, SIGNAL(menuBarVisibilityChangeRequested(bool)));
#endif
#if defined(QWEBENGINEPAGE_STATUSBARVISIBILITYCHANGEREQUESTED)
    connect(page, SIGNAL(statusBarVisibilityChangeRequested(bool)),
            this, SIGNAL(statusBarVisibilityChangeRequested(bool)));
#endif
#if defined(QWEBENGINEPAGE_TOOLBARVISIBILITYCHANGEREQUESTED)
    connect(page, SIGNAL(toolBarVisibilityChangeRequested(bool)),
            this, SIGNAL(toolBarVisibilityChangeRequested(bool)));
#endif

    // webview actions
    for (int i = 0; i < m_actions.count(); ++i) {
        WebActionMapper *mapper = m_actions[i];
        mapper->addChild(page->action(mapper->webAction()));
    }
}

WebView *TabWidget::newTab(bool makeCurrent)
{
    // line edit
    UrlLineEdit *urlLineEdit = new UrlLineEdit;
    QLineEdit *lineEdit = urlLineEdit->lineEdit();
    if (!m_lineEditCompleter && count() > 0) {
        HistoryCompletionModel *completionModel = new HistoryCompletionModel(this);
        completionModel->setSourceModel(BrowserApplication::historyManager()->historyFilterModel());
        m_lineEditCompleter = new QCompleter(completionModel, this);
        // Should this be in Qt by default?
        QAbstractItemView *popup = m_lineEditCompleter->popup();
        QListView *listView = qobject_cast<QListView*>(popup);
        if (listView)
            listView->setUniformItemSizes(true);
    }
    lineEdit->setCompleter(m_lineEditCompleter);
    connect(lineEdit, SIGNAL(returnPressed()), this, SLOT(lineEditReturnPressed()));
    m_lineEdits->addWidget(urlLineEdit);
    m_lineEdits->setSizePolicy(lineEdit->sizePolicy());

    // optimization to delay creating the more expensive WebView, history, etc
    if (count() == 0) {
        QWidget *emptyWidget = new QWidget;
        QPalette p = emptyWidget->palette();
        p.setColor(QPalette::Window, palette().color(QPalette::Base));
        emptyWidget->setPalette(p);
        emptyWidget->setAutoFillBackground(true);
        disconnect(this, SIGNAL(currentChanged(int)),
            this, SLOT(currentChanged(int)));
        addTab(emptyWidget, tr("(Untitled)"));
        connect(this, SIGNAL(currentChanged(int)),
            this, SLOT(currentChanged(int)));
        return 0;
    }

    // webview
    WebView *webView = new WebView;
    webView->setPage(new WebPage(m_profile, webView));
    urlLineEdit->setWebView(webView);
    connect(webView, SIGNAL(loadStarted()),
            this, SLOT(webViewLoadStarted()));
    connect(webView, SIGNAL(iconChanged(QIcon)),
            this, SLOT(webViewIconChanged(QIcon)));
    connect(webView, SIGNAL(titleChanged(QString)),
            this, SLOT(webViewTitleChanged(QString)));
    connect(webView->page(), SIGNAL(audioMutedChanged(bool)),
            this, SLOT(webPageMutedOrAudibleChanged()));
    connect(webView->page(), SIGNAL(recentlyAudibleChanged(bool)),
            this, SLOT(webPageMutedOrAudibleChanged()));
    connect(webView, SIGNAL(urlChanged(QUrl)),
            this, SLOT(webViewUrlChanged(QUrl)));


    addTab(webView, tr("(Untitled)"));
    if (makeCurrent)
        setCurrentWidget(webView);

    setupPage(webView->page());

    if (count() == 1)
        currentChanged(currentIndex());
    emit tabsChanged();
    return webView;
}

void TabWidget::reloadAllTabs()
{
    for (int i = 0; i < count(); ++i) {
        QWidget *tabWidget = widget(i);
        if (WebView *tab = qobject_cast<WebView*>(tabWidget)) {
            tab->reload();
        }
    }
}

void TabWidget::lineEditReturnPressed()
{
    if (QLineEdit *lineEdit = qobject_cast<QLineEdit*>(sender())) {
        emit loadPage(lineEdit->text());
        if (m_lineEdits->currentWidget() == lineEdit)
            currentWebView()->setFocus();
    }
}

void TabWidget::windowCloseRequested()
{
    WebPage *webPage = qobject_cast<WebPage*>(sender());
    WebView *webView = qobject_cast<WebView*>(webPage->view());
    int index = webViewIndex(webView);
    if (index >= 0)
        closeTab(index);
}

void TabWidget::closeOtherTabs(int index)
{
    if (-1 == index)
        return;
    for (int i = count() - 1; i > index; --i)
        closeTab(i);
    for (int i = index - 1; i >= 0; --i)
        closeTab(i);
}

// When index is -1 index chooses the current tab
void TabWidget::cloneTab(int index)
{
    if (index < 0)
        index = currentIndex();
    if (index < 0 || index >= count())
        return;
    WebView *tab = newTab(false);
    tab->setUrl(webView(index)->url());
}

// When index is -1 index chooses the current tab
void TabWidget::requestCloseTab(int index)
{
    if (index < 0)
        index = currentIndex();
    if (index < 0 || index >= count())
        return;
    WebView *tab = webView(index);
    if (!tab)
        return;
    tab->page()->triggerAction(QWebEnginePage::RequestClose);
}

void TabWidget::closeTab(int index)
{
    if (index < 0 || index >= count())
        return;

    bool hasFocus = false;
    if (WebView *tab = webView(index)) {
        hasFocus = tab->hasFocus();

        if (m_profile == QWebEngineProfile::defaultProfile()) {
            m_recentlyClosedTabsAction->setEnabled(true);
            m_recentlyClosedTabs.prepend(tab->url());
            if (m_recentlyClosedTabs.size() >= TabWidget::m_recentlyClosedTabsSize)
                m_recentlyClosedTabs.removeLast();
        }
    }
    QWidget *lineEdit = m_lineEdits->widget(index);
    m_lineEdits->removeWidget(lineEdit);
    lineEdit->deleteLater();
    QWidget *webView = widget(index);
    removeTab(index);
    webView->deleteLater();
    emit tabsChanged();
    if (hasFocus && count() > 0)
        currentWebView()->setFocus();
    if (count() == 0)
        emit lastTabClosed();
}

void TabWidget::setProfile(QWebEngineProfile *profile)
{
    m_profile = profile;
    for (int i = 0; i < count(); ++i) {
        QWidget *tabWidget = widget(i);
        if (WebView *tab = qobject_cast<WebView*>(tabWidget)) {
            WebPage* webPage = new WebPage(m_profile, tab);
            setupPage(webPage);
            webPage->load(tab->page()->url());
            tab->setPage(webPage);
        }
    }
}

void TabWidget::webViewLoadStarted()
{
    WebView *webView = qobject_cast<WebView*>(sender());
    int index = webViewIndex(webView);
    if (-1 != index) {
        QIcon icon(QLatin1String(":loading.gif"));
        setTabIcon(index, icon);
    }
}

void TabWidget::webViewIconChanged(const QIcon &icon)
{
    WebView *webView = qobject_cast<WebView*>(sender());
    int index = webViewIndex(webView);
    if (-1 != index)
        setTabIcon(index, icon);
}

void TabWidget::webViewTitleChanged(const QString &title)
{
    WebView *webView = qobject_cast<WebView*>(sender());
    int index = webViewIndex(webView);
    if (-1 != index) {
        setTabText(index, title);
    }
    if (currentIndex() == index)
        emit setCurrentTitle(title);
    BrowserApplication::historyManager()->updateHistoryItem(webView->url(), title);
}

void TabWidget::webPageMutedOrAudibleChanged() {
    QWebEnginePage* webPage = qobject_cast<QWebEnginePage*>(sender());
    WebView *webView = qobject_cast<WebView*>(webPage->view());

    int index = webViewIndex(webView);
    if (-1 != index) {
        QString title = webView->title();

        bool muted = webPage->isAudioMuted();
        bool audible = webPage->recentlyAudible();
        if (muted) title += tr(" (muted)");
        else if (audible) title += tr(" (audible)");

        setTabText(index, title);
    }
}

void TabWidget::webViewUrlChanged(const QUrl &url)
{
    WebView *webView = qobject_cast<WebView*>(sender());
    int index = webViewIndex(webView);
    if (-1 != index) {
        m_tabBar->setTabData(index, url);
        HistoryManager *manager = BrowserApplication::historyManager();
        if (url.isValid())
            manager->addHistoryEntry(url.toString());
    }
    emit tabsChanged();
}

void TabWidget::aboutToShowRecentTabsMenu()
{
    m_recentlyClosedTabsMenu->clear();
    for (int i = 0; i < m_recentlyClosedTabs.count(); ++i) {
        QAction *action = new QAction(m_recentlyClosedTabsMenu);
        action->setData(m_recentlyClosedTabs.at(i));
        QIcon icon = BrowserApplication::instance()->icon(m_recentlyClosedTabs.at(i));
        action->setIcon(icon);
        action->setText(m_recentlyClosedTabs.at(i).toString());
        m_recentlyClosedTabsMenu->addAction(action);
    }
}

void TabWidget::aboutToShowRecentTriggeredAction(QAction *action)
{
    QUrl url = action->data().toUrl();
    loadUrlInCurrentTab(url);
}

void TabWidget::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (!childAt(event->pos())
            // Remove the line below when QTabWidget does not have a one pixel frame
            && event->pos().y() < (tabBar()->y() + tabBar()->height())) {
        newTab();
        return;
    }
    QTabWidget::mouseDoubleClickEvent(event);
}

void TabWidget::contextMenuEvent(QContextMenuEvent *event)
{
    if (!childAt(event->pos())) {
        m_tabBar->contextMenuRequested(event->pos());
        return;
    }
    QTabWidget::contextMenuEvent(event);
}

void TabWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::MidButton && !childAt(event->pos())
            // Remove the line below when QTabWidget does not have a one pixel frame
            && event->pos().y() < (tabBar()->y() + tabBar()->height())) {
        QUrl url(QApplication::clipboard()->text(QClipboard::Selection));
        if (!url.isEmpty() && url.isValid() && !url.scheme().isEmpty()) {
            WebView *webView = newTab();
            webView->setUrl(url);
        }
    }
}

void TabWidget::loadUrlInCurrentTab(const QUrl &url)
{
    WebView *webView = currentWebView();
    if (webView) {
        webView->loadUrl(url);
        webView->setFocus();
    }
}

void TabWidget::nextTab()
{
    int next = currentIndex() + 1;
    if (next == count())
        next = 0;
    setCurrentIndex(next);
}

void TabWidget::previousTab()
{
    int next = currentIndex() - 1;
    if (next < 0)
        next = count() - 1;
    setCurrentIndex(next);
}

static const qint32 TabWidgetMagic = 0xaa;

QByteArray TabWidget::saveState() const
{
    int version = 1;
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);

    stream << qint32(TabWidgetMagic);
    stream << qint32(version);

    QStringList tabs;
    for (int i = 0; i < count(); ++i) {
        if (WebView *tab = qobject_cast<WebView*>(widget(i))) {
            tabs.append(tab->url().toString());
        } else {
            tabs.append(QString::null);
        }
    }
    stream << tabs;
    stream << currentIndex();
    return data;
}

bool TabWidget::restoreState(const QByteArray &state)
{
    int version = 1;
    QByteArray sd = state;
    QDataStream stream(&sd, QIODevice::ReadOnly);
    if (stream.atEnd())
        return false;

    qint32 marker;
    qint32 v;
    stream >> marker;
    stream >> v;
    if (marker != TabWidgetMagic || v != version)
        return false;

    QStringList openTabs;
    stream >> openTabs;

    for (int i = 0; i < openTabs.count(); ++i) {
        if (i != 0)
            newTab();
        loadPage(openTabs.at(i));
    }

    int currentTab;
    stream >> currentTab;
    setCurrentIndex(currentTab);

    return true;
}

void TabWidget::downloadRequested(QWebEngineDownloadItem *download)
{
    if (download->savePageFormat() != QWebEngineDownloadItem::UnknownSaveFormat) {
        SavePageDialog dlg(this, download->savePageFormat(), download->path());
        if (dlg.exec() != SavePageDialog::Accepted)
            return;
        download->setSavePageFormat(dlg.pageFormat());
        download->setPath(dlg.filePath());
    }

    BrowserApplication::downloadManager()->download(download);
    download->accept();
}

WebActionMapper::WebActionMapper(QAction *root, QWebEnginePage::WebAction webAction, QObject *parent)
    : QObject(parent)
    , m_currentParent(0)
    , m_root(root)
    , m_webAction(webAction)
{
    if (!m_root)
        return;
    connect(m_root, SIGNAL(triggered()), this, SLOT(rootTriggered()));
    connect(root, SIGNAL(destroyed(QObject*)), this, SLOT(rootDestroyed()));
    root->setEnabled(false);
}

void WebActionMapper::rootDestroyed()
{
    m_root = 0;
}

void WebActionMapper::currentDestroyed()
{
    updateCurrent(0);
}

void WebActionMapper::addChild(QAction *action)
{
    if (!action)
        return;
    connect(action, SIGNAL(changed()), this, SLOT(childChanged()));
}

QWebEnginePage::WebAction WebActionMapper::webAction() const
{
    return m_webAction;
}

void WebActionMapper::rootTriggered()
{
    if (m_currentParent) {
        QAction *gotoAction = m_currentParent->action(m_webAction);
        gotoAction->trigger();
    }
}

void WebActionMapper::childChanged()
{
    if (QAction *source = qobject_cast<QAction*>(sender())) {
        if (m_root
            && m_currentParent
            && source->parent() == m_currentParent) {
            m_root->setChecked(source->isChecked());
            m_root->setEnabled(source->isEnabled());
        }
    }
}

void WebActionMapper::updateCurrent(QWebEnginePage *currentParent)
{
    if (m_currentParent)
        disconnect(m_currentParent, SIGNAL(destroyed(QObject*)),
                   this, SLOT(currentDestroyed()));

    m_currentParent = currentParent;
    if (!m_root)
        return;
    if (!m_currentParent) {
        m_root->setEnabled(false);
        m_root->setChecked(false);
        return;
    }
    QAction *source = m_currentParent->action(m_webAction);
    m_root->setChecked(source->isChecked());
    m_root->setEnabled(source->isEnabled());
    connect(m_currentParent, SIGNAL(destroyed(QObject*)),
            this, SLOT(currentDestroyed()));
}
