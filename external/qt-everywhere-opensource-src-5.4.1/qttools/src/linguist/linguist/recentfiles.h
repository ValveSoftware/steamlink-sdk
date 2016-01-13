/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Linguist of the Qt Toolkit.
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

#ifndef RECENTFILES_H
#define RECENTFILES_H

#include <QString>
#include <QStringList>
#include <QTimer>

QT_BEGIN_NAMESPACE

class RecentFiles : public QObject
{
    Q_OBJECT

public:
    explicit RecentFiles(const int maxEntries);

    bool isEmpty() { return m_strLists.isEmpty(); }
    void addFiles(const QStringList &names);
    QString lastOpenedFile() const {
        if (m_strLists.isEmpty() || m_strLists.first().isEmpty())
            return QString::null;
        return m_strLists.at(0).at(0);
    }
    const QList<QStringList>& filesLists() const { return m_strLists; }

    void readConfig();
    void writeConfig() const;

public slots:
    void closeGroup();

private:
    bool m_groupOpen;
    bool m_clone1st;
    int m_maxEntries;
    QList<QStringList> m_strLists;
    QTimer m_timer;
};

QT_END_NAMESPACE

#endif // RECENTFILES_H
