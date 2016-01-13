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

#ifndef REMOTECONTROL_H
#define REMOTECONTROL_H

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QUrl>

QT_BEGIN_NAMESPACE

class HelpEngineWrapper;
class MainWindow;

class RemoteControl : public QObject
{
    Q_OBJECT

public:
    RemoteControl(MainWindow *mainWindow);

private slots:
    void handleCommandString(const QString &cmdString);
    void applyCache();

private:
    void clearCache();
    void splitInputString(const QString &input, QString &cmd, QString &arg);
    void handleDebugCommand(const QString &arg);
    void handleShowOrHideCommand(const QString &arg, bool show);
    void handleSetSourceCommand(const QString &arg);
    void handleSyncContentsCommand();
    void handleActivateKeywordCommand(const QString &arg);
    void handleActivateIdentifierCommand(const QString &arg);
    void handleExpandTocCommand(const QString &arg);
    void handleSetCurrentFilterCommand(const QString &arg);
    void handleRegisterCommand(const QString &arg);
    void handleUnregisterCommand(const QString &arg);

private:
    MainWindow *m_mainWindow;
    bool m_debug;

    bool m_caching;
    QUrl m_setSource;
    bool m_syncContents;
    QString m_activateKeyword;
    QString m_activateIdentifier;
    int m_expandTOC;
    QString m_currentFilter;
    HelpEngineWrapper &helpEngine;
};

QT_END_NAMESPACE

#endif
