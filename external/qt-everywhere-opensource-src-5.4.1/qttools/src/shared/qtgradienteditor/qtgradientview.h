/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the tools applications of the Qt Toolkit.
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

#ifndef GRADIENTVIEW_H
#define GRADIENTVIEW_H

#include <QtWidgets/QWidget>
#include <QtCore/QMap>
#include "ui_qtgradientview.h"

QT_BEGIN_NAMESPACE

class QtGradientManager;
class QListViewItem;
class QAction;

class QtGradientView : public QWidget
{
    Q_OBJECT
public:
    QtGradientView(QWidget *parent = 0);

    void setGradientManager(QtGradientManager *manager);
    QtGradientManager *gradientManager() const;

    void setCurrentGradient(const QString &id);
    QString currentGradient() const;

signals:
    void currentGradientChanged(const QString &id);
    void gradientActivated(const QString &id);

private slots:
    void slotGradientAdded(const QString &id, const QGradient &gradient);
    void slotGradientRenamed(const QString &id, const QString &newId);
    void slotGradientChanged(const QString &id, const QGradient &newGradient);
    void slotGradientRemoved(const QString &id);
    void slotNewGradient();
    void slotEditGradient();
    void slotRemoveGradient();
    void slotRenameGradient();
    void slotRenameGradient(QListWidgetItem *item);
    void slotCurrentItemChanged(QListWidgetItem *item);
    void slotGradientActivated(QListWidgetItem *item);

private:
    QMap<QString, QListWidgetItem *> m_idToItem;
    QMap<QListWidgetItem *, QString> m_itemToId;

    QAction *m_newAction;
    QAction *m_editAction;
    QAction *m_renameAction;
    QAction *m_removeAction;

    QtGradientManager *m_manager;
    Ui::QtGradientView m_ui;
};

QT_END_NAMESPACE

#endif
