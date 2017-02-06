/****************************************************************************
 **
 ** Copyright (C) 2016 Ivan Vizir <define-true-false@yandex.com>
 ** Copyright (C) 2016 The Qt Company Ltd.
 ** Contact: https://www.qt.io/licensing/
 **
 ** This file is part of the QtWinExtras module of the Qt Toolkit.
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

#include <QtCore/QtGlobal>

#ifdef Q_CC_MINGW // MinGW: Include the correct definition of SHARDAPPIDINFOLINK
#  if defined(NTDDI_VERSION) && NTDDI_VERSION < 0x06010000
#     undef NTDDI_VERSION
#  endif
#  ifndef NTDDI_VERSION
#    define NTDDI_VERSION 0x06010000
#  endif
#endif // Q_CC_MINGW

#include "qwinjumplistcategory.h"
#include "qwinjumplistcategory_p.h"
#include "qwinjumplistitem_p.h"
#include "qwinfunctions_p.h"
#include "qwinjumplist_p.h"
#include "winshobjidl_p.h"
#include "windowsguidsdefs_p.h"

#include <QtCore/QDebug>

#include <shlobj.h>

QT_BEGIN_NAMESPACE

/*!
    \class QWinJumpListCategory
    \inmodule QtWinExtras
    \since 5.2
    \brief The QWinJumpListCategory class represents a jump list category.
 */

/*!
    \enum QWinJumpListCategory::Type

    This enum describes the available QWinJumpListCategory types.

    \value  Custom
            A custom jump list category.
    \value  Recent
            A jump list category of "recent" items.
    \value  Frequent
            A jump list category of "frequent" items.
    \value  Tasks
            A jump list category of tasks.
 */

QWinJumpListCategoryPrivate::QWinJumpListCategoryPrivate() :
    visible(false), jumpList(0), type(QWinJumpListCategory::Custom)
{
}

QWinJumpListCategory *QWinJumpListCategoryPrivate::create(QWinJumpListCategory::Type type, QWinJumpList *jumpList)
{
    QWinJumpListCategory *category = new QWinJumpListCategory;
    category->d_func()->type = type;
    category->d_func()->jumpList = jumpList;
    if (type == QWinJumpListCategory::Recent || type == QWinJumpListCategory::Frequent)
        category->d_func()->loadRecents();
    return category;
}

void QWinJumpListCategoryPrivate::invalidate()
{
    if (jumpList)
        QWinJumpListPrivate::get(jumpList)->invalidate();
}

void QWinJumpListCategoryPrivate::loadRecents()
{
    Q_ASSERT(jumpList);
    IApplicationDocumentLists *pDocList = 0;
    HRESULT hresult = CoCreateInstance(qCLSID_ApplicationDocumentLists, 0, CLSCTX_INPROC_SERVER, qIID_IApplicationDocumentLists, reinterpret_cast<void **>(&pDocList));
    if (SUCCEEDED(hresult)) {
        if (!jumpList->identifier().isEmpty()) {
            wchar_t *id = qt_qstringToNullTerminated(jumpList->identifier());
            hresult = pDocList->SetAppID(id);
            delete[] id;
        }
        if (SUCCEEDED(hresult)) {
            IObjectArray *array = 0;
            hresult = pDocList->GetList(type == QWinJumpListCategory::Recent ? ADLT_RECENT : ADLT_FREQUENT,
                                        0, qIID_IObjectArray, reinterpret_cast<void **>(&array));
            if (SUCCEEDED(hresult)) {
                items = QWinJumpListPrivate::fromComCollection(array);
                array->Release();
            }
        }
        pDocList->Release();
    }
    if (FAILED(hresult))
        QWinJumpListPrivate::warning("loadRecents", hresult);
}

void QWinJumpListCategoryPrivate::addRecent(QWinJumpListItem *item)
{
    Q_ASSERT(item->type() == QWinJumpListItem::Link);
    wchar_t *id = 0;
    if (jumpList && !jumpList->identifier().isEmpty())
        id = qt_qstringToNullTerminated(jumpList->identifier());

    SHARDAPPIDINFOLINK info;
    info.pszAppID = id;
    info.psl =  QWinJumpListPrivate::toIShellLink(item);
    if (info.psl) {
        SHAddToRecentDocs(SHARD_APPIDINFOLINK, &info);
        info.psl->Release();
    }
    delete[] id;
}

void QWinJumpListCategoryPrivate::clearRecents()
{
    IApplicationDestinations *pDest = 0;
    HRESULT hresult = CoCreateInstance(qCLSID_ApplicationDestinations, 0, CLSCTX_INPROC_SERVER, qIID_IApplicationDestinations, reinterpret_cast<void **>(&pDest));
    if (SUCCEEDED(hresult)) {
        const QString identifier = jumpList ? jumpList->identifier() : QString();
        if (!identifier.isEmpty()) {
            wchar_t *id = qt_qstringToNullTerminated(identifier);
            hresult = pDest->SetAppID(id);
            delete[] id;
        }
        hresult = pDest->RemoveAllDestinations();
        pDest->Release();
    }
    if (FAILED(hresult))
        QWinJumpListPrivate::warning("clearRecents", hresult);
}

/*!
    Constructs a custom QWinJumpListCategory with the specified \a title.
 */
QWinJumpListCategory::QWinJumpListCategory(const QString &title) :
    d_ptr(new QWinJumpListCategoryPrivate)
{
    d_ptr->title = title;
}

/*!
    Destroys the QWinJumpListCategory.
 */
QWinJumpListCategory::~QWinJumpListCategory()
{
    Q_D(QWinJumpListCategory);
    qDeleteAll(d->items);
    d->items.clear();
}

/*!
    Returns the category type.
 */
QWinJumpListCategory::Type QWinJumpListCategory::type() const
{
    Q_D(const QWinJumpListCategory);
    return d->type;
}

/*!
    Returns whether the category is visible.
 */
bool QWinJumpListCategory::isVisible() const
{
    Q_D(const QWinJumpListCategory);
    return d->visible;
}

/*!
    Sets the category \a visible.
 */
void QWinJumpListCategory::setVisible(bool visible)
{
    Q_D(QWinJumpListCategory);
    if (d->visible != visible) {
        d->visible = visible;
        d->invalidate();
    }
}

/*!
    Returns the category title.
 */
QString QWinJumpListCategory::title() const
{
    Q_D(const QWinJumpListCategory);
    return d->title;
}

/*!
    Sets the category \a title.
 */
void QWinJumpListCategory::setTitle(const QString &title)
{
    Q_D(QWinJumpListCategory);
    if (d->title != title) {
        d->title = title;
        d->invalidate();
    }
}

/*!
    Returns the amount of items in the category.
 */
int QWinJumpListCategory::count() const
{
    Q_D(const QWinJumpListCategory);
    return d->items.count();
}

/*!
    Returns whether the category is empty.
 */
bool QWinJumpListCategory::isEmpty() const
{
    Q_D(const QWinJumpListCategory);
    return d->items.isEmpty();
}

/*!
    Returns the list of items in the category.
 */
QList<QWinJumpListItem *> QWinJumpListCategory::items() const
{
    Q_D(const QWinJumpListCategory);
    return d->items;
}

/*!
    Adds an \a item to the category.
 */
void QWinJumpListCategory::addItem(QWinJumpListItem *item)
{
    Q_D(QWinJumpListCategory);
    if (!item)
        return;

    if (d->type == Recent || d->type == Frequent) {
        if (item->type() == QWinJumpListItem::Separator) {
            qWarning("QWinJumpListCategory::addItem(): only tasks/custom categories support separators.");
            return;
        }
        if (item->type() == QWinJumpListItem::Destination) {
            qWarning("QWinJumpListCategory::addItem(): only tasks/custom categories support destinations.");
            return;
        }
    }

    QWinJumpListItemPrivate *p = QWinJumpListItemPrivate::get(item);
    if (p->category != this) {
        p->category = this;
        d->items.append(item);
        if (d->type == QWinJumpListCategory::Recent || d->type == QWinJumpListCategory::Frequent)
            d->addRecent(item);
        d->invalidate();
    }
}

/*!
    Adds a destination to the category pointing to \a filePath.
 */
QWinJumpListItem *QWinJumpListCategory::addDestination(const QString &filePath)
{
    QWinJumpListItem *item = new QWinJumpListItem(QWinJumpListItem::Destination);
    item->setFilePath(filePath);
    addItem(item);
    return item;
}

/*!
    Adds a link to the category using \a title, \a executablePath, and
    optionally \a arguments.
 */
QWinJumpListItem *QWinJumpListCategory::addLink(const QString &title, const QString &executablePath, const QStringList &arguments)
{
    return addLink(QIcon(), title, executablePath, arguments);
}

/*!
    \overload addLink()

    Adds a link to the category using \a icon, \a title, \a executablePath,
    and optionally \a arguments.
 */
QWinJumpListItem *QWinJumpListCategory::addLink(const QIcon &icon, const QString &title, const QString &executablePath, const QStringList &arguments)
{
    QWinJumpListItem *item = new QWinJumpListItem(QWinJumpListItem::Link);
    item->setFilePath(executablePath);
    item->setTitle(title);
    item->setArguments(arguments);
    item->setIcon(icon);
    addItem(item);
    return item;
}

/*!
    Adds a separator to the category.

    \note Only tasks category supports separators.
 */
QWinJumpListItem *QWinJumpListCategory::addSeparator()
{
    QWinJumpListItem *item = new QWinJumpListItem(QWinJumpListItem::Separator);
    addItem(item);
    return item;
}

/*!
    Clears the category.
 */
void QWinJumpListCategory::clear()
{
    Q_D(QWinJumpListCategory);
    if (!d->items.isEmpty()) {
        qDeleteAll(d->items);
        d->items.clear();
        if (d->type == QWinJumpListCategory::Recent || d->type == QWinJumpListCategory::Frequent)
            d->clearRecents();
        d->invalidate();
    }
}

#ifndef QT_NO_DEBUG_STREAM

QDebug operator<<(QDebug debug, const QWinJumpListCategory *category)
{
    QDebugStateSaver saver(debug);
    debug.nospace();
    debug.noquote();
    debug << "QWinJumpListCategory(";
    if (category) {
        debug << "type=" << category->type() << ", isVisible="
            << category->isVisible() << ", title=\"" << category->title()
            << "\", items=" << category->items();
    } else {
        debug << '0';
    }
    debug << ')';
    return debug;


    return debug;
}

#endif // !QT_NO_DEBUG_STREAM

QT_END_NAMESPACE
