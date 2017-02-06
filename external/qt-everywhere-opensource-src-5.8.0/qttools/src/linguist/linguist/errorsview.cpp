/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Linguist of the Qt Toolkit.
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

#include "errorsview.h"

#include "messagemodel.h"

#include <QtCore/QList>
#include <QtCore/QString>
#include <QtCore/QUrl>

#include <QtGui/QStandardItem>
#include <QtGui/QStandardItemModel>
#include <QtWidgets/QListView>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

ErrorsView::ErrorsView(MultiDataModel *dataModel, QWidget *parent) :
    QListView(parent),
    m_dataModel(dataModel)
{
    m_list = new QStandardItemModel(this);
    setModel(m_list);
}

void ErrorsView::clear()
{
    m_list->clear();
}

void ErrorsView::addError(int model, const ErrorType type, const QString &arg)
{
    QString error;
    switch (type) {
      case SuperfluousAccelerator:
        addError(model, tr("Accelerator possibly superfluous in translation."));
        break;
      case MissingAccelerator:
        addError(model, tr("Accelerator possibly missing in translation."));
        break;
      case PunctuationDiffer:
        addError(model, tr("Translation does not end with the same punctuation as the source text."));
        break;
      case IgnoredPhrasebook:
        addError(model, tr("A phrase book suggestion for '%1' was ignored.").arg(arg));
        break;
      case PlaceMarkersDiffer:
        addError(model, tr("Translation does not refer to the same place markers as in the source text."));
        break;
      case NumerusMarkerMissing:
        addError(model, tr("Translation does not contain the necessary %n/%Ln place marker."));
        break;
      default:
        addError(model, tr("Unknown error"));
        break;
    }
}

QString ErrorsView::firstError()
{
    return (m_list->rowCount() == 0) ? QString() : m_list->item(0)->text();
}

void ErrorsView::addError(int model, const QString &error)
{
    // NOTE: Three statics instead of one just for GCC 3.3.5
    static QLatin1String imageLocation(":/images/s_check_danger.png");
    static QPixmap image(imageLocation);
    static QIcon pxDanger(image);
    QString lang;
    if (m_dataModel->modelCount() > 1)
        lang = m_dataModel->model(model)->localizedLanguage() + QLatin1String(": ");
    QStandardItem *item = new QStandardItem(pxDanger, lang + error);
    item->setEditable(false);
    m_list->appendRow(QList<QStandardItem*>() << item);
}

QT_END_NAMESPACE
