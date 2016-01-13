/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the demonstration applications of the Qt Toolkit.
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

// EXTERNAL INCLUDES
#include <QKeyEvent>
#include <QMessageBox>
#include <QListWidget>
#include <QVBoxLayout>
#include <QFileInfoList>
#include <QListWidgetItem>

// INTERNAL INCLUDES

// CLASS HEADER
#include "contenttab.h"


// CONSTRUCTORS & DESTRUCTORS
ContentTab::ContentTab(QWidget *parent) :
        QListWidget(parent)
{
    setDragEnabled(false);
    setIconSize(QSize(45, 45));
}

ContentTab::~ContentTab()
{
}

// NEW PUBLIC METHODS
void ContentTab::init(const QStandardPaths::StandardLocation &location,
                      const QString &filter,
                      const QString &icon)
{
    setContentDir(location);
    QStringList filterList;
    filterList = filter.split(";");
    m_ContentDir.setNameFilters(filterList);
    setIcon(icon);

    connect(this, SIGNAL(itemClicked(QListWidgetItem*)),
            this, SLOT(openItem(QListWidgetItem*)));

    populateListWidget();
}

// NEW PROTECTED METHODS
void ContentTab::setContentDir(const QStandardPaths::StandardLocation &location)
{
    m_ContentDir.setPath(QStandardPaths::writableLocation(location));
}

void ContentTab::setIcon(const QString &icon)
{
    m_Icon = QIcon(icon);
}

void ContentTab::populateListWidget()
{
    QFileInfoList fileList = m_ContentDir.entryInfoList(QDir::Files, QDir::Time);
    foreach(QFileInfo item, fileList) {
        new QListWidgetItem(m_Icon, itemName(item), this);
    }
}

QString ContentTab::itemName(const QFileInfo &item)
{
    return QString(item.baseName() + "." + item.completeSuffix());
}

QUrl ContentTab::itemUrl(QListWidgetItem *item)
{
    return QUrl("file:///" + m_ContentDir.absolutePath() + "/" + item->text());
}

void ContentTab::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Select:
        openItem(currentItem());
    default:
        QListWidget::keyPressEvent(event);
        break;
    }
}

void ContentTab::handleErrorInOpen(QListWidgetItem *item)
{
    Q_UNUSED(item);
    QMessageBox::warning(this, tr("Operation Failed"), tr("Unknown error!"), QMessageBox::Close);
}

// NEW SLOTS
void ContentTab::openItem(QListWidgetItem *item)
{
    bool ret = QDesktopServices::openUrl(itemUrl(item));
    if (!ret)
        handleErrorInOpen(item);
}


// End of File
