/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Designer of the Qt Toolkit.
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

#include "promotionmodel_p.h"
#include "widgetdatabase_p.h"

#include <QtDesigner/QDesignerWidgetDataBaseInterface>
#include <QtDesigner/QDesignerPromotionInterface>
#include <QtDesigner/QDesignerFormEditorInterface>

#include <QtGui/QStandardItem>
#include <QtCore/QCoreApplication>

QT_BEGIN_NAMESPACE

namespace {
    typedef QList<QStandardItem *> StandardItemList;

    // Model columns.
    enum { ClassNameColumn, IncludeFileColumn, IncludeTypeColumn, ReferencedColumn, NumColumns };

    // Create a model row.
    StandardItemList modelRow() {
        StandardItemList rc;
        for (int i = 0; i < NumColumns; i++) {
            rc.push_back(new QStandardItem());
        }
        return rc;
    }

    // Create a model row for a base class (read-only, cannot be selected).
    StandardItemList baseModelRow(const QDesignerWidgetDataBaseItemInterface *dbItem) {
        StandardItemList rc =  modelRow();

        rc[ClassNameColumn]->setText(dbItem->name());
        for (int i = 0; i < NumColumns; i++) {
            rc[i]->setFlags(Qt::ItemIsEnabled);
        }
        return rc;
    }

    // Create an editable model row for a promoted class.
    StandardItemList promotedModelRow(QDesignerWidgetDataBaseItemInterface *baseItem,
                                      QDesignerWidgetDataBaseItemInterface *dbItem,
                                      bool referenced)
    {
        qdesigner_internal::PromotionModel::ModelData data;
        data.baseItem = baseItem;
        data.promotedItem = dbItem;
        data.referenced = referenced;

        const QVariant userData = qVariantFromValue(data);

        StandardItemList rc =  modelRow();
        // name
        rc[ClassNameColumn]->setText(dbItem->name());
        rc[ClassNameColumn]->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled|Qt::ItemIsEditable);
        rc[ClassNameColumn]->setData(userData);
        // header
        const qdesigner_internal::IncludeSpecification spec = qdesigner_internal::includeSpecification(dbItem->includeFile());
        rc[IncludeFileColumn]->setText(spec.first);
        rc[IncludeFileColumn]->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled|Qt::ItemIsEditable);
        rc[IncludeFileColumn]->setData(userData);
        // global include
        rc[IncludeTypeColumn]->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled|Qt::ItemIsEditable|Qt::ItemIsUserCheckable);
        rc[IncludeTypeColumn]->setData(userData);
        rc[IncludeTypeColumn]->setCheckState(spec.second == qdesigner_internal::IncludeGlobal ? Qt::Checked : Qt::Unchecked);
        // referenced
        rc[ReferencedColumn]->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
        rc[ClassNameColumn]->setData(userData);
        if (!referenced) {
            //: Usage of promoted widgets
            static const QString notUsed = QCoreApplication::translate("PromotionModel", "Not used");
            rc[ReferencedColumn]->setText(notUsed);
        }
        return rc;
    }
}

namespace qdesigner_internal {

    PromotionModel::PromotionModel(QDesignerFormEditorInterface *core) :
        m_core(core)
    {
        connect(this, &QStandardItemModel::itemChanged, this, &PromotionModel::slotItemChanged);
    }

    void PromotionModel::initializeHeaders() {
        setColumnCount(NumColumns);
        QStringList horizontalLabels(tr("Name"));
        horizontalLabels += tr("Header file");
        horizontalLabels += tr("Global include");
        horizontalLabels += tr("Usage");
        setHorizontalHeaderLabels (horizontalLabels);
    }

    void PromotionModel::updateFromWidgetDatabase() {
        typedef QDesignerPromotionInterface::PromotedClasses PromotedClasses;

        clear();
        initializeHeaders();

        // retrieve list of pairs from DB and convert into a tree structure.
        // Set the item index as user data on the item.
        const PromotedClasses promotedClasses = m_core->promotion()->promotedClasses();

        if (promotedClasses.empty())
            return;

        const QSet<QString> usedPromotedClasses = m_core->promotion()->referencedPromotedClassNames();

        QDesignerWidgetDataBaseItemInterface *baseClass = 0;
        QStandardItem *baseItem = 0;

        const PromotedClasses::const_iterator bcend = promotedClasses.constEnd();
        for (PromotedClasses::const_iterator it = promotedClasses.constBegin(); it !=  bcend; ++it) {
            // Start a new base class?
            if (baseClass !=  it->baseItem) {
                baseClass =  it->baseItem;
                const StandardItemList baseRow = baseModelRow(it->baseItem);
                baseItem = baseRow.front();
                appendRow(baseRow);
            }
            Q_ASSERT(baseItem);
            // Append derived
            baseItem->appendRow(promotedModelRow(it->baseItem, it->promotedItem, usedPromotedClasses.contains(it->promotedItem->name())));
        }
    }

    void PromotionModel::slotItemChanged(QStandardItem * changedItem) {
        // Retrieve DB item
        const ModelData data = modelData(changedItem);
        Q_ASSERT(data.isValid());
        QDesignerWidgetDataBaseItemInterface *dbItem = data.promotedItem;
        // Change header or type
        switch (changedItem->column()) {
        case ClassNameColumn:
            emit classNameChanged(dbItem,  changedItem->text());
            break;
        case IncludeTypeColumn:
        case IncludeFileColumn: {
            // Get both file and type items via parent.
            const QStandardItem *baseClassItem = changedItem->parent();
            const QStandardItem *fileItem = baseClassItem->child(changedItem->row(), IncludeFileColumn);
            const QStandardItem *typeItem =  baseClassItem->child(changedItem->row(), IncludeTypeColumn);
            emit includeFileChanged(dbItem, buildIncludeFile(fileItem->text(), typeItem->checkState() == Qt::Checked ? IncludeGlobal : IncludeLocal));
        }
            break;
        }
    }

    PromotionModel::ModelData PromotionModel::modelData(const QStandardItem *item) const
    {
        const QVariant userData = item->data();
        return userData.canConvert<ModelData>() ? userData.value<ModelData>() : ModelData();
    }

    PromotionModel::ModelData PromotionModel::modelData(const QModelIndex &index) const
    {
        return index.isValid() ? modelData(itemFromIndex(index)) : ModelData();
    }

    QModelIndex PromotionModel::indexOfClass(const QString &className) const {
        const StandardItemList matches = findItems (className, Qt::MatchFixedString|Qt::MatchCaseSensitive|Qt::MatchRecursive);
        return matches.empty() ? QModelIndex() : indexFromItem (matches.front());
    }
} // namespace qdesigner_internal

QT_END_NAMESPACE
