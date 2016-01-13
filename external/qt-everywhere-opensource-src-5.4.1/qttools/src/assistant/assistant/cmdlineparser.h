/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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

#ifndef CMDLINEPARSER_H
#define CMDLINEPARSER_H

#include <QtCore/QCoreApplication>
#include <QtCore/QStringList>
#include <QtCore/QUrl>

QT_BEGIN_NAMESPACE

class CmdLineParser
{
    Q_DECLARE_TR_FUNCTIONS(CmdLineParser)
public:
    enum Result {Ok, Help, Error};
    enum ShowState {Untouched, Show, Hide, Activate};
    enum RegisterState {None, Register, Unregister};

    CmdLineParser(const QStringList &arguments);
    Result parse();

    void setCollectionFile(const QString &file);
    QString collectionFile() const;
    bool collectionFileGiven() const;
    QString cloneFile() const;
    QUrl url() const;
    bool enableRemoteControl() const;
    ShowState contents() const;
    ShowState index() const;
    ShowState bookmarks() const;
    ShowState search() const;
    QString currentFilter() const;
    bool removeSearchIndex() const;
    bool rebuildSearchIndex() const;
    RegisterState registerRequest() const;
    QString helpFile() const;

    void showMessage(const QString &msg, bool error);

private:
    QString getFileName(const QString &fileName);
    bool hasMoreArgs() const;
    const QString &nextArg();
    void handleCollectionFileOption();
    void handleShowUrlOption();
    void handleShowOption();
    void handleHideOption();
    void handleActivateOption();
    void handleShowOrHideOrActivateOption(ShowState state);
    void handleRegisterOption();
    void handleUnregisterOption();
    void handleRegisterOrUnregisterOption(RegisterState state);
    void handleSetCurrentFilterOption();

    QStringList m_arguments;
    int m_pos;
    QString m_collectionFile;
    QString m_cloneFile;
    QString m_helpFile;
    QUrl m_url;
    bool m_enableRemoteControl;

    ShowState m_contents;
    ShowState m_index;
    ShowState m_bookmarks;
    ShowState m_search;
    RegisterState m_register;
    QString m_currentFilter;
    bool m_removeSearchIndex;
    bool m_rebuildSearchIndex;
    bool m_quiet;
    QString m_error;
};

QT_END_NAMESPACE

#endif
