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

#ifdef Q_CC_MINGW // MinGW: Enable SHCreateItemFromParsingName()
#  if defined(_WIN32_IE) && _WIN32_IE << 0x0700 // _WIN32_IE_IE70
#     undef _WIN32_IE
#  endif
#  ifndef _WIN32_IE
#    define _WIN32_IE 0x0700
#  endif
#endif // Q_CC_MINGW

#include "qwinjumplist.h"
#include "qwinjumplist_p.h"
#include "qwinjumplistitem.h"
#include "qwinjumplistcategory.h"
#include "qwinjumplistcategory_p.h"
#include "windowsguidsdefs_p.h"
#include "winpropkey_p.h"

#include <QDir>
#include <QtCore/QDebug>
#include <QCoreApplication>
#include <qt_windows.h>
#include <propvarutil.h>

#include "qwinfunctions.h"
#include "qwinfunctions_p.h"
#include "winpropkey_p.h"

#include <shobjidl.h>

QT_BEGIN_NAMESPACE

/*!
    \class QWinJumpList
    \inmodule QtWinExtras
    \brief The QWinJumpList class represents a transparent wrapper around Windows
    Jump Lists.

    \since 5.2

    An application can use Jump Lists to provide users with faster access to
    files or to display shortcuts to tasks or commands.
 */

/*!
    \title Application User Model IDs
    \externalpage http://msdn.microsoft.com/en-us/library/windows/desktop/dd378459%28v=vs.85%29.aspx
 */

// partial copy of qprocess_win.cpp:qt_create_commandline()
static QString createArguments(const QStringList &arguments)
{
    QString args;
    for (int i=0; i<arguments.size(); ++i) {
        QString tmp = arguments.at(i);
        // Quotes are escaped and their preceding backslashes are doubled.
        tmp.replace(QRegExp(QLatin1String("(\\\\*)\"")), QLatin1String("\\1\\1\\\""));
        if (tmp.isEmpty() || tmp.contains(QLatin1Char(' ')) || tmp.contains(QLatin1Char('\t'))) {
            // The argument must not end with a \ since this would be interpreted
            // as escaping the quote -- rather put the \ behind the quote: e.g.
            // rather use "foo"\ than "foo\"
            int i = tmp.length();
            while (i > 0 && tmp.at(i - 1) == QLatin1Char('\\'))
                --i;
            tmp.insert(i, QLatin1Char('"'));
            tmp.prepend(QLatin1Char('"'));
        }
        args += QLatin1Char(' ') + tmp;
    }
    return args;
}

QWinJumpListPrivate::QWinJumpListPrivate() :
    pDestList(0), recent(0), frequent(0), tasks(0), dirty(false)
{
}

void QWinJumpListPrivate::warning(const char *function, HRESULT hresult)
{
    const QString err = QtWin::errorStringFromHresult(hresult);
    qWarning("QWinJumpList: %s() failed: %#010x, %s.", function, unsigned(hresult), qPrintable(err));
}

QString QWinJumpListPrivate::iconsDirPath()
{
    QString iconDirPath = QDir::tempPath() + QLatin1Char('/') + QCoreApplication::instance()->applicationName() + QLatin1String("/qt-jl-icons/");
    QDir().mkpath(iconDirPath);
    return iconDirPath;
}

void QWinJumpListPrivate::invalidate()
{
    Q_Q(QWinJumpList);
    if (!pDestList)
        return;

    if (!dirty) {
        dirty = true;
        QMetaObject::invokeMethod(q, "_q_rebuild", Qt::QueuedConnection);
    }
}

void QWinJumpListPrivate::_q_rebuild()
{
    if (beginList()) {
        if (recent && recent->isVisible())
            appendKnownCategory(KDC_RECENT);
        if (frequent && frequent->isVisible())
            appendKnownCategory(KDC_FREQUENT);
        for (QWinJumpListCategory *category : qAsConst(categories)) {
            if (category->isVisible())
                appendCustomCategory(category);
        }
        if (tasks && tasks->isVisible())
            appendTasks(tasks->items());
        commitList();
    }
    dirty = false;
}

void QWinJumpListPrivate::destroy()
{
    delete recent;
    recent = 0;
    delete frequent;
    frequent = 0;
    delete tasks;
    tasks = 0;
    qDeleteAll(categories);
    categories.clear();
    invalidate();
}

bool QWinJumpListPrivate::beginList()
{
    HRESULT hresult = S_OK;
    if (!identifier.isEmpty()) {
        wchar_t *id = qt_qstringToNullTerminated(identifier);
        hresult = pDestList->SetAppID(id);
        delete[] id;
    }
    if (SUCCEEDED(hresult)) {
        UINT maxSlots = 0;
        IUnknown *array = 0;
        hresult = pDestList->BeginList(&maxSlots, qIID_IUnknown, reinterpret_cast<void **>(&array));
        if (array)
            array->Release();
    }
    if (FAILED(hresult))
        QWinJumpListPrivate::warning("BeginList", hresult);
    return SUCCEEDED(hresult);
}

bool QWinJumpListPrivate::commitList()
{
    HRESULT hresult = pDestList->CommitList();
    if (FAILED(hresult))
        QWinJumpListPrivate::warning("CommitList", hresult);
    return SUCCEEDED(hresult);
}

void QWinJumpListPrivate::appendKnownCategory(KNOWNDESTCATEGORY category)
{
    HRESULT hresult = pDestList->AppendKnownCategory(category);
    if (FAILED(hresult))
        QWinJumpListPrivate::warning("AppendKnownCategory", hresult);
}

void QWinJumpListPrivate::appendCustomCategory(QWinJumpListCategory *category)
{
    IObjectCollection *collection = toComCollection(category->items());
    if (collection) {
        wchar_t *title = qt_qstringToNullTerminated(category->title());
        HRESULT hresult = pDestList->AppendCategory(title, collection);
        if (FAILED(hresult))
            QWinJumpListPrivate::warning("AppendCategory", hresult);
        delete[] title;
        collection->Release();
    }
}

void QWinJumpListPrivate::appendTasks(const QList<QWinJumpListItem *> &items)
{
    IObjectCollection *collection = toComCollection(items);
    if (collection) {
        HRESULT hresult = pDestList->AddUserTasks(collection);
        if (FAILED(hresult))
            QWinJumpListPrivate::warning("AddUserTasks", hresult);
        collection->Release();
    }
}

QList<QWinJumpListItem *> QWinJumpListPrivate::fromComCollection(IObjectArray *array)
{
    QList<QWinJumpListItem *> list;
    UINT count = 0;
    array->GetCount(&count);
    for (UINT i = 0; i < count; ++i) {
        IUnknown *collectionItem = 0;
        HRESULT hresult = array->GetAt(i, qIID_IUnknown, reinterpret_cast<void **>(&collectionItem));
        if (FAILED(hresult)) {
            QWinJumpListPrivate::warning("GetAt", hresult);
            continue;
        }
        IShellItem2 *shellItem = 0;
        IShellLinkW *shellLink = 0;
        QWinJumpListItem *jumplistItem = 0;
        if (SUCCEEDED(collectionItem->QueryInterface(qIID_IShellItem2, reinterpret_cast<void **>(&shellItem)))) {
            jumplistItem = fromIShellItem(shellItem);
            shellItem->Release();
        } else if (SUCCEEDED(collectionItem->QueryInterface(qIID_IShellLinkW, reinterpret_cast<void **>(&shellLink)))) {
            jumplistItem = fromIShellLink(shellLink);
            shellLink->Release();
        } else {
            qWarning("QWinJumpList: object of unexpected class found");
        }
        collectionItem->Release();
        if (jumplistItem)
            list.append(jumplistItem);
    }
    return list;
}

IObjectCollection *QWinJumpListPrivate::toComCollection(const QList<QWinJumpListItem *> &list)
{
    if (list.isEmpty())
        return 0;
    IObjectCollection *collection = 0;
    HRESULT hresult = CoCreateInstance(qCLSID_EnumerableObjectCollection, 0, CLSCTX_INPROC_SERVER, qIID_IObjectCollection, reinterpret_cast<void **>(&collection));
    if (FAILED(hresult)) {
        QWinJumpListPrivate::warning("QWinJumpList: failed to instantiate IObjectCollection", hresult);
        return 0;
    }
    for (QWinJumpListItem *item : list) {
        IUnknown *iitem = toICustomDestinationListItem(item);
        if (iitem) {
            collection->AddObject(iitem);
            iitem->Release();
        }
    }
    return collection;
}

QWinJumpListItem *QWinJumpListPrivate::fromIShellLink(IShellLinkW *link)
{
    QWinJumpListItem *item = new QWinJumpListItem(QWinJumpListItem::Link);

    IPropertyStore *linkProps;
    link->QueryInterface(qIID_IPropertyStore, reinterpret_cast<void **>(&linkProps));
    PROPVARIANT var;
    linkProps->GetValue(qPKEY_Link_Arguments, &var);
    item->setArguments(QStringList(QString::fromWCharArray(var.pwszVal)));
    PropVariantClear(&var);
    linkProps->Release();

    const int buffersize = 2048;
    wchar_t buffer[buffersize];

    link->GetDescription(buffer, INFOTIPSIZE);
    item->setDescription(QString::fromWCharArray(buffer));

    int dummyindex;
    link->GetIconLocation(buffer, buffersize-1, &dummyindex);
    item->setIcon(QIcon(QString::fromWCharArray(buffer)));

    link->GetPath(buffer, buffersize-1, 0, 0);
    item->setFilePath(QDir::fromNativeSeparators(QString::fromWCharArray(buffer)));

    return item;
}

QWinJumpListItem *QWinJumpListPrivate::fromIShellItem(IShellItem2 *shellitem)
{
    QWinJumpListItem *item = new QWinJumpListItem(QWinJumpListItem::Destination);
    wchar_t *strPtr;
    shellitem->GetDisplayName(SIGDN_FILESYSPATH, &strPtr);
    item->setFilePath(QDir::fromNativeSeparators(QString::fromWCharArray(strPtr)));
    CoTaskMemFree(strPtr);
    return item;
}

IUnknown *QWinJumpListPrivate::toICustomDestinationListItem(const QWinJumpListItem *item)
{
    switch (item->type()) {
    case QWinJumpListItem::Destination :
        return toIShellItem(item);
    case QWinJumpListItem::Link :
        return toIShellLink(item);
    case QWinJumpListItem::Separator :
        return makeSeparatorShellItem();
    default:
        return 0;
    }
}

IShellLinkW *QWinJumpListPrivate::toIShellLink(const QWinJumpListItem *item)
{
    IShellLinkW *link = 0;
    HRESULT hresult = CoCreateInstance(CLSID_ShellLink, 0, CLSCTX_INPROC_SERVER, qIID_IShellLinkW, reinterpret_cast<void **>(&link));
    if (FAILED(hresult)) {
        QWinJumpListPrivate::warning("QWinJumpList: failed to instantiate IShellLinkW", hresult);
        return 0;
    }

    const QString args = createArguments(item->arguments());
    const int iconPathSize = QWinJumpListPrivate::iconsDirPath().size()
        + int(sizeof(void *)) * 2 + 4; // path + ptr-name-in-hex + .ico
    const int bufferSize = qMax(args.size(),
                                qMax(item->workingDirectory().size(),
                                     qMax(item->description().size(),
                                          qMax(item->title().size(),
                                               qMax(item->filePath().size(), iconPathSize))))) + 1;
    wchar_t *buffer = new wchar_t[bufferSize];

    if (!item->description().isEmpty()) {
        qt_qstringToNullTerminated(item->description(), buffer);
        link->SetDescription(buffer);
    }

    qt_qstringToNullTerminated(item->filePath(), buffer);
    link->SetPath(buffer);

    if (!item->workingDirectory().isEmpty()) {
        qt_qstringToNullTerminated(item->workingDirectory(), buffer);
        link->SetWorkingDirectory(buffer);
    }

    qt_qstringToNullTerminated(args, buffer);
    link->SetArguments(buffer);

    if (!item->icon().isNull()) {
        QString iconPath = QWinJumpListPrivate::iconsDirPath() + QString::number(reinterpret_cast<quintptr>(item), 16) + QLatin1String(".ico");
        bool iconSaved = item->icon().pixmap(GetSystemMetrics(SM_CXICON)).save(iconPath, "ico");
        if (iconSaved) {
            qt_qstringToNullTerminated(iconPath, buffer);
            link->SetIconLocation(buffer, 0);
        }
    }

    IPropertyStore *properties;
    PROPVARIANT titlepv;
    hresult = link->QueryInterface(qIID_IPropertyStore, reinterpret_cast<void **>(&properties));
    if (FAILED(hresult)) {
        link->Release();
        return 0;
    }

    qt_qstringToNullTerminated(item->title(), buffer);
    InitPropVariantFromString(buffer, &titlepv);
    properties->SetValue(qPKEY_Title, titlepv);
    properties->Commit();
    properties->Release();
    PropVariantClear(&titlepv);

    delete[] buffer;
    return link;
}

IShellItem2 *QWinJumpListPrivate::toIShellItem(const QWinJumpListItem *item)
{
    IShellItem2 *shellitem = 0;
    QScopedArrayPointer<wchar_t> buffer(qt_qstringToNullTerminated(item->filePath()));
    SHCreateItemFromParsingName(buffer.data(), 0, qIID_IShellItem2, reinterpret_cast<void **>(&shellitem));
    return shellitem;
}

IShellLinkW *QWinJumpListPrivate::makeSeparatorShellItem()
{
    IShellLinkW *separator;
    HRESULT res = CoCreateInstance(CLSID_ShellLink, 0, CLSCTX_INPROC_SERVER, qIID_IShellLinkW, reinterpret_cast<void **>(&separator));
    if (FAILED(res))
        return 0;

    IPropertyStore *properties;
    res = separator->QueryInterface(qIID_IPropertyStore, reinterpret_cast<void **>(&properties));
    if (FAILED(res)) {
        separator->Release();
        return 0;
    }

    PROPVARIANT isSeparator;
    InitPropVariantFromBoolean(TRUE, &isSeparator);
    properties->SetValue(qPKEY_AppUserModel_IsDestListSeparator, isSeparator);
    properties->Commit();
    properties->Release();
    PropVariantClear(&isSeparator);

    return separator;
}

/*!
    Constructs a QWinJumpList with the parent object \a parent.
 */
QWinJumpList::QWinJumpList(QObject *parent) :
    QObject(parent), d_ptr(new QWinJumpListPrivate)
{
    Q_D(QWinJumpList);
    d->q_ptr = this;
    HRESULT hresult = CoCreateInstance(qCLSID_DestinationList, 0, CLSCTX_INPROC_SERVER, qIID_ICustomDestinationList, reinterpret_cast<void **>(&d_ptr->pDestList));
    if (FAILED(hresult))
        QWinJumpListPrivate::warning("CoCreateInstance", hresult);
    d->invalidate();
}

/*!
    Destroys the QWinJumpList.
 */
QWinJumpList::~QWinJumpList()
{
    Q_D(QWinJumpList);
    if (d->dirty)
        d->_q_rebuild();
    if (d->pDestList) {
        d->pDestList->Release();
        d->pDestList = 0;
    }
    d->destroy();
}

/*!
    \property QWinJumpList::identifier
    \brief the jump list identifier

    Specifies an optional explicit unique identifier for the
    application jump list.

    The default value is empty; a system-defined internal identifier
    is used instead. See \l {Application User Model IDs} on MSDN for
    further details.

    \note The identifier cannot have more than \c 128 characters and
    cannot contain spaces. A too long identifier is automatically truncated
    to \c 128 characters, and spaces are replaced by underscores.
 */
QString QWinJumpList::identifier() const
{
    Q_D(const QWinJumpList);
    return d->identifier;
}

void QWinJumpList::setIdentifier(const QString &identifier)
{
    Q_D(QWinJumpList);
    QString id = identifier;
    id.replace(QLatin1Char(' '), QLatin1Char('_'));
    if (id.size() > 128)
        id.truncate(128);
    if (d->identifier != id) {
        d->identifier = id;
        d->invalidate();
    }
}

/*!
    Returns the recent items category in the jump list.
 */
QWinJumpListCategory *QWinJumpList::recent() const
{
    Q_D(const QWinJumpList);
    if (!d->recent) {
        QWinJumpList *that = const_cast<QWinJumpList *>(this);
        that->d_func()->recent = QWinJumpListCategoryPrivate::create(QWinJumpListCategory::Recent, that);
    }
    return d->recent;
}

/*!
    Returns the frequent items category in the jump list.
 */
QWinJumpListCategory *QWinJumpList::frequent() const
{
    Q_D(const QWinJumpList);
    if (!d->frequent) {
        QWinJumpList *that = const_cast<QWinJumpList *>(this);
        that->d_func()->frequent = QWinJumpListCategoryPrivate::create(QWinJumpListCategory::Frequent, that);
    }
    return d->frequent;
}

/*!
    Returns the tasks category in the jump list.
 */
QWinJumpListCategory *QWinJumpList::tasks() const
{
    Q_D(const QWinJumpList);
    if (!d->tasks) {
        QWinJumpList *that = const_cast<QWinJumpList *>(this);
        that->d_func()->tasks = QWinJumpListCategoryPrivate::create(QWinJumpListCategory::Tasks, that);
    }
    return d->tasks;
}

/*!
    Returns the custom categories in the jump list.
 */
QList<QWinJumpListCategory *> QWinJumpList::categories() const
{
    Q_D(const QWinJumpList);
    return d->categories;
}

/*!
    Adds a custom \a category to the jump list.
 */
void QWinJumpList::addCategory(QWinJumpListCategory *category)
{
    Q_D(QWinJumpList);
    if (!category)
        return;

    QWinJumpListCategoryPrivate::get(category)->jumpList = this;
    d->categories.append(category);
    d->invalidate();
}

/*!
    \overload addCategory()
    Creates a custom category with provided \a title and optional \a items,
    and adds it to the jump list.
 */
QWinJumpListCategory *QWinJumpList::addCategory(const QString &title, const QList<QWinJumpListItem *> items)
{
    QWinJumpListCategory *category = new QWinJumpListCategory(title);
    for (QWinJumpListItem *item : items)
        category->addItem(item);
    addCategory(category);
    return category;
}

/*!
    Clears the jump list.

    \sa QWinJumpListCategory::clear()
 */
void QWinJumpList::clear()
{
    Q_D(QWinJumpList);
    recent()->clear();
    frequent()->clear();
    if (d->tasks)
        d->tasks->clear();
    for (QWinJumpListCategory *category : qAsConst(d->categories))
        category->clear();
    d->destroy();
}

#ifndef QT_NO_DEBUG_STREAM

QDebug operator<<(QDebug debug, const QWinJumpList *jumplist)
{
    QDebugStateSaver saver(debug);
    debug.nospace();
    debug.noquote();
    debug << "QWinJumpList(";
    if (jumplist) {
        debug << "(identifier=\"" << jumplist->identifier() << "\", recent="
            << jumplist->recent() << ", frequent=" << jumplist->frequent()
            << ", tasks=" << jumplist->tasks()
            << ", categories=" << jumplist->categories();
    } else {
        debug << '0';
    }
    debug << ')';
    return debug;
}
#endif // !QT_NO_DEBUG_STREAM

QT_END_NAMESPACE

#include "moc_qwinjumplist.cpp"
