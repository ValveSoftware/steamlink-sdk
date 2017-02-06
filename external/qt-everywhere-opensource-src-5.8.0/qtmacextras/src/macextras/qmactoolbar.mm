/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtMacExtras module of the Qt Toolkit.
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
#import <AppKit/AppKit.h>
#include "qmactoolbar.h"
#include "qmactoolbar_p.h"

#include <QtCore/QDebug>
#include <QtCore/QTimer>
#include <QtCore/QUuid>
#include <QtCore/QString>
#include <QtCore/qdebug.h>

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QtGui/QGuiApplication>
#include <qpa/qplatformnativeinterface.h>
#else
#include <QtGui/QMainWindow>
#endif

#include "qmacfunctions.h"
#include "qmacfunctions_p.h"
#include "qmactoolbaritem_p.h"
#include "qmactoolbardelegate_p.h"
#include "qnstoolbar_p.h"

QT_BEGIN_NAMESPACE

/*!
  \class QMacToolBar
  \inmodule QtMacExtras
  \since 5.3
  \brief The QMacToolBar class wraps the native NSToolbar class.

  QMacToolBar provides a Qt-based API for NSToolBar. The toolbar displays one or
  more \e items. Each toolbar item has an icon and a text label.

  The toolbar must be attached to a QWindow using the
  \l{QMacToolBar::attachToWindow()}{attachToWindow()} method in order to be visible.
  The toolbar is attached to the native NSWindow and is displayed above the
  QWindow. QMacToolBar visibility follows window visibility.

  Add items by calling addItem(). The toolbar has a customization menu which
  is available to the user from the toolbar context menu. Use addAllowedItem() to
  add items to the customization menu.

  Usage: (QtWidgets)
  \code
    QMacToolBar *toolBar = new QMacToolBar(this);
    QMacToolBarItem *toolBarItem = toolBar->addItem(QIcon(), QStringLiteral("foo"));
    connect(toolBarItem, SIGNAL(activated()), this, SLOT(fooClicked()));

    this->window()->winId(); // create window->windowhandle()
    toolBar->attachToWindow(this->window()->windowHandle());
  \endcode

  \sa QMacToolBarItem
*/

/*!
    Constructs a QMacToolBar with the given \a parent
*/
QMacToolBar::QMacToolBar(QObject *parent)
    : QObject(*new QMacToolBarPrivate(), parent)
{
}

/*!
    Constructs a QMacToolBar with the given \a identifier and \a parent. The identifier is used
    to uniquely identify the toolbar within the appliation, for example when autosaving the
    toolbar configuration.
*/
QMacToolBar::QMacToolBar(const QString &identifier, QObject *parent)
    : QObject(*new QMacToolBarPrivate(identifier), parent)
{
}

/*!
    Destroys the toolbar.
*/
QMacToolBar::~QMacToolBar()
{
}

/*!
    Add a toolbar item with \a icon and \a text.
*/
QMacToolBarItem *QMacToolBar::addItem(const QIcon &icon, const QString &text)
{
    Q_D(QMacToolBar);
    QMacToolBarItem *item = new QMacToolBarItem(this);
    item->setText(text);
    item->setIcon(icon);
    d->items.append(item);
    d->allowedItems.append(item);
    return item;
}

/*!
    Add a toolbar separator item.
*/
void QMacToolBar::addSeparator()
{
    addStandardItem(QMacToolBarItem::Space); // No Seprator on OS X.
}

/*!
    Add a toolbar standard item.
    \omitvalue standardItem
    \internal
*/
QMacToolBarItem *QMacToolBar::addStandardItem(QMacToolBarItem::StandardItem standardItem)
{
    Q_D(QMacToolBar);
    QMacToolBarItem *item = new QMacToolBarItem(this);
    item->setStandardItem(standardItem);
    d->items.append(item);
    d->allowedItems.append(item);
    return item;
}

/*!
    Add atoolbar item with \a icon and \a text to the toolbar customization menu.
*/
QMacToolBarItem *QMacToolBar::addAllowedItem(const QIcon &icon, const QString &text)
{
    Q_D(QMacToolBar);
    QMacToolBarItem *item = new QMacToolBarItem(this);
    item->setText(text);
    item->setIcon(icon);
    d->allowedItems.append(item);
    return item;
}

/*!
    Add a standard toolbar item to the toolbar customization menu.
    \omitvalue standardItem
    \internal
*/
QMacToolBarItem *QMacToolBar::addAllowedStandardItem(QMacToolBarItem::StandardItem standardItem)
{
    Q_D(QMacToolBar);
    QMacToolBarItem *item = new QMacToolBarItem(this);
    item->setStandardItem(standardItem);
    d->allowedItems.append(item);
    return item;
}

/*!
    Sets the list of the default toolbar \a items.
*/
void QMacToolBar::setItems(QList<QMacToolBarItem *> &items)
{
    Q_D(QMacToolBar);
    d->items = items;
}

/*!
    Returns the list of the default toolbar items.
*/
QList<QMacToolBarItem *> QMacToolBar::items()
{
    Q_D(QMacToolBar);
    return d->items;
}

/*!
    Sets the list of toolbar items shown on the toolbar customization menu to \a allowedItems.
*/
void QMacToolBar::setAllowedItems(QList<QMacToolBarItem *> &allowedItems)
{
    Q_D(QMacToolBar);
    d->allowedItems = allowedItems;
}

/*!
    Returns the list oftoolbar items shown on the the toolbar customization menu.
*/
QList<QMacToolBarItem *> QMacToolBar::allowedItems()
{
    Q_D(QMacToolBar);
    return d->allowedItems;
}

/*!
    Attaches the toolbar to \a window. The toolbar will be displayed at the top of the
    native window, under and attached to the title bar above the QWindow. The toolbar is displayed
    outside the QWidnow area.

    Use QWidget::windowHandle() to get a QWindow pointer from a QWidget instance. At app startup
    the QWindow might not have been created yet, call QWidget::winId() to make sure it is.
*/
void QMacToolBar::attachToWindow(QWindow *window)
{
    Q_D(QMacToolBar);
    if (!window) {
        detachFromWindow();
        return;
    }

    QPlatformNativeInterface::NativeResourceForIntegrationFunction function = resolvePlatformFunction("setNSToolbar");
    if (function) {
        typedef void (*SetNSToolbarFunction)(QWindow *window, void *nsToolbar);
        reinterpret_cast<SetNSToolbarFunction>(function)(window, d->toolbar);
    } else {
        d->targetWindow = window;
        QTimer::singleShot(100, this, SLOT(showInWindow_impl())); // ### hackety hack
    }
}

/*!
    \internal
*/
void QMacToolBar::showInWindow_impl()
{
    Q_D(QMacToolBar);
    if (!d->targetWindow) {
        QTimer::singleShot(100, this, SLOT(showInWindow_impl()));
        return;
    }

    NSWindow *macWindow = static_cast<NSWindow*>(
        QGuiApplication::platformNativeInterface()->nativeResourceForWindow("nswindow", d->targetWindow));

    if (!macWindow) {
        QTimer::singleShot(100, this, SLOT(showInWindow_impl()));
        return;
    }

    [macWindow setToolbar: d->toolbar];
    [macWindow setShowsToolbarButton:YES];
}

/*!
    Detatches the toolbar from the current window.
*/
void QMacToolBar::detachFromWindow()
{
    Q_D(QMacToolBar);
    if (!d->targetWindow)
        return;

    NSWindow *macWindow = static_cast<NSWindow*>(
        QGuiApplication::platformNativeInterface()->nativeResourceForWindow("nswindow", d->targetWindow));
    [macWindow setToolbar:nil];
}

/*!
    Returns the naitve NSTooolbar object.
*/
NSToolbar *QMacToolBar::nativeToolbar() const
{
    Q_D(const QMacToolBar);
    return d->toolbar;
}

QMacToolBarPrivate::QMacToolBarPrivate(const QString &identifier)
{
    QString ident = identifier.isEmpty() ? QUuid::createUuid().toString() : identifier;
    toolbar = [[NSToolbar alloc] initWithIdentifier:ident.toNSString()];
    [toolbar setAutosavesConfiguration:NO];
    [toolbar setAllowsUserCustomization:YES];

    QMacToolbarDelegate *delegate = [[QMacToolbarDelegate alloc] init];
    delegate->toolbarPrivate = this;
    [toolbar setDelegate:delegate];

    targetWindow = 0;
}

QMacToolBarPrivate::~QMacToolBarPrivate()
{
    [[toolbar delegate]release];
    [toolbar release];
}

NSMutableArray *QMacToolBarPrivate::getItemIdentifiers(const QList<QMacToolBarItem *> &items, bool cullUnselectable)
{
    NSMutableArray *array = [[NSMutableArray alloc] init];
    for (const QMacToolBarItem * item : items) {
        if (cullUnselectable && item->selectable() == false)
            continue;
        [array addObject : item->d_func()->itemIdentifier()];
    }
    return array;
}

void QMacToolBarPrivate::itemClicked(NSToolbarItem *item)
{
    QString identifier = QString::fromNSString([item itemIdentifier]);
    QMacToolBarItem *toolButton = reinterpret_cast<QMacToolBarItem *>(identifier.toULongLong());
    Q_EMIT toolButton->activated();
}


QT_END_NAMESPACE
