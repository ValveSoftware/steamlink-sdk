/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Assistant of the Qt Toolkit.
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

#ifndef FILESPAGE_H
#define FILESPAGE_H

#include <QtWidgets/QWizardPage>
#include "ui_filespage.h"

QT_BEGIN_NAMESPACE

class FilesPage : public QWizardPage
{
    Q_OBJECT

public:
    FilesPage(QWidget *parent = 0);
    void setFilesToRemove(const QStringList &files);
    QStringList filesToRemove() const;

private slots:
    void removeFile();
    void removeAllFiles();

private:
    bool eventFilter(QObject *obj, QEvent *event);

    Ui::FilesPage m_ui;
    QStringList m_files;
    QStringList m_filesToRemove;
};

QT_END_NAMESPACE

#endif
