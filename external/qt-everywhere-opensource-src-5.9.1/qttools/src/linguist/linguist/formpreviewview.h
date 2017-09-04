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

#ifndef FORMPREVIEWVIEW_H
#define FORMPREVIEWVIEW_H

#include <private/quiloader_p.h>

#include <QtCore/QHash>
#include <QtCore/QList>

#include <QtWidgets/QMainWindow>

QT_BEGIN_NAMESPACE

class MultiDataModel;
class FormFrame;
class MessageItem;

class QComboBox;
class QListWidgetItem;
class QGridLayout;
class QMdiArea;
class QMdiSubWindow;
class QToolBox;
class QTableWidgetItem;
class QTreeWidgetItem;

enum TranslatableEntryType {
    TranslatableProperty,
    TranslatableToolItemText,
    TranslatableToolItemToolTip,
    TranslatableTabPageText,
    TranslatableTabPageToolTip,
    TranslatableTabPageWhatsThis,
    TranslatableListWidgetItem,
    TranslatableTableWidgetItem,
    TranslatableTreeWidgetItem,
    TranslatableComboBoxItem
};

struct TranslatableEntry {
    TranslatableEntryType type;
    union {
        QObject *object;
        QComboBox *comboBox;
        QTabWidget *tabWidget;
        QToolBox *toolBox;
        QListWidgetItem *listWidgetItem;
        QTableWidgetItem *tableWidgetItem;
        QTreeWidgetItem *treeWidgetItem;
    } target;
    union {
        char *name;
        int index;
        struct {
            short index; // Known to be below 1000
            short column;
        } treeIndex;
    } prop;
};

typedef QHash<QUiTranslatableStringValue, QList<TranslatableEntry> > TargetsHash;

class FormPreviewView : public QMainWindow
{
    Q_OBJECT
public:
    FormPreviewView(QWidget *parent, MultiDataModel *dataModel);

    void setSourceContext(int model, MessageItem *messageItem);

private:
    bool m_isActive;
    QString m_currentFileName;
    QMdiArea *m_mdiArea;
    QMdiSubWindow *m_mdiSubWindow;
    QWidget *m_form;
    TargetsHash m_targets;
    QList<TranslatableEntry> m_highlights;
    MultiDataModel *m_dataModel;

    QString m_lastFormName;
    QString m_lastClassName;
    int m_lastModel;
};

QT_END_NAMESPACE

#endif // FORMPREVIEWVIEW_H
