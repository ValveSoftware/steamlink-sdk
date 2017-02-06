/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Assistant module of the Qt Toolkit.
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
#include "openpagesmodel.h"

#include "helpenginewrapper.h"
#include "helpviewer.h"
#include "tracer.h"

#include <QtCore/QStringList>
#include <QtCore/QUrl>

QT_BEGIN_NAMESPACE

OpenPagesModel::OpenPagesModel(QObject *parent) : QAbstractTableModel(parent)
{
    TRACE_OBJ
}

int OpenPagesModel::rowCount(const QModelIndex &parent) const
{
    TRACE_OBJ
    return  parent.isValid() ? 0 : m_pages.count();
}

int OpenPagesModel::columnCount(const QModelIndex &/*parent*/) const
{
    TRACE_OBJ
    return 2;
}

QVariant OpenPagesModel::data(const QModelIndex &index, int role) const
{
    TRACE_OBJ
    if (!index.isValid() || index.row() >= rowCount() || index.column() > 0
        || role != Qt::DisplayRole)
        return QVariant();
    QString title = m_pages.at(index.row())->title();
    title.replace(QLatin1Char('&'), QLatin1String("&&"));
    return title.isEmpty() ? QLatin1String("(Untitled)") : title;
}

void OpenPagesModel::addPage(const QUrl &url, qreal zoom)
{
    TRACE_OBJ
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    HelpViewer *page = new HelpViewer(zoom);
    connect(page, SIGNAL(titleChanged()), this, SLOT(handleTitleChanged()));
    m_pages << page;
    endInsertRows();
    page->setSource(url);
}

void OpenPagesModel::removePage(int index)
{
    TRACE_OBJ
    Q_ASSERT(index >= 0 && index < rowCount());
    beginRemoveRows(QModelIndex(), index, index);
    HelpViewer *page = m_pages.at(index);
    m_pages.removeAt(index);
    endRemoveRows();
    page->deleteLater();
}

HelpViewer *OpenPagesModel::pageAt(int index) const
{
    TRACE_OBJ
    Q_ASSERT(index >= 0 && index < rowCount());
    return m_pages.at(index);
}

void OpenPagesModel::handleTitleChanged()
{
    TRACE_OBJ
    HelpViewer *page = static_cast<HelpViewer *>(sender());
    const int row = m_pages.indexOf(page);
    Q_ASSERT(row != -1 );
    const QModelIndex &item = index(row, 0);
    emit dataChanged(item, item);
}

QT_END_NAMESPACE
