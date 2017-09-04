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

#ifndef CONTENTTAB_H_
#define CONTENTTAB_H_

// EXTERNAL INCLUDES
#include <QDir>
#include <QUrl>
#include <QIcon>
#include <QFileInfo>
#include <QListWidget>
#include <QDesktopServices>
#include <QStandardPaths>

// INTERNAL INCLUDES

// FORWARD DECLARATIONS
QT_BEGIN_NAMESPACE
class QListWidgetItem;
QT_END_NAMESPACE

// CLASS DECLARATION

/**
* ContentTab class.
*
* This class implements general purpose tab for media files.
*/
class ContentTab : public QListWidget
{
    Q_OBJECT

public:         // Constructors & Destructors
    ContentTab(QWidget *parent);
    virtual ~ContentTab();

public:         // New Methods
    virtual void init(const QStandardPaths::StandardLocation &location,
                      const QString &filter,
                      const QString &icon);

protected:      // New Methods
    virtual void setContentDir(const QStandardPaths::StandardLocation &location);
    virtual void setIcon(const QString &icon);
    virtual void populateListWidget();
    virtual QString itemName(const QFileInfo &item);
    virtual QUrl itemUrl(QListWidgetItem *item);
    virtual void handleErrorInOpen(QListWidgetItem *item);
protected:
    void keyPressEvent(QKeyEvent *event) override;

public slots:   // New Slots
    virtual void openItem(QListWidgetItem *item);

protected:     // Owned variables
    QDir m_ContentDir;
    QIcon m_Icon;
};


#endif // CONTENTTAB_H_

// End of File
