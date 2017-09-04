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

#include "qwidgetplatformfiledialog_p.h"
#include "qwidgetplatformdialog_p.h"

#include <QtWidgets/qfiledialog.h>

QT_BEGIN_NAMESPACE

QWidgetPlatformFileDialog::QWidgetPlatformFileDialog(QObject *parent)
    : m_dialog(new QFileDialog)
{
    setParent(parent);

    connect(m_dialog.data(), &QDialog::accepted, this, &QPlatformDialogHelper::accept);
    connect(m_dialog.data(), &QDialog::rejected, this, &QPlatformDialogHelper::reject);

    connect(m_dialog.data(), &QFileDialog::fileSelected, [this](const QString &file) {
        emit fileSelected(QUrl::fromLocalFile(file));
    });
    connect(m_dialog.data(), &QFileDialog::filesSelected, [this](const QList<QString> &files) {
        QList<QUrl> urls;
        urls.reserve(files.count());
        for (const QString &file : files)
            urls += QUrl::fromLocalFile(file);
        emit filesSelected(urls);
    });
    connect(m_dialog.data(), &QFileDialog::currentChanged, [this](const QString &path) {
        emit currentChanged(QUrl::fromLocalFile(path));
    });
    connect(m_dialog.data(), &QFileDialog::directoryEntered, this, &QWidgetPlatformFileDialog::directoryEntered);
    connect(m_dialog.data(), &QFileDialog::filterSelected, this, &QWidgetPlatformFileDialog::filterSelected);
}

QWidgetPlatformFileDialog::~QWidgetPlatformFileDialog()
{
}

bool QWidgetPlatformFileDialog::defaultNameFilterDisables() const
{
    return false; // TODO: ?
}

void QWidgetPlatformFileDialog::setDirectory(const QUrl &directory)
{
    m_dialog->setDirectory(directory.toLocalFile());
}

QUrl QWidgetPlatformFileDialog::directory() const
{
    return m_dialog->directoryUrl();
}

void QWidgetPlatformFileDialog::selectFile(const QUrl &filename)
{
    m_dialog->selectUrl(filename);
}

QList<QUrl> QWidgetPlatformFileDialog::selectedFiles() const
{
    return m_dialog->selectedUrls();
}

void QWidgetPlatformFileDialog::setFilter()
{
    // TODO: ?
}

void QWidgetPlatformFileDialog::selectNameFilter(const QString &filter)
{
    m_dialog->selectNameFilter(filter);
}

QString QWidgetPlatformFileDialog::selectedNameFilter() const
{
    return m_dialog->selectedNameFilter();
}

void QWidgetPlatformFileDialog::exec()
{
    m_dialog->exec();
}

bool QWidgetPlatformFileDialog::show(Qt::WindowFlags flags, Qt::WindowModality modality, QWindow *parent)
{
    QSharedPointer<QFileDialogOptions> options = QPlatformFileDialogHelper::options();
    m_dialog->setWindowTitle(options->windowTitle());
    m_dialog->setAcceptMode(static_cast<QFileDialog::AcceptMode>(options->acceptMode()));
    m_dialog->setFileMode(static_cast<QFileDialog::FileMode>(options->fileMode()));
    m_dialog->setOptions(static_cast<QFileDialog::Options>(int(options->options())) | QFileDialog::DontUseNativeDialog);
    m_dialog->setNameFilters(options->nameFilters());
    m_dialog->setDefaultSuffix(options->defaultSuffix());
    if (options->isLabelExplicitlySet(QFileDialogOptions::Accept))
        m_dialog->setLabelText(QFileDialog::Accept, options->labelText(QFileDialogOptions::Accept));
    if (options->isLabelExplicitlySet(QFileDialogOptions::Reject))
        m_dialog->setLabelText(QFileDialog::Reject, options->labelText(QFileDialogOptions::Reject));

    return QWidgetPlatformDialog::show(m_dialog.data(), flags, modality, parent);
}

void QWidgetPlatformFileDialog::hide()
{
    m_dialog->hide();
}

QT_END_NAMESPACE
