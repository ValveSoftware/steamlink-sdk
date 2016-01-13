/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
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

#include "BookmarksView.h"

#include <QtWidgets>

BookmarksView::BookmarksView(QWidget *parent)
    : QWidget(parent)
{
    QListWidget *m_iconView = new QListWidget(this);
    connect(m_iconView, SIGNAL(itemActivated(QListWidgetItem*)), SLOT(activate(QListWidgetItem*)));

    QVBoxLayout *layout = new QVBoxLayout(this);
    setLayout(layout);
    layout->addWidget(m_iconView);

    m_iconView->addItem("www.google.com");
    m_iconView->addItem("qt-project.org/doc/qt-5.0");
    m_iconView->addItem("news.bbc.co.uk/2/mobile/default.stm");
    m_iconView->addItem("mobile.wikipedia.org");
    m_iconView->addItem("qt.digia.com");
    m_iconView->addItem("en.wikipedia.org");

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void BookmarksView::activate(QListWidgetItem *item)
{
    QUrl url = item->text().prepend("http://");
    emit urlSelected(url);
}
