/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Dialogs module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QQUICKABSTRACTMESSAGEDIALOG_P_H
#define QQUICKABSTRACTMESSAGEDIALOG_P_H

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

#include <QQuickView>
#include <QtGui/qpa/qplatformdialoghelper.h>
#include <qpa/qplatformtheme.h>
#include "qquickabstractdialog_p.h"

QT_BEGIN_NAMESPACE

class QQuickAbstractMessageDialog : public QQuickAbstractDialog
{
    Q_OBJECT

    Q_ENUMS(Icon)

    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(QString informativeText READ informativeText WRITE setInformativeText NOTIFY informativeTextChanged)
    Q_PROPERTY(QString detailedText READ detailedText WRITE setDetailedText NOTIFY detailedTextChanged)
    Q_PROPERTY(Icon icon READ icon WRITE setIcon NOTIFY iconChanged)
    Q_PROPERTY(QUrl standardIconSource READ standardIconSource NOTIFY iconChanged)
    Q_PROPERTY(QQuickAbstractDialog::StandardButtons standardButtons READ standardButtons WRITE setStandardButtons NOTIFY standardButtonsChanged)
    Q_PROPERTY(QQuickAbstractDialog::StandardButton clickedButton READ clickedButton NOTIFY buttonClicked)

public:
    QQuickAbstractMessageDialog(QObject *parent = 0);
    virtual ~QQuickAbstractMessageDialog();

    virtual QString title() const { return m_options->windowTitle(); }
    QString text() const { return m_options->text(); }
    QString informativeText() const { return m_options->informativeText(); }
    QString detailedText() const { return m_options->detailedText(); }

    enum Icon {
        NoIcon = QMessageDialogOptions::NoIcon,
        Information = QMessageDialogOptions::Information,
        Warning = QMessageDialogOptions::Warning,
        Critical = QMessageDialogOptions::Critical,
        Question = QMessageDialogOptions::Question
    };

    Icon icon() const { return static_cast<Icon>(m_options->icon()); }

    QUrl standardIconSource();

    StandardButtons standardButtons() const { return static_cast<StandardButtons>(static_cast<int>(m_options->standardButtons())); }

    StandardButton clickedButton() const { return m_clickedButton; }

public Q_SLOTS:
    virtual void setVisible(bool v);
    virtual void setTitle(const QString &arg);
    void setText(const QString &arg);
    void setInformativeText(const QString &arg);
    void setDetailedText(const QString &arg);
    void setIcon(Icon icon);
    void setStandardButtons(StandardButtons buttons);
    void click(QQuickAbstractDialog::StandardButton button);

Q_SIGNALS:
    void textChanged();
    void informativeTextChanged();
    void detailedTextChanged();
    void iconChanged();
    void standardButtonsChanged();
    void buttonClicked();
    void discard();
    void help();
    void yes();
    void no();
    void apply();
    void reset();

protected Q_SLOTS:
    void click(QPlatformDialogHelper::StandardButton button, QPlatformDialogHelper::ButtonRole);

protected:
    QPlatformMessageDialogHelper *m_dlgHelper;
    QSharedPointer<QMessageDialogOptions> m_options;
    StandardButton m_clickedButton;

    Q_DISABLE_COPY(QQuickAbstractMessageDialog)
};

QT_END_NAMESPACE

#endif // QQUICKABSTRACTMESSAGEDIALOG_P_H
