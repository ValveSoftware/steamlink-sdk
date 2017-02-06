/****************************************************************************
**
** Copyright (C) 2016 BlackBerry Limited. All rights reserved.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
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

#include <QtCore/QFileSelector>
#include <QtQml/QQmlAbstractUrlInterceptor>
#include <qobjectdefs.h>
#include "qqmlfileselector.h"
#include "qqmlfileselector_p.h"
#include <QDebug>

QT_BEGIN_NAMESPACE

typedef QHash<QQmlAbstractUrlInterceptor*, QQmlFileSelector*> interceptorSelectorMap;
Q_GLOBAL_STATIC(interceptorSelectorMap, interceptorInstances);
/*!
   \class QQmlFileSelector
   \since 5.2
   \inmodule QtQml
   \brief A class for applying a QFileSelector to QML file loading

  QQmlFileSelector will automatically apply a QFileSelector to
  qml file and asset paths.

  It is used as follows:

  \code
  QQmlEngine engine;
  QQmlFileSelector* selector = new QQmlFileSelector(&engine);
  \endcode

  Then you can swap out files like so:
  \code
  main.qml
  Component.qml
  asset.png
  +unix/Component.qml
  +mac/asset.png
  \endcode

  In this example, main.qml will normally use Component.qml for the Component type. However on a
  unix platform, the unix selector will be present and the +unix/Component.qml version will be
  used instead. Note that this acts like swapping out Component.qml with +unix/Component.qml, so
  when using Component.qml you should not need to alter any paths based on which version was
  selected.

  For example, to pass the "asset.png" file path around you would refer to it just as "asset.png" in
  all of main.qml, Component.qml, and +linux/Component.qml. It will be replaced with +mac/asset.png
  on Mac platforms in all cases.

  For a list of available selectors, see \c QFileSelector.

  Your platform may also provide additional selectors for you to use. As specified by QFileSelector,
  directories used for selection must start with a '+' character, so you will not accidentally
  trigger this feature unless you have directories with such names inside your project.

  If a new QQmlFileSelector is set on the engine, the old one will be replaced. Use
  \l QQmlFileSelector::get() to query or use the existing instance.
 */

/*!
  Creates a new QQmlFileSelector with parent object \a parent, which includes
  its own QFileSelector. \a engine is the QQmlEngine you wish to apply file
  selectors to. It will also take ownership of the QQmlFileSelector.
*/

QQmlFileSelector::QQmlFileSelector(QQmlEngine* engine, QObject* parent)
    : QObject(*(new QQmlFileSelectorPrivate), parent)
{
    Q_D(QQmlFileSelector);
    d->engine = engine;
    interceptorInstances()->insert(d->myInstance.data(), this);
    d->engine->setUrlInterceptor(d->myInstance.data());
}

/*!
   Destroys the QQmlFileSelector object.
*/
QQmlFileSelector::~QQmlFileSelector()
{
    Q_D(QQmlFileSelector);
    if (d->engine && QQmlFileSelector::get(d->engine) == this) {
        d->engine->setUrlInterceptor(0);
        d->engine = 0;
    }
    interceptorInstances()->remove(d->myInstance.data());
}

/*!
  \since 5.7
  Returns the QFileSelector instance used by the QQmlFileSelector.
*/
QFileSelector *QQmlFileSelector::selector() const Q_DECL_NOTHROW
{
    Q_D(const QQmlFileSelector);
    return d->selector;
}

QQmlFileSelectorPrivate::QQmlFileSelectorPrivate()
{
    Q_Q(QQmlFileSelector);
    ownSelector = true;
    selector = new QFileSelector(q);
    myInstance.reset(new QQmlFileSelectorInterceptor(this));
}

QQmlFileSelectorPrivate::~QQmlFileSelectorPrivate()
{
    if (ownSelector)
        delete selector;
}

/*!
  Sets the QFileSelector instance for use by the QQmlFileSelector to \a selector.
  QQmlFileSelector does not take ownership of the new QFileSelector. To reset QQmlFileSelector
  to use its internal QFileSelector instance, call setSelector(0).
*/

void QQmlFileSelector::setSelector(QFileSelector *selector)
{
    Q_D(QQmlFileSelector);
    if (selector) {
        if (d->ownSelector) {
            delete d->selector;
            d->ownSelector = false;
        }
        d->selector = selector;
    } else {
        if (!d->ownSelector) {
            d->ownSelector = true;
            d->selector = new QFileSelector(this);
        } // Do nothing if already using internal selector
    }
}

/*!
  Adds extra selectors contained in \a strings to the current QFileSelector being used.
  Use this when extra selectors are all you need to avoid having to create your own
  QFileSelector instance.
*/
void QQmlFileSelector::setExtraSelectors(QStringList &strings)
{
    Q_D(QQmlFileSelector);
    d->selector->setExtraSelectors(strings);
}


/*!
  Adds extra selectors contained in \a strings to the current QFileSelector being used.
  Use this when extra selectors are all you need to avoid having to create your own
  QFileSelector instance.
*/
void QQmlFileSelector::setExtraSelectors(const QStringList &strings)
{
    Q_D(QQmlFileSelector);
    d->selector->setExtraSelectors(strings);
}

/*!
  Gets the QQmlFileSelector currently active on the target \a engine.
*/
QQmlFileSelector* QQmlFileSelector::get(QQmlEngine* engine)
{
    //Since I think we still can't use dynamic_cast inside Qt...
    QQmlAbstractUrlInterceptor* current = engine->urlInterceptor();
    if (current && interceptorInstances()->contains(current))
        return interceptorInstances()->value(current);
    return 0;
}

/*!
  \internal
*/
QQmlFileSelectorInterceptor::QQmlFileSelectorInterceptor(QQmlFileSelectorPrivate* pd)
    : d(pd)
{
}

/*!
  \internal
*/
QUrl QQmlFileSelectorInterceptor::intercept(const QUrl &path, DataType type)
{
    if ( type ==  QQmlAbstractUrlInterceptor::QmldirFile ) //Don't intercept qmldir files, to prevent double interception
        return path;
    return d->selector->select(path);
}

QT_END_NAMESPACE
