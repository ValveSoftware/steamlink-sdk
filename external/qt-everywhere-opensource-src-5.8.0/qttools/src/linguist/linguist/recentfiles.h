/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Linguist of the Qt Toolkit.
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
