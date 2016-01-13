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

#ifndef QQUICKDIALOG_P_H
#define QQUICKDIALOG_P_H

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

#include "qquickabstractmessagedialog_p.h"
#include <QJSValue>

QT_BEGIN_NAMESPACE

class QQuickDialog : public QQuickAbstractDialog
{
    Q_OBJECT

    Q_ENUMS(StandardButton)
    Q_FLAGS(StandardButtons)

    Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
    Q_PROPERTY(QQuickAbstractDialog::StandardButtons standardButtons READ standardButtons WRITE setStandardButtons NOTIFY standardButtonsChanged)
    Q_PROPERTY(QQuickAbstractDialog::StandardButton clickedButton READ clickedButton NOTIFY buttonClicked)
    Q_PROPERTY(QQuickItem* contentItem READ contentItem WRITE setContentItem DESIGNABLE false)
    Q_CLASSINFO("DefaultProperty", "contentItem")    // Dialog in QML can have only one child

public:
    explicit QQuickDialog(QObject *parent = 0);
    ~QQuickDialog();

    StandardButtons standardButtons() const { return m_enabledButtons; }
    StandardButton clickedButton() const { return m_clickedButton; }
    Q_INVOKABLE QJSValue __standardButtonsLeftModel();
    Q_INVOKABLE QJSValue __standardButtonsRightModel();

    QString title() const { return m_title; }
    virtual void setVisible(bool v);

public Q_SLOTS:
    virtual void setTitle(const QString &arg);
    void setStandardButtons(StandardButtons buttons);
    void click(QQuickAbstractDialog::StandardButton button);

Q_SIGNALS:
    void titleChanged();
    void standardButtonsChanged();
    void buttonClicked();
    void discard();
    void help();
    void yes();
    void no();
    void apply();
    void reset();

protected:
    virtual QPlatformDialogHelper *helper() { return 0; }
    void click(QPlatformDialogHelper::StandardButton button, QPlatformDialogHelper::ButtonRole);

protected Q_SLOTS:
    virtual void accept();
    virtual void reject();

private:
    void updateStandardButtons();

private:
    QString m_title;
    StandardButtons m_enabledButtons;
    StandardButton m_clickedButton;
    QJSValue m_standardButtonsLeftModel;
    QJSValue m_standardButtonsRightModel;
    Q_DISABLE_COPY(QQuickDialog)
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QQuickDialog *)

#endif // QQUICKDIALOG_P_H
