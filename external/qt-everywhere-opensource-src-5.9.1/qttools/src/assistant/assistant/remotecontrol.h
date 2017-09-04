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
