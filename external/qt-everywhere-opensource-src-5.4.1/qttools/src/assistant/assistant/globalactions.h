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

#ifndef GLOBALACTION_H
#define GLOBALACTION_H

#include <QtCore/QList>
#include <QtCore/QObject>

QT_BEGIN_NAMESPACE

class QAction;
class QMenu;

class GlobalActions : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(GlobalActions)
public:
    static GlobalActions *instance(QObject *parent = 0);

    QList<QAction *> actionList() const { return m_actionList; }
    QAction *backAction() const { return m_backAction; }
    QAction *nextAction() const { return m_nextAction; }
    QAction *homeAction() const { return m_homeAction; }
    QAction *zoomInAction() const { return m_zoomInAction; }
    QAction *zoomOutAction() const { return m_zoomOutAction; }
    QAction *copyAction() const { return m_copyAction; }
    QAction *printAction() const { return m_printAction; }
    QAction *findAction() const { return m_findAction; }

    Q_SLOT void updateActions();
    Q_SLOT void setCopyAvailable(bool available);

#if !defined(QT_NO_WEBKIT)
private slots:
    void slotAboutToShowBackMenu();
    void slotAboutToShowNextMenu();
    void slotOpenActionUrl(QAction *action);
#endif

private:
    void setupNavigationMenus(QAction *back, QAction *next, QWidget *parent);

private:
    GlobalActions(QObject *parent);

    static GlobalActions *m_instance;

    QAction *m_backAction;
    QAction *m_nextAction;
    QAction *m_homeAction;
    QAction *m_zoomInAction;
    QAction *m_zoomOutAction;
    QAction *m_copyAction;
    QAction *m_printAction;
    QAction *m_findAction;

    QList<QAction *> m_actionList;

    QMenu *m_backMenu;
    QMenu *m_nextMenu;
};

QT_END_NAMESPACE

#endif // GLOBALACTION_H
