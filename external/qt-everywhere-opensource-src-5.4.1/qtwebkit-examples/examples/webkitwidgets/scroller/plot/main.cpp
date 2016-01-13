/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
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
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
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

#include <QApplication>
#include <QListWidget>
#include <QListWidgetItem>
#include <QSplitter>
#include <QStackedWidget>
#include <QSignalMapper>
#include <QMainWindow>
#include <QMenuBar>
#include <QActionGroup>
#include <QWebView>
#include <QTimer>
#include <QScroller>

#include <QtDebug>

#include <QGesture>

#include "settingswidget.h"
#include "plotwidget.h"


class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(bool smallscreen, bool touch)
        : QMainWindow(), m_touch(touch)
    {
        m_list = new QListWidget();
        m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        m_list_scroller = installKineticScroller(m_list);

        for (int i = 0; i < 1000; ++i)
            new QListWidgetItem(QString("This is a test text %1 %2").arg(i).arg(QString("--------").left(i % 8)), m_list);

        connect(m_list, SIGNAL(itemActivated(QListWidgetItem*)), this, SLOT(listItemActivated(QListWidgetItem*)));
        connect(m_list, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(listItemClicked(QListWidgetItem*)));
        connect(m_list, SIGNAL(itemPressed(QListWidgetItem*)), this, SLOT(listItemPressed(QListWidgetItem*)));
        connect(m_list, SIGNAL(itemSelectionChanged()), this, SLOT(listItemSelectionChanged()));
        connect(m_list, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), this, SLOT(listItemCurrentChanged(QListWidgetItem*)));

        m_web = new QWebView();
        m_web_scroller = installKineticScroller(m_web);

        QTimer::singleShot(1000, this, SLOT(loadUrl()));

        m_settings = new SettingsWidget(smallscreen);
        installKineticScroller(m_settings);
        m_plot = new PlotWidget(smallscreen);

        QStackedWidget *stack = new QStackedWidget();
        stack->addWidget(m_list);
        stack->addWidget(m_web);

        QActionGroup *pages = new QActionGroup(this);
        pages->setExclusive(true);
        QSignalMapper *mapper = new QSignalMapper(this);
        connect(mapper, SIGNAL(mapped(int)), stack, SLOT(setCurrentIndex(int)));

        createAction("List", pages, mapper, 0, true);
        createAction("Web",  pages, mapper, 1);

        if (smallscreen) {
            stack->addWidget(m_settings);
            stack->addWidget(m_plot);

            createAction("Settings", pages, mapper, 2);
            createAction("Plot",     pages, mapper, 3);

            setCentralWidget(stack);
        } else {
            QSplitter *split = new QSplitter();
            m_settings->setMinimumWidth(m_settings->sizeHint().width());
            split->addWidget(stack);
            split->addWidget(m_settings);
            split->addWidget(m_plot);
            setCentralWidget(split);
        }
        menuBar()->addMenu(QLatin1String("Pages"))->addActions(pages->actions());
        connect(stack, SIGNAL(currentChanged(int)), this, SLOT(pageChanged(int)));
        pageChanged(0);
    }

private slots:
    void pageChanged(int page)
    {
        if (page < 0 || page > 1)
            return;
        switch (page) {
        case 0:
            m_settings->setScroller(m_list);
            m_plot->setScroller(m_list);
            break;
        case 1:
            m_settings->setScroller(m_web);
            m_plot->setScroller(m_web);
            break;
        default:
            break;
        }
    }

    void loadUrl()
    {
        m_web->load(QUrl("http://www.google.com"));
    }

    void listItemActivated(QListWidgetItem *lwi)      { qWarning() << "Item ACTIVATED: " << lwi->text(); }
    void listItemClicked(QListWidgetItem *lwi)        { qWarning() << "Item CLICKED: " << lwi->text(); }
    void listItemPressed(QListWidgetItem *lwi)        { qWarning() << "Item PRESSED: " << lwi->text(); }
    void listItemCurrentChanged(QListWidgetItem *lwi) { qWarning() << "Item CURRENT: " << (lwi ? lwi->text() : QString("(none)")); }
    void listItemSelectionChanged()
    {
        int n = m_list->selectedItems().count();
        qWarning("Item%s SELECTED: %d", n == 1 ? "" : "s", n);
        foreach (QListWidgetItem *lwi, m_list->selectedItems())
            qWarning() << "    " << lwi->text();
    }

private:
    QAction *createAction(const char *text, QActionGroup *group, QSignalMapper *mapper, int mapping, bool checked = false)
    {
        QAction *a = new QAction(QLatin1String(text), group);
        a->setCheckable(true);
        a->setChecked(checked);
#if defined(Q_WS_MAC)
        a->setMenuRole(QAction::NoRole);
#endif
        mapper->setMapping(a, mapping);
        connect(a, SIGNAL(toggled(bool)), mapper, SLOT(map()));
        return a;
    }

    QScroller *installKineticScroller(QWidget *w)
    {
        if (QAbstractScrollArea *area = qobject_cast<QAbstractScrollArea *>(w)) {
            QScroller::grabGesture(area->viewport(), m_touch ? QScroller::TouchGesture : QScroller::LeftMouseButtonGesture);
            return QScroller::scroller(area->viewport());
        } else if (QWebView *web = qobject_cast<QWebView *>(w)) {
            QScroller::grabGesture(web, m_touch ? QScroller::TouchGesture : QScroller::LeftMouseButtonGesture);
        }
        return QScroller::scroller(w);
    }

private:
    QListWidget *m_list;
    QWebView *m_web;
    QScroller *m_list_scroller, *m_web_scroller;
    SettingsWidget *m_settings;
    PlotWidget *m_plot;
    bool m_touch;
};

int main(int argc, char **argv)
{
    QApplication a(argc, argv);

#if defined(Q_WS_MAEMO_5) || defined(Q_WS_S60) || defined(Q_WS_WINCE)
    bool smallscreen = true;
#else
    bool smallscreen = false;
#endif
    bool touch = false;

    if (a.arguments().contains(QLatin1String("--small")))
        smallscreen = true;
    if (a.arguments().contains(QLatin1String("--touch")))
        touch = true;

    MainWindow mw(smallscreen, touch);
    if (smallscreen)
        mw.showMaximized();
    else
        mw.show();
#if defined(Q_WS_MAC)
    mw.raise();
#endif
    return a.exec();
}

#include "main.moc"
