/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQuick.Dialogs module of the Qt Toolkit.
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

#ifndef QQUICKFILEDIALOG_P_H
#define QQUICKFILEDIALOG_P_H

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

#include "qquickabstractfiledialog_p.h"
#include <QJSValue>
#include <QStandardPaths>

QT_BEGIN_NAMESPACE

class QQuickFileDialog : public QQuickAbstractFileDialog
{
    Q_OBJECT
    Q_PROPERTY(QQuickItem* contentItem READ contentItem WRITE setContentItem DESIGNABLE false)
    Q_PROPERTY(QJSValue shortcuts READ shortcuts CONSTANT)
    Q_CLASSINFO("DefaultProperty", "contentItem")    // AbstractFileDialog in QML can have only one child

public:
    explicit QQuickFileDialog(QObject *parent = 0);
    ~QQuickFileDialog();
    virtual QList<QUrl> fileUrls() const;

    QJSValue shortcuts();

Q_SIGNALS:

public Q_SLOTS:
    void clearSelection();
    bool addSelection(const QUrl &path);

protected:
    virtual QPlatformFileDialogHelper *helper() { return 0; }
    Q_INVOKABLE QString urlToPath(const QUrl &url) { return url.toLocalFile(); }
    Q_INVOKABLE QUrl pathToUrl(const QString &path) { return QUrl::fromLocalFile(path); }
    Q_INVOKABLE QUrl pathFolder(const QString &path);

    void addShortcut(int &i, const QString &name, const QString &path);
    void addIfReadable(int &i, const QString &name, QStandardPaths::StandardLocation loc);

private:
    QList<QUrl> m_selections;

    Q_DISABLE_COPY(QQuickFileDialog)
    QJSValue m_shortcuts;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickFileDialog *)

#endif // QQUICKFILEDIALOG_P_H
