/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#ifndef QQUICKQFILEDIALOG_P_H
#define QQUICKQFILEDIALOG_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QFileDialog>
#include "../dialogs/qquickabstractfiledialog_p.h"

QT_BEGIN_NAMESPACE

class QQuickQFileDialog : public QQuickAbstractFileDialog
{
    Q_OBJECT

public:
    QQuickQFileDialog(QObject *parent = 0);
    virtual ~QQuickQFileDialog();

protected:
    QPlatformFileDialogHelper *helper();

    Q_DISABLE_COPY(QQuickQFileDialog)
};

class QFileDialogHelper : public QPlatformFileDialogHelper
{
    Q_OBJECT
public:
    QFileDialogHelper();

    bool defaultNameFilterDisables() const Q_DECL_OVERRIDE { return true; }
    void setDirectory(const QUrl &dir) Q_DECL_OVERRIDE { m_dialog.setDirectoryUrl(dir); }
    QUrl directory() const Q_DECL_OVERRIDE { return m_dialog.directoryUrl(); }
    void selectFile(const QUrl &f) Q_DECL_OVERRIDE { m_dialog.selectUrl(f); }
    QList<QUrl> selectedFiles() const Q_DECL_OVERRIDE;
    void setFilter() Q_DECL_OVERRIDE;
    void selectNameFilter(const QString &f) Q_DECL_OVERRIDE { m_dialog.selectNameFilter(f); }
    QString selectedNameFilter() const Q_DECL_OVERRIDE { return m_dialog.selectedNameFilter(); }
    void exec() Q_DECL_OVERRIDE { m_dialog.exec(); }
    bool show(Qt::WindowFlags f, Qt::WindowModality m, QWindow *parent) Q_DECL_OVERRIDE;
    void hide() Q_DECL_OVERRIDE { m_dialog.hide(); }

private Q_SLOTS:
    void currentChanged(const QString& path);
    void directoryEntered(const QString& path);
    void fileSelected(const QString& path);
    void filesSelected(const QStringList& paths);

private:
    QFileDialog m_dialog;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickQFileDialog *)

#endif // QQUICKQFILEDIALOG_P_H
