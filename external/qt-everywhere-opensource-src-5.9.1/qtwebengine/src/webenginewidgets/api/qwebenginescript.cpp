/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtWebEngine module of the Qt Toolkit.
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

#include "qwebenginescript.h"

#include "user_script.h"
#include <QtCore/QDebug>

using QtWebEngineCore::UserScript;

/*!
    \class QWebEngineScript
    \inmodule QtWebEngineWidgets
    \since 5.5
    \brief The QWebEngineScript class encapsulates a JavaScript program.


    QWebEngineScript enables the programmatic injection of so called \e {user scripts} in the
    JavaScript engine at different points, determined by injectionPoint(), during the loading of web
    contents.

    Scripts can be executed either in the main JavaScript \e world, along with the rest of the
    JavaScript coming from the web contents, or in their own isolated world. While the DOM of the
    page can be accessed from any world, JavaScript variables of a function defined in one world are
    not accessible from a different one. ScriptWorldId provides some predefined IDs for this
    purpose.

    The following \l Greasemonkey attributes are supported since Qt 5.8:
    \c @exclude, \c @include, \c @name, \c @match, and \c @run-at.

    Use QWebEnginePage::scripts() and QWebEngineProfile::scripts() to access
    the collection of scripts associated with a single page or a
    number of pages sharing the same profile.
*/
/*!
    \enum QWebEngineScript::InjectionPoint

    This enum describes the timing of the script injection:

    \value DocumentCreation The script will be executed as soon as the document is created. This is not suitable for
    any DOM operation.
    \value DocumentReady The script will run as soon as the DOM is ready. This is equivalent to the
    \c DOMContentLoaded event firing in JavaScript.
    \value Deferred The script will run when the page load finishes, or 500ms after the document is ready, whichever
    comes first.

*/
/*!
    \enum QWebEngineScript::ScriptWorldId

    This enum provides pre-defined world IDs for isolating user scripts into different worlds:

    \value MainWorld The world used by the page's web contents. It can be useful in order to expose custom functionality
    to web contents in certain scenarios.
    \value ApplicationWorld The default isolated world used for application level functionality implemented in JavaScript.
    \value UserWorld The first isolated world to be used by scripts set by users if the application is not making use
    of more worlds. As a rule of thumb, if that functionality is exposed to the application users, each individual script
    should probably get its own isolated world.

*/

/*!
    \fn QWebEngineScript::operator!=(const QWebEngineScript &other) const

    Returns \c true if the script is not equal to \a other, otherwise returns \c false.
*/

/*!
    \fn QWebEngineScript::swap(QWebEngineScript &other)

     Swaps the contents of the script with the contents of \a other.
*/

/*!
 * Constructs a null script.
 */

QWebEngineScript::QWebEngineScript()
    : d(new UserScript)
{
}

/*!
 * Constructs a user script using the contents of \a other.
 */
QWebEngineScript::QWebEngineScript(const QWebEngineScript &other)
    : d(other.d)
{
}

/*!
    Destroys a script.
*/
QWebEngineScript::~QWebEngineScript()
{
}

/*!
    Assigns \a other to the script.
*/
QWebEngineScript &QWebEngineScript::operator=(const QWebEngineScript &other)
{
    d = other.d;
    return *this;
}

/*!
    Returns \c true is the script is null; otherwise returns \c false.
*/
bool QWebEngineScript::isNull() const
{
    return d->isNull();
}

/*!
 * Returns the name of the script. Can be useful to retrieve a particular script from a
 * QWebEngineScriptCollection.
 *
 * \sa QWebEngineScriptCollection::findScript(), QWebEngineScriptCollection::findScripts()
 */

QString QWebEngineScript::name() const
{
    return d->name();
}

/*!
 * Sets the script name to \a scriptName.
 */
void QWebEngineScript::setName(const QString &scriptName)
{
    if (scriptName == name())
        return;
    d->setName(scriptName);
}

/*!
    Returns the source of the script.
 */
QString QWebEngineScript::sourceCode() const
{
    return d->sourceCode();
}

/*!
 * Sets the script source to \a scriptSource.
 */
void QWebEngineScript::setSourceCode(const QString &scriptSource)
{
    if (scriptSource == sourceCode())
        return;
    d->setSourceCode(scriptSource);
}

ASSERT_ENUMS_MATCH(QWebEngineScript::Deferred, UserScript::AfterLoad)
ASSERT_ENUMS_MATCH(QWebEngineScript::DocumentReady, UserScript::DocumentLoadFinished)
ASSERT_ENUMS_MATCH(QWebEngineScript::DocumentCreation, UserScript::DocumentElementCreation)

/*!
 * Returns the point in the loading process at which the script will be executed.
 * The default value is QWebEngineScript::Deferred.
 *
 * \sa setInjectionPoint()
 */
QWebEngineScript::InjectionPoint QWebEngineScript::injectionPoint() const
{
    return static_cast<QWebEngineScript::InjectionPoint>(d->injectionPoint());
}
/*!
 * Sets the point at which to execute the script to be \a p.
 *
 * \sa InjectionPoint
 */
void QWebEngineScript::setInjectionPoint(QWebEngineScript::InjectionPoint p)
{
    if (p == injectionPoint())
        return;
    d->setInjectionPoint(static_cast<UserScript::InjectionPoint>(p));
}

/*!
    Returns the world ID defining which world the script is executed in.
 */
quint32 QWebEngineScript::worldId() const
{
    return d->worldId();
}

/*!
    Sets the world ID of the isolated world to \a id when running this script.
 */
void QWebEngineScript::setWorldId(quint32 id)
{
    if (id == d->worldId())
        return;
    d->setWorldId(id);
}

/*!
    Returns \c true if the script is executed on every frame in the page, or \c false if it is only
    ran for the main frame.
 */
bool QWebEngineScript::runsOnSubFrames() const
{
    return d->runsOnSubFrames();
}

/*!
 * Executes the script on sub frames in addition to the main frame if \a on returns \c true.
 */
void QWebEngineScript::setRunsOnSubFrames(bool on)
{
    if (runsOnSubFrames() == on)
        return;
    d->setRunsOnSubFrames(on);
}

/*!
    Returns \c true if the script is equal to \a other, otherwise returns \c false.
 */
bool QWebEngineScript::operator==(const QWebEngineScript &other) const
{
    return d == other.d || *d == *(other.d);
}

QWebEngineScript::QWebEngineScript(const UserScript &coreScript)
    : d(new UserScript(coreScript))
{
}

#ifndef QT_NO_DEBUG_STREAM
QDebug operator<<(QDebug d, const QWebEngineScript &script)
{
    if (script.isNull())
        return d.maybeSpace() << "QWebEngineScript()";

    d.nospace() << "QWebEngineScript(" << script.name() << ", ";
    switch (script.injectionPoint()) {
    case QWebEngineScript::DocumentCreation:
        d << "QWebEngineScript::DocumentCreation" << ", ";
        break;
    case QWebEngineScript::DocumentReady:
        d << "QWebEngineScript::DocumentReady" << ", ";
        break;
    case QWebEngineScript::Deferred:
        d << "QWebEngineScript::Deferred" << ", ";
        break;
    }
    d << script.worldId() << ", "
      << script.runsOnSubFrames() << ", " << script.sourceCode() << ")";
    return d.space();
}
#endif
