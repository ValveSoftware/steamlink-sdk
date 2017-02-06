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

#include "qquickabstractcolordialog_p.h"
#include "qquickitem.h"

#include <private/qguiapplication_p.h>
#include <QWindow>
#include <QQuickWindow>

QT_BEGIN_NAMESPACE

QQuickAbstractColorDialog::QQuickAbstractColorDialog(QObject *parent)
    : QQuickAbstractDialog(parent)
    , m_dlgHelper(0)
    , m_options(QColorDialogOptions::create())
{
    // On the Mac, modality doesn't work unless you call exec().  But this is a reasonable default anyway.
    m_modality = Qt::NonModal;
    connect(this, SIGNAL(accepted()), this, SIGNAL(selectionAccepted()));
}

QQuickAbstractColorDialog::~QQuickAbstractColorDialog()
{
}

void QQuickAbstractColorDialog::setVisible(bool v)
{
    if (helper() && v) {
        m_dlgHelper->setOptions(m_options);
        // Due to the fact that QColorDialogOptions doesn't have currentColor...
        m_dlgHelper->setCurrentColor(m_color);
    }
    QQuickAbstractDialog::setVisible(v);
}

void QQuickAbstractColorDialog::setModality(Qt::WindowModality m)
{
#ifdef Q_OS_MAC
    // On the Mac, modality doesn't work unless you call exec()
    m_modality = Qt::NonModal;
    emit modalityChanged();
    return;
#endif
    QQuickAbstractDialog::setModality(m);
}

QString QQuickAbstractColorDialog::title() const
{
    return m_options->windowTitle();
}

bool QQuickAbstractColorDialog::showAlphaChannel() const
{
    return m_options->testOption(QColorDialogOptions::ShowAlphaChannel);
}

void QQuickAbstractColorDialog::setTitle(const QString &t)
{
    if (m_options->windowTitle() == t) return;
    m_options->setWindowTitle(t);
    emit titleChanged();
}

void QQuickAbstractColorDialog::setColor(QColor arg)
{
    if (m_dlgHelper)
        m_dlgHelper->setCurrentColor(arg);
    // m_options->setCustomColor or setStandardColor don't make sense here
    if (m_color != arg) {
        m_color = arg;
        emit colorChanged();
    }
    setCurrentColor(arg);
}

void QQuickAbstractColorDialog::setCurrentColor(QColor currentColor)
{
    if (m_currentColor != currentColor) {
        m_currentColor = currentColor;
        emit currentColorChanged();
    }
}

void QQuickAbstractColorDialog::setShowAlphaChannel(bool arg)
{
    m_options->setOption(QColorDialogOptions::ShowAlphaChannel, arg);
    emit showAlphaChannelChanged();
}

void QQuickAbstractColorDialog::accept()
{
    setColor(m_currentColor);
    QQuickAbstractDialog::accept();
}

QT_END_NAMESPACE
