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

#include "HomeView.h"

#include <QtCore>
#include <QtWidgets>

#include "AddressBar.h"
#include "BookmarksView.h"

HomeView::HomeView(QWidget *parent)
    : QWidget(parent)
    , m_addressBar(0)
{
    m_addressBar = new AddressBar(parent);
    connect(m_addressBar, SIGNAL(addressEntered(QString)), SLOT(gotoAddress(QString)));

    m_bookmarks = new BookmarksView(parent);
    connect(m_bookmarks, SIGNAL(urlSelected(QUrl)), SIGNAL(urlActivated(QUrl)));

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setMargin(4);
    layout->setSpacing(4);
    layout->addWidget(m_addressBar);
    layout->addWidget(m_bookmarks);
}

void HomeView::gotoAddress(const QString &address)
{
    emit addressEntered(address);
}

void HomeView::focusInEvent(QFocusEvent *event)
{
    m_addressBar->setFocus();
    QWidget::focusInEvent(event);
}
