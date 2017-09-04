/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Labs Platform module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwidgetplatformfontdialog_p.h"
#include "qwidgetplatformdialog_p.h"

#include <QtWidgets/qfontdialog.h>

QT_BEGIN_NAMESPACE

QWidgetPlatformFontDialog::QWidgetPlatformFontDialog(QObject *parent)
    : m_dialog(new QFontDialog)
{
    setParent(parent);

    connect(m_dialog.data(), &QFontDialog::accepted, this, &QPlatformDialogHelper::accept);
    connect(m_dialog.data(), &QFontDialog::rejected, this, &QPlatformDialogHelper::reject);
    connect(m_dialog.data(), &QFontDialog::currentFontChanged, this, &QPlatformFontDialogHelper::currentFontChanged);
}

QWidgetPlatformFontDialog::~QWidgetPlatformFontDialog()
{
}

QFont QWidgetPlatformFontDialog::currentFont() const
{
    return m_dialog->currentFont();
}

void QWidgetPlatformFontDialog::setCurrentFont(const QFont &font)
{
    m_dialog->setCurrentFont(font);
}

void QWidgetPlatformFontDialog::exec()
{
    m_dialog->exec();
}

bool QWidgetPlatformFontDialog::show(Qt::WindowFlags flags, Qt::WindowModality modality, QWindow *parent)
{
    QSharedPointer<QFontDialogOptions> options = QPlatformFontDialogHelper::options();
    m_dialog->setWindowTitle(options->windowTitle());
    m_dialog->setOptions(static_cast<QFontDialog::FontDialogOptions>(int(options->options())) | QFontDialog::DontUseNativeDialog);

    return QWidgetPlatformDialog::show(m_dialog.data(), flags, modality, parent);
}

void QWidgetPlatformFontDialog::hide()
{
    m_dialog->hide();
}

QT_END_NAMESPACE
