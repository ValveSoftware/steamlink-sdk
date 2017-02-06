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

#include "qdesigner_promotion_p.h"
#include "widgetdatabase_p.h"
#include "metadatabase_p.h"
#include "widgetdatabase_p.h"

#include <QtDesigner/QDesignerFormEditorInterface>
#include <QtDesigner/QDesignerFormWindowInterface>
#include <QtDesigner/QDesignerFormWindowManagerInterface>
#include <QtDesigner/QDesignerObjectInspectorInterface>
#include <QtDesigner/QDesignerWidgetBoxInterface>
#include <QtDesigner/QDesignerWidgetDataBaseInterface>

#include <QtCore/QMap>
#include <QtCore/QCoreApplication>
#include <qdebug.h>

QT_BEGIN_NAMESPACE

namespace {
    // Return a set of on-promotable classes
    const QSet<QString> &nonPromotableClasses() {
        static QSet<QString> rc;
        if (rc.empty()) {
            rc.insert(QStringLiteral("Line"));
            rc.insert(QStringLiteral("QAction"));
            rc.insert(QStringLiteral("Spacer"));
            rc.insert(QStringLiteral("QMainWindow"));
            rc.insert(QStringLiteral("QDialog"));
            rc.insert(QStringLiteral("QMdiArea"));
            rc.insert(QStringLiteral("QMdiSubWindow"));
        }
        return rc;
    }

    // Return widget database index of a promoted class or -1 with error message
    int promotedWidgetDataBaseIndex(const QDesignerWidgetDataBaseInterface *widgetDataBase,
                                                                 const QString &className,
                                                                 QString *errorMessage) {
        const int index = widgetDataBase->indexOfClassName(className);
        if (index == -1 || !widgetDataBase->item(index)->isPromoted()) {
            *errorMessage = QCoreApplication::tr("%1 is not a promoted class.").arg(className);
            return -1;
        }
        return index;
    }

    // Return widget database item of a promoted class or 0 with error message
    QDesignerWidgetDataBaseItemInterface *promotedWidgetDataBaseItem(const QDesignerWidgetDataBaseInterface *widgetDataBase,
                                                                     const QString &className,
                                                                     QString *errorMessage) {

        const int index =  promotedWidgetDataBaseIndex(widgetDataBase, className, errorMessage);
        if (index == -1)
            return 0;
        return widgetDataBase->item(index);
    }

    // extract class name from xml  "<widget class="QWidget" ...>". Quite a hack.
    QString classNameFromXml(QString xml) {
        static const QString tag = QStringLiteral("class=\"");
        const int pos = xml.indexOf(tag);
        if (pos == -1)
            return QString();
        xml.remove(0, pos + tag.size());
        const int closingPos = xml.indexOf(QLatin1Char('"'));
        if (closingPos == -1)
            return QString();
        xml.remove(closingPos, xml.size() - closingPos);
        return xml;
    }

    // return a list of class names in the scratch pad
    QStringList getScratchPadClasses(const QDesignerWidgetBoxInterface *wb) {
        QStringList rc;
        const int catCount =  wb->categoryCount();
        for (int c = 0; c <  catCount; c++) {
            const QDesignerWidgetBoxInterface::Category category = wb->category(c);
            if (category.type() == QDesignerWidgetBoxInterface::Category::Scratchpad) {
                const int widgetCount = category.widgetCount();
                for (int w = 0; w < widgetCount; w++) {
                    const QString className = classNameFromXml( category.widget(w).domXml());
                    if (!className.isEmpty())
                        rc += className;
                }
            }
        }
        return rc;
    }
}

static void markFormsDirty(const QDesignerFormEditorInterface *core)
{
    const QDesignerFormWindowManagerInterface *fwm = core->formWindowManager();
    for (int f = 0, count = fwm->formWindowCount(); f < count; ++f)
        fwm->formWindow(f)->setDirty(true);
}

namespace qdesigner_internal {

    QDesignerPromotion::QDesignerPromotion(QDesignerFormEditorInterface *core) :
        m_core(core)  {
    }

    bool  QDesignerPromotion::addPromotedClass(const QString &baseClass,
                                               const QString &className,
                                               const QString &includeFile,
                                               QString *errorMessage)
    {
        QDesignerWidgetDataBaseInterface *widgetDataBase = m_core->widgetDataBase();
        const int baseClassIndex = widgetDataBase->indexOfClassName(baseClass);

        if (baseClassIndex == -1) {
            *errorMessage = QCoreApplication::tr("The base class %1 is invalid.").arg(baseClass);
            return false;
        }

        const int existingClassIndex = widgetDataBase->indexOfClassName(className);

        if (existingClassIndex != -1) {
            *errorMessage = QCoreApplication::tr("The class %1 already exists.").arg(className);
            return false;
        }
        // Clone derived item.
        QDesignerWidgetDataBaseItemInterface *promotedItem = WidgetDataBaseItem::clone(widgetDataBase->item(baseClassIndex));
        // Also inherit the container flag in case of QWidget-derived classes
        // as it is most likely intended for stacked pages.
        // set new props
        promotedItem->setName(className);
        promotedItem->setGroup(QCoreApplication::tr("Promoted Widgets"));
        promotedItem->setCustom(true);
        promotedItem->setPromoted(true);
        promotedItem->setExtends(baseClass);
        promotedItem->setIncludeFile(includeFile);
        widgetDataBase->append(promotedItem);
        markFormsDirty(m_core);
        return true;
    }

    QList<QDesignerWidgetDataBaseItemInterface *> QDesignerPromotion::promotionBaseClasses() const
    {
        typedef QMap<QString, QDesignerWidgetDataBaseItemInterface *> SortedDatabaseItemMap;
        SortedDatabaseItemMap sortedDatabaseItemMap;

        QDesignerWidgetDataBaseInterface *widgetDataBase = m_core->widgetDataBase();

        const int cnt = widgetDataBase->count();
        for (int i = 0; i <  cnt; i++) {
            QDesignerWidgetDataBaseItemInterface *dbItem = widgetDataBase->item(i);
            if (canBePromoted(dbItem)) {
                sortedDatabaseItemMap.insert(dbItem->name(), dbItem);
            }
        }

        return sortedDatabaseItemMap.values();
    }


    bool QDesignerPromotion::canBePromoted(const QDesignerWidgetDataBaseItemInterface *dbItem) const
    {
        if (dbItem->isPromoted() ||  !dbItem->extends().isEmpty())
            return false;

        const QString name = dbItem->name();

        if (nonPromotableClasses().contains(name))
            return false;

        if (name.startsWith(QStringLiteral("QDesigner")) ||
            name.startsWith(QStringLiteral("QLayout")))
            return false;

        return true;
    }

    QDesignerPromotion::PromotedClasses QDesignerPromotion::promotedClasses()  const
    {
        typedef QMap<QString, QDesignerWidgetDataBaseItemInterface *> ClassNameItemMap;
        // A map containing base classes and their promoted classes.
        typedef QMap<QString, ClassNameItemMap> BaseClassPromotedMap;

        BaseClassPromotedMap baseClassPromotedMap;

        QDesignerWidgetDataBaseInterface *widgetDataBase = m_core->widgetDataBase();
        // Look for promoted classes and insert into map according to base class.
        const  int cnt = widgetDataBase->count();
        for (int i = 0; i < cnt; i++) {
            QDesignerWidgetDataBaseItemInterface *dbItem = widgetDataBase->item(i);
            if (dbItem->isPromoted()) {
                const QString baseClassName = dbItem->extends();
                BaseClassPromotedMap::iterator it = baseClassPromotedMap.find(baseClassName);
                if (it == baseClassPromotedMap.end()) {
                    it = baseClassPromotedMap.insert(baseClassName, ClassNameItemMap());
                }
                it.value().insert(dbItem->name(), dbItem);
            }
        }
        // convert map into list.
        PromotedClasses rc;

        if (baseClassPromotedMap.empty())
            return rc;

        const BaseClassPromotedMap::const_iterator bcend = baseClassPromotedMap.constEnd();
        for (BaseClassPromotedMap::const_iterator bit = baseClassPromotedMap.constBegin(); bit !=  bcend; ++bit) {
            const int baseIndex = widgetDataBase->indexOfClassName(bit.key());
            Q_ASSERT(baseIndex >= 0);
            QDesignerWidgetDataBaseItemInterface *baseItem = widgetDataBase->item(baseIndex);
            // promoted
            const ClassNameItemMap::const_iterator pcend = bit.value().constEnd();
            for (ClassNameItemMap::const_iterator pit = bit.value().constBegin(); pit != pcend; ++pit) {
                PromotedClass item;
                item.baseItem = baseItem;
                item.promotedItem = pit.value();
                rc.push_back(item);
            }
        }

        return rc;
    }

    QSet<QString> QDesignerPromotion::referencedPromotedClassNames()  const {
        QSet<QString> rc;
        const MetaDataBase *metaDataBase = qobject_cast<const MetaDataBase*>(m_core->metaDataBase());
        if (!metaDataBase)
            return rc;

        const QList<QObject*> objs = metaDataBase->objects();
        const QList<QObject*>::const_iterator cend = objs.constEnd();
        for ( QList<QObject*>::const_iterator it = objs.constBegin(); it != cend; ++it) {
            const QString customClass = metaDataBase->metaDataBaseItem(*it)->customClassName();
            if (!customClass.isEmpty())
                rc.insert(customClass);

        }
        // check the scratchpad of the widget box
        if (QDesignerWidgetBoxInterface *widgetBox = m_core->widgetBox()) {
            const QStringList scratchPadClasses = getScratchPadClasses(widgetBox);
            if (!scratchPadClasses.empty()) {
                // Check whether these are actually promoted
                QDesignerWidgetDataBaseInterface *widgetDataBase = m_core->widgetDataBase();
                QStringList::const_iterator cend = scratchPadClasses.constEnd();
                for (QStringList::const_iterator it = scratchPadClasses.constBegin(); it != cend; ++it ) {
                    const int index = widgetDataBase->indexOfClassName(*it);
                    if (index != -1 && widgetDataBase->item(index)->isPromoted())
                        rc += *it;
                }
            }
        }
        return rc;
    }

    bool QDesignerPromotion::removePromotedClass(const QString &className, QString *errorMessage) {
        // check if it exists and is promoted
        WidgetDataBase *widgetDataBase = qobject_cast<WidgetDataBase *>(m_core->widgetDataBase());
        if (!widgetDataBase) {
            *errorMessage = QCoreApplication::tr("The class %1 cannot be removed").arg(className);
            return false;
        }

        const int index = promotedWidgetDataBaseIndex(widgetDataBase, className, errorMessage);
        if (index == -1)
            return false;

        if (referencedPromotedClassNames().contains(className)) {
            *errorMessage = QCoreApplication::tr("The class %1 cannot be removed because it is still referenced.").arg(className);
            return false;
        }
        // QTBUG-52963: Check for classes that specify the to-be-removed class as
        // base class of a promoted class. This should not happen in the normal case
        // as promoted classes cannot serve as base for further promotion. It is possible
        // though if a class provided by a plugin (say Qt WebKit's QWebView) is used as
        // a base class for a promoted widget B and the plugin is removed in the next
        // launch. QWebView will then appear as promoted class itself and the promoted
        // class B will depend on it. When removing QWebView, the base class of B will
        // be changed to that of QWebView by the below code.
        const PromotedClasses promotedList = promotedClasses();
        for (PromotedClasses::const_iterator it = promotedList.constBegin(), end = promotedList.constEnd(); it != end; ++it) {
            if (it->baseItem->name() == className) {
                const QString extends = widgetDataBase->item(index)->extends();
                qWarning().nospace() << "Warning: Promoted class " << it->promotedItem->name()
                    << " extends " << className << ", changing its base class to " <<  extends << '.';
                it->promotedItem->setExtends(extends);
            }
        }
        widgetDataBase->remove(index);
        markFormsDirty(m_core);
        return true;
    }

    bool QDesignerPromotion::changePromotedClassName(const QString &oldclassName, const QString &newClassName, QString *errorMessage) {
        const MetaDataBase *metaDataBase = qobject_cast<const MetaDataBase*>(m_core->metaDataBase());
        if (!metaDataBase) {
            *errorMessage = QCoreApplication::tr("The class %1 cannot be renamed").arg(oldclassName);
            return false;
        }
        QDesignerWidgetDataBaseInterface *widgetDataBase = m_core->widgetDataBase();

        // check the new name
        if (newClassName.isEmpty()) {
            *errorMessage = QCoreApplication::tr("The class %1 cannot be renamed to an empty name.").arg(oldclassName);
            return false;
        }
        const int existingIndex = widgetDataBase->indexOfClassName(newClassName);
        if (existingIndex != -1) {
            *errorMessage = QCoreApplication::tr("There is already a class named %1.").arg(newClassName);
            return false;
        }
        // Check old class
        QDesignerWidgetDataBaseItemInterface *dbItem = promotedWidgetDataBaseItem(widgetDataBase, oldclassName, errorMessage);
        if (!dbItem)
            return false;

        // Change the name in the data base and change all referencing objects in the meta database
        dbItem->setName(newClassName);
        bool foundReferences = false;
        foreach (QObject* object, metaDataBase->objects()) {
            MetaDataBaseItem *item =  metaDataBase->metaDataBaseItem(object);
            Q_ASSERT(item);
            if (item->customClassName() == oldclassName) {
                item->setCustomClassName(newClassName);
                foundReferences = true;
            }
        }
        // set state
        if (foundReferences)
            refreshObjectInspector();

        markFormsDirty(m_core);
        return true;
    }

    bool QDesignerPromotion::setPromotedClassIncludeFile(const QString &className, const QString &includeFile, QString *errorMessage) {
        // check file
        if (includeFile.isEmpty()) {
            *errorMessage = QCoreApplication::tr("Cannot set an empty include file.");
            return false;
        }
        // check item
        QDesignerWidgetDataBaseInterface *widgetDataBase = m_core->widgetDataBase();
        QDesignerWidgetDataBaseItemInterface *dbItem = promotedWidgetDataBaseItem(widgetDataBase, className, errorMessage);
        if (!dbItem)
            return false;
        if (dbItem->includeFile() != includeFile) {
            dbItem->setIncludeFile(includeFile);
            markFormsDirty(m_core);
        }
        return true;
    }

    void QDesignerPromotion::refreshObjectInspector() {
        if (QDesignerFormWindowManagerInterface *fwm = m_core->formWindowManager()) {
            if (QDesignerFormWindowInterface *fw = fwm->activeFormWindow())
                if ( QDesignerObjectInspectorInterface *oi = m_core->objectInspector()) {
                    oi->setFormWindow(fw);
                }
        }
    }
}

QT_END_NAMESPACE
