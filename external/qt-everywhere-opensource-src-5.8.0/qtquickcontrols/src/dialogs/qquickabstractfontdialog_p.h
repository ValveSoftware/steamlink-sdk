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

#ifndef QQUICKABSTRACTFONTDIALOG_P_H
#define QQUICKABSTRACTFONTDIALOG_P_H

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
#include <QtGui/qfont.h>
#include <qpa/qplatformtheme.h>
#include "qquickabstractdialog_p.h"

QT_BEGIN_NAMESPACE

class QQuickAbstractFontDialog : public QQuickAbstractDialog
{
    Q_OBJECT
    Q_PROPERTY(bool scalableFonts READ scalableFonts WRITE setScalableFonts NOTIFY scalableFontsChanged)
    Q_PROPERTY(bool nonScalableFonts READ nonScalableFonts WRITE setNonScalableFonts NOTIFY nonScalableFontsChanged)
    Q_PROPERTY(bool monospacedFonts READ monospacedFonts WRITE setMonospacedFonts NOTIFY monospacedFontsChanged)
    Q_PROPERTY(bool proportionalFonts READ proportionalFonts WRITE setProportionalFonts NOTIFY proportionalFontsChanged)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY fontChanged)
    Q_PROPERTY(QFont currentFont READ currentFont WRITE setCurrentFont NOTIFY currentFontChanged)

public:
    QQuickAbstractFontDialog(QObject *parent = 0);
    virtual ~QQuickAbstractFontDialog();

    virtual QString title() const;
    bool scalableFonts() const;
    bool nonScalableFonts() const;
    bool monospacedFonts() const;
    bool proportionalFonts() const;
    QFont font() const { return m_font; }
    QFont currentFont() const { return m_currentFont; }

public Q_SLOTS:
    void setVisible(bool v);
    void setModality(Qt::WindowModality m);
    void setTitle(const QString &t);
    void setFont(const QFont &arg);
    void setCurrentFont(const QFont &arg);
    void setScalableFonts(bool arg);
    void setNonScalableFonts(bool arg);
    void setMonospacedFonts(bool arg);
    void setProportionalFonts(bool arg);

Q_SIGNALS:
    void scalableFontsChanged();
    void nonScalableFontsChanged();
    void monospacedFontsChanged();
    void proportionalFontsChanged();
    void fontChanged();
    void currentFontChanged();
    void selectionAccepted();

protected Q_SLOTS:
    virtual void accept();

protected:
    QPlatformFontDialogHelper *m_dlgHelper;
    QSharedPointer<QFontDialogOptions> m_options;
    QFont m_font;
    QFont m_currentFont;

    Q_DISABLE_COPY(QQuickAbstractFontDialog)
};

QT_END_NAMESPACE

#endif // QQUICKABSTRACTFONTDIALOG_P_H
