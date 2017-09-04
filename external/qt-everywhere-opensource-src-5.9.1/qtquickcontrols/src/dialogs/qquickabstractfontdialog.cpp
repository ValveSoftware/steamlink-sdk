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

#include "qquickabstractfontdialog_p.h"
#include "qquickitem.h"

#include <private/qguiapplication_p.h>
#include <QWindow>
#include <QQuickWindow>

QT_BEGIN_NAMESPACE

QQuickAbstractFontDialog::QQuickAbstractFontDialog(QObject *parent)
    : QQuickAbstractDialog(parent)
    , m_dlgHelper(0)
    , m_options(QFontDialogOptions::create())
{
    // On the Mac, modality doesn't work unless you call exec().  But this is a reasonable default anyway.
    m_modality = Qt::NonModal;
    connect(this, SIGNAL(accepted()), this, SIGNAL(selectionAccepted()));
}

QQuickAbstractFontDialog::~QQuickAbstractFontDialog()
{
}

void QQuickAbstractFontDialog::setVisible(bool v)
{
    if (helper() && v) {
        m_dlgHelper->setOptions(m_options);
        // Due to the fact that QFontDialogOptions doesn't have currentFont...
        m_dlgHelper->setCurrentFont(m_font);
    }
    QQuickAbstractDialog::setVisible(v);
}

void QQuickAbstractFontDialog::setModality(Qt::WindowModality m)
{
#ifdef Q_OS_MAC
    // On the Mac, modality doesn't work unless you call exec()
    m_modality = Qt::NonModal;
    emit modalityChanged();
    return;
#endif
    QQuickAbstractDialog::setModality(m);
}

QString QQuickAbstractFontDialog::title() const
{
    return m_options->windowTitle();
}

bool QQuickAbstractFontDialog::scalableFonts() const
{
    return m_options->testOption(QFontDialogOptions::ScalableFonts);
}

bool QQuickAbstractFontDialog::nonScalableFonts() const
{
    return m_options->testOption(QFontDialogOptions::NonScalableFonts);
}

bool QQuickAbstractFontDialog::monospacedFonts() const
{
    return m_options->testOption(QFontDialogOptions::MonospacedFonts);
}

bool QQuickAbstractFontDialog::proportionalFonts() const
{
    return m_options->testOption(QFontDialogOptions::ProportionalFonts);
}

void QQuickAbstractFontDialog::setTitle(const QString &t)
{
    if (m_options->windowTitle() == t) return;
    m_options->setWindowTitle(t);
    emit titleChanged();
}

void QQuickAbstractFontDialog::setFont(const QFont &arg)
{
    if (m_font != arg) {
        m_font = arg;
        emit fontChanged();
    }
    setCurrentFont(arg);
}

void QQuickAbstractFontDialog::setCurrentFont(const QFont &arg)
{
    if (m_currentFont != arg) {
        m_currentFont = arg;
        emit currentFontChanged();
    }
}

void QQuickAbstractFontDialog::setScalableFonts(bool arg)
{
    m_options->setOption(QFontDialogOptions::ScalableFonts, arg);
    emit scalableFontsChanged();
}

void QQuickAbstractFontDialog::setNonScalableFonts(bool arg)
{
    m_options->setOption(QFontDialogOptions::NonScalableFonts, arg);
    emit nonScalableFontsChanged();
}

void QQuickAbstractFontDialog::setMonospacedFonts(bool arg)
{
    m_options->setOption(QFontDialogOptions::MonospacedFonts, arg);
    emit monospacedFontsChanged();
}

void QQuickAbstractFontDialog::setProportionalFonts(bool arg)
{
    m_options->setOption(QFontDialogOptions::ProportionalFonts, arg);
    emit proportionalFontsChanged();
}

void QQuickAbstractFontDialog::accept()
{
    setFont(m_currentFont);
    QQuickAbstractDialog::accept();
}

QT_END_NAMESPACE
