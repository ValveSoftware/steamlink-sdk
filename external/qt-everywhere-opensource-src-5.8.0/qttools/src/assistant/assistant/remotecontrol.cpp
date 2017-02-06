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
#include "remotecontrol.h"

#include "centralwidget.h"
#include "helpenginewrapper.h"
#include "mainwindow.h"
#include "openpagesmanager.h"
#include "tracer.h"

#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QFileSystemWatcher>
#include <QtCore/QTextStream>

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QApplication>

#include <QtHelp/QHelpEngine>
#include <QtHelp/QHelpIndexWidget>
#include <QtHelp/QHelpSearchQueryWidget>

#ifdef Q_OS_WIN
#   include "stdinlistener_win.h"
#else
#   include "stdinlistener.h"
#endif

QT_BEGIN_NAMESPACE

RemoteControl::RemoteControl(MainWindow *mainWindow)
    : QObject(mainWindow)
    , m_mainWindow(mainWindow)
    , m_debug(false)
    , m_caching(true)
    , m_syncContents(false)
    , m_expandTOC(-2)
    , helpEngine(HelpEngineWrapper::instance())

{
    TRACE_OBJ
    connect(m_mainWindow, SIGNAL(initDone()), this, SLOT(applyCache()));

    StdInListener *l = new StdInListener(this);
    connect(l, SIGNAL(receivedCommand(QString)),
        this, SLOT(handleCommandString(QString)));
    l->start();
}

void RemoteControl::handleCommandString(const QString &cmdString)
{
    TRACE_OBJ
    QStringList cmds = cmdString.split(QLatin1Char(';'));
    QStringList::const_iterator it = cmds.constBegin();
    while (it != cmds.constEnd()) {
        QString cmd, arg;
        splitInputString(*it, cmd, arg);

        if (m_debug)
            QMessageBox::information(0, tr("Debugging Remote Control"),
                tr("Received Command: %1 %2").arg(cmd).arg(arg));

        if (cmd == QLatin1String("debug"))
            handleDebugCommand(arg);
         else if (cmd == QLatin1String("show"))
            handleShowOrHideCommand(arg, true);
         else if (cmd == QLatin1String("hide"))
            handleShowOrHideCommand(arg, false);
         else if (cmd == QLatin1String("setsource"))
            handleSetSourceCommand(arg);
         else if (cmd == QLatin1String("synccontents"))
            handleSyncContentsCommand();
         else if (cmd == QLatin1String("activatekeyword"))
            handleActivateKeywordCommand(arg);
         else if (cmd == QLatin1String("activateidentifier"))
            handleActivateIdentifierCommand(arg);
         else if (cmd == QLatin1String("expandtoc"))
            handleExpandTocCommand(arg);
         else if (cmd == QLatin1String("setcurrentfilter"))
            handleSetCurrentFilterCommand(arg);
         else if (cmd == QLatin1String("register"))
            handleRegisterCommand(arg);
         else if (cmd == QLatin1String("unregister"))
            handleUnregisterCommand(arg);
         else
            break;

        ++it;
    }
    m_mainWindow->raise();
    m_mainWindow->activateWindow();
}

void RemoteControl::splitInputString(const QString &input, QString &cmd,
                                     QString &arg)
{
    TRACE_OBJ
    QString cmdLine = input.trimmed();
    int i = cmdLine.indexOf(QLatin1Char(' '));
    cmd = cmdLine.left(i);
    arg = cmdLine.mid(i+1);
    cmd = cmd.toLower();
}

void RemoteControl::handleDebugCommand(const QString &arg)
{
    TRACE_OBJ
    m_debug = arg == QLatin1String("on");
}

void RemoteControl::handleShowOrHideCommand(const QString &arg, bool show)
{
    TRACE_OBJ
    if (arg.toLower() == QLatin1String("contents"))
        m_mainWindow->setContentsVisible(show);
    else if (arg.toLower() == QLatin1String("index"))
        m_mainWindow->setIndexVisible(show);
    else if (arg.toLower() == QLatin1String("bookmarks"))
        m_mainWindow->setBookmarksVisible(show);
    else if (arg.toLower() == QLatin1String("search"))
        m_mainWindow->setSearchVisible(show);
}

void RemoteControl::handleSetSourceCommand(const QString &arg)
{
    TRACE_OBJ
    QUrl url(arg);
    if (url.isValid()) {
        if (url.isRelative())
            url = CentralWidget::instance()->currentSource().resolved(url);
        if (m_caching) {
            clearCache();
            m_setSource = url;
        } else {
            CentralWidget::instance()->setSource(url);
        }
    }
}

void RemoteControl::handleSyncContentsCommand()
{
    TRACE_OBJ
    if (m_caching)
        m_syncContents = true;
    else
        m_mainWindow->syncContents();
}

void RemoteControl::handleActivateKeywordCommand(const QString &arg)
{
    TRACE_OBJ
    if (m_caching) {
        clearCache();
        m_activateKeyword = arg;
    } else {
        m_mainWindow->setIndexString(arg);
        if (!arg.isEmpty()) {
            if (!helpEngine.indexWidget()->currentIndex().isValid()
                && helpEngine.fullTextSearchFallbackEnabled()) {
                if (QHelpSearchEngine *se = helpEngine.searchEngine()) {
                    m_mainWindow->setSearchVisible(true);
                    if (QHelpSearchQueryWidget *w = se->queryWidget()) {
                        w->collapseExtendedSearch();
                        QList<QHelpSearchQuery> queryList;
                        queryList << QHelpSearchQuery(QHelpSearchQuery::DEFAULT,
                            QStringList(arg));
                        w->setQuery(queryList);
                        se->search(queryList);
                    }
                }
            } else {
                m_mainWindow->setIndexVisible(true);
                helpEngine.indexWidget()->activateCurrentItem();
            }
        }
    }
}

void RemoteControl::handleActivateIdentifierCommand(const QString &arg)
{
    TRACE_OBJ
    if (m_caching) {
        clearCache();
        m_activateIdentifier = arg;
    } else {
        const QMap<QString, QUrl> &links = helpEngine.linksForIdentifier(arg);
        if (!links.isEmpty())
            CentralWidget::instance()->setSource(links.constBegin().value());
    }
}

void RemoteControl::handleExpandTocCommand(const QString &arg)
{
    TRACE_OBJ
    bool ok = false;
    int depth = -2;
    if (!arg.isEmpty())
        depth = arg.toInt(&ok);
    if (!ok || depth < -2)
        depth = -2;

    if (m_caching)
        m_expandTOC = depth;
    else if (depth != -2)
        m_mainWindow->expandTOC(depth);
}

void RemoteControl::handleSetCurrentFilterCommand(const QString &arg)
{
    TRACE_OBJ
    if (helpEngine.customFilters().contains(arg)) {
        if (m_caching) {
            clearCache();
            m_currentFilter = arg;
        } else {
            helpEngine.setCurrentFilter(arg);
        }
    }
}

void RemoteControl::handleRegisterCommand(const QString &arg)
{
    TRACE_OBJ
    const QString &absFileName = QFileInfo(arg).absoluteFilePath();
    if (helpEngine.registeredDocumentations().
        contains(QHelpEngineCore::namespaceName(absFileName)))
        return;
    if (helpEngine.registerDocumentation(absFileName))
        helpEngine.setupData();
}

void RemoteControl::handleUnregisterCommand(const QString &arg)
{
    TRACE_OBJ
    const QString &absFileName = QFileInfo(arg).absoluteFilePath();
    const QString &ns = QHelpEngineCore::namespaceName(absFileName);
    if (helpEngine.registeredDocumentations().contains(ns)) {
        OpenPagesManager::instance()->closePages(ns);
        if (helpEngine.unregisterDocumentation(ns))
            helpEngine.setupData();
    }
}

void RemoteControl::applyCache()
{
    TRACE_OBJ
    if (m_setSource.isValid()) {
        CentralWidget::instance()->setSource(m_setSource);
    } else if (!m_activateKeyword.isEmpty()) {
        m_mainWindow->setIndexString(m_activateKeyword);
        helpEngine.indexWidget()->activateCurrentItem();
    } else if (!m_activateIdentifier.isEmpty()) {
        QMap<QString, QUrl> links =
            helpEngine.linksForIdentifier(m_activateIdentifier);
        if (!links.isEmpty())
            CentralWidget::instance()->setSource(links.constBegin().value());
    } else if (!m_currentFilter.isEmpty()) {
        helpEngine.setCurrentFilter(m_currentFilter);
    }

    if (m_syncContents)
        m_mainWindow->syncContents();

    Q_ASSERT(m_expandTOC >= -2);
    if (m_expandTOC != -2)
        m_mainWindow->expandTOC(m_expandTOC);

    m_caching = false;
}

void RemoteControl::clearCache()
{
    TRACE_OBJ
    m_currentFilter.clear();
    m_setSource.clear();
    m_syncContents = false;
    m_activateKeyword.clear();
    m_activateIdentifier.clear();
}

QT_END_NAMESPACE
