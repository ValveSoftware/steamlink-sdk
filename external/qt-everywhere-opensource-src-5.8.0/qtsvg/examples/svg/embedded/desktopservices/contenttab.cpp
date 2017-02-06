/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the demonstration applications of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
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
    const QFileInfoList fileList = m_ContentDir.entryInfoList(QDir::Files, QDir::Time);
    for (const QFileInfo &item : fileList)
        new QListWidgetItem(m_Icon, itemName(item), this);
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
