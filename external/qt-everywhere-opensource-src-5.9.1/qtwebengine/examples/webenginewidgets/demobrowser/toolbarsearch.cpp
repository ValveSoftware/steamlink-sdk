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

#include "toolbarsearch.h"
#include "autosaver.h"
#include "browserapplication.h"

#include <QtCore/QSettings>
#include <QtCore/QUrl>
#include <QtCore/QUrlQuery>

#include <QtWidgets/QCompleter>
#include <QtWidgets/QMenu>
#include <QtCore/QStringListModel>

#include <QWebEngineSettings>

/*
    ToolbarSearch is a very basic search widget that also contains a small history.
    Searches are turned into urls that use Google to perform search
 */
ToolbarSearch::ToolbarSearch(QWidget *parent)
    : SearchLineEdit(parent)
    , m_autosaver(new AutoSaver(this))
    , m_maxSavedSearches(10)
    , m_stringListModel(new QStringListModel(this))
{
    QMenu *m = menu();
    connect(m, SIGNAL(aboutToShow()), this, SLOT(aboutToShowMenu()));
    connect(m, SIGNAL(triggered(QAction*)), this, SLOT(triggeredMenuAction(QAction*)));

    QCompleter *completer = new QCompleter(m_stringListModel, this);
    completer->setCompletionMode(QCompleter::InlineCompletion);
    lineEdit()->setCompleter(completer);

    connect(lineEdit(), SIGNAL(returnPressed()), SLOT(searchNow()));
    setInactiveText(tr("Google"));
    load();
}

ToolbarSearch::~ToolbarSearch()
{
    m_autosaver->saveIfNeccessary();
}

void ToolbarSearch::save()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("toolbarsearch"));
    settings.setValue(QLatin1String("recentSearches"), m_stringListModel->stringList());
    settings.setValue(QLatin1String("maximumSaved"), m_maxSavedSearches);
    settings.endGroup();
}

void ToolbarSearch::load()
{
    QSettings settings;
    settings.beginGroup(QLatin1String("toolbarsearch"));
    QStringList list = settings.value(QLatin1String("recentSearches")).toStringList();
    m_maxSavedSearches = settings.value(QLatin1String("maximumSaved"), m_maxSavedSearches).toInt();
    m_stringListModel->setStringList(list);
    settings.endGroup();
}

void ToolbarSearch::searchNow()
{
    QString searchText = lineEdit()->text();
    QStringList newList = m_stringListModel->stringList();
    if (newList.contains(searchText))
        newList.removeAt(newList.indexOf(searchText));
    newList.prepend(searchText);
    if (newList.size() >= m_maxSavedSearches)
        newList.removeLast();

    if (!BrowserApplication::instance()->privateBrowsing()) {
        m_stringListModel->setStringList(newList);
        m_autosaver->changeOccurred();
    }

    QUrl url(QLatin1String("http://www.google.com/search"));
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QLatin1String("q"), searchText);
    urlQuery.addQueryItem(QLatin1String("ie"), QLatin1String("UTF-8"));
    urlQuery.addQueryItem(QLatin1String("oe"), QLatin1String("UTF-8"));
    urlQuery.addQueryItem(QLatin1String("client"), QLatin1String("qtdemobrowser"));
    url.setQuery(urlQuery);
    emit search(url);
}

void ToolbarSearch::aboutToShowMenu()
{
    lineEdit()->selectAll();
    QMenu *m = menu();
    m->clear();
    QStringList list = m_stringListModel->stringList();
    if (list.isEmpty()) {
        m->addAction(tr("No Recent Searches"));
        return;
    }

    QAction *recent = m->addAction(tr("Recent Searches"));
    recent->setEnabled(false);
    for (int i = 0; i < list.count(); ++i) {
        QString text = list.at(i);
        m->addAction(text)->setData(text);
    }
    m->addSeparator();
    m->addAction(tr("Clear Recent Searches"), this, SLOT(clear()));
}

void ToolbarSearch::triggeredMenuAction(QAction *action)
{
    QVariant v = action->data();
    if (v.canConvert<QString>()) {
        QString text = v.toString();
        lineEdit()->setText(text);
        searchNow();
    }
}

void ToolbarSearch::clear()
{
    m_stringListModel->setStringList(QStringList());
    m_autosaver->changeOccurred();;
}
