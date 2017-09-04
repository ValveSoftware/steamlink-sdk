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
#include <QUrl>
#include <QMessageBox>
#include <QListWidgetItem>

// INTERNAL INCLUDES

// CLASS HEADER
#include "linktab.h"

LinkTab::LinkTab(QWidget *parent) :
        ContentTab(parent)
{
}

LinkTab::~LinkTab()
{
}

void LinkTab::populateListWidget()
{
    m_WebItem = new QListWidgetItem(QIcon(":/resources/browser.png"), tr("Launch Browser"), this);
    m_MailToItem = new QListWidgetItem(QIcon(":/resources/message.png"), tr("New e-mail"), this);
}

QUrl LinkTab::itemUrl(QListWidgetItem *item)
{
    if (m_WebItem == item) {
        return QUrl(tr("http://qt.io"));
    } else if (m_MailToItem == item) {
        return QUrl(tr("mailto:noreply@qt.io?subject=Qt feedback&body=Hello"));
    } else {
        // We should never endup here
        Q_ASSERT(false);
        return QUrl();
    }
}
void LinkTab::handleErrorInOpen(QListWidgetItem *item)
{
    if (m_MailToItem == item) {
        QMessageBox::warning(this, tr("Operation Failed"), tr("Please check that you have\ne-mail account defined."), QMessageBox::Close);
    } else {
        ContentTab::handleErrorInOpen(item);
    }
}

// End of file
