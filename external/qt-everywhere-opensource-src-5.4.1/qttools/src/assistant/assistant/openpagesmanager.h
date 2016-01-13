/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Assistant module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef OPENPAGESMANAGER_H
#define OPENPAGESMANAGER_H

#include <QtCore/QObject>

QT_BEGIN_NAMESPACE

class QAbstractItemView;
class QModelIndex;
class QUrl;

class HelpViewer;
class OpenPagesModel;
class OpenPagesSwitcher;
class OpenPagesWidget;

class OpenPagesManager : public QObject
{
    Q_OBJECT
public:
    static OpenPagesManager *createInstance(QObject *parent,
        bool defaultCollection, const QUrl &cmdLineUrl);
    static OpenPagesManager *instance();

    bool pagesOpenForNamespace(const QString &nameSpace) const;
    void closePages(const QString &nameSpace);
    void reloadPages(const QString &nameSpace);

    QAbstractItemView* openPagesWidget() const;

    int pageCount() const;
    void setCurrentPage(int index);

public slots:
    HelpViewer *createPage(const QUrl &url, bool fromSearch = false);
    HelpViewer *createNewPageFromSearch(const QUrl &url);
    HelpViewer *createPage();
    void closeCurrentPage();

    void nextPage();
    void nextPageWithSwitcher();
    void previousPage();
    void previousPageWithSwitcher();

    void closePage(HelpViewer *page);
    void setCurrentPage(HelpViewer *page);

signals:
    void aboutToAddPage();
    void pageAdded(int index);

    void pageClosed();
    void aboutToClosePage(int index);

private slots:
    void setCurrentPage(const QModelIndex &index);
    void closePage(const QModelIndex &index);
    void closePagesExcept(const QModelIndex &index);

private:
    OpenPagesManager(QObject *parent, bool defaultCollection,
        const QUrl &cmdLineUrl);
    ~OpenPagesManager();

    void setupInitialPages(bool defaultCollection, const QUrl &cmdLineUrl);
    void closeOrReloadPages(const QString &nameSpace, bool tryReload);
    void removePage(int index);

    void nextOrPreviousPage(int offset);
    void showSwitcherOrSelectPage() const;

    OpenPagesModel *m_model;
    OpenPagesWidget *m_openPagesWidget;
    OpenPagesSwitcher *m_openPagesSwitcher;

    static OpenPagesManager *m_instance;
};

QT_END_NAMESPACE

#endif // OPENPAGESMANAGER_H
