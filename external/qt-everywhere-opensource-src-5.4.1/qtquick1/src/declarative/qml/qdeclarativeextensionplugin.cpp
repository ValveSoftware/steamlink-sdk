/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtDeclarative module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qdeclarativeextensionplugin.h"

QT_BEGIN_NAMESPACE

/*!
    \since 4.7
    \class QDeclarativeExtensionPlugin
    \brief The QDeclarativeExtensionPlugin class provides an abstract base for custom QML extension plugins.

    \ingroup plugins

    QDeclarativeExtensionPlugin is a plugin interface that makes it possible to
    create QML extensions that can be loaded dynamically into QML applications.
    These extensions allow custom QML types to be made available to the QML engine.

    To write a QML extension plugin:

    \list
    \li Subclass QDeclarativeExtensionPlugin, implement registerTypes() method
    to register types using qmlRegisterType(), and export the class using the Q_PLUGIN_METADATA() macro
    \li Write an appropriate project file for the plugin
    \li Create a \l{Writing a qmldir file}{qmldir file} to describe the plugin
    \endlist

    QML extension plugins can be used to provide either application-specific or
    library-like plugins. Library plugins should limit themselves to registering types,
    as any manipulation of the engine's root context may cause conflicts
    or other issues in the library user's code.


    \section1 An example

    Suppose there is a new \c TimeModel C++ class that should be made available
    as a new QML element. It provides the current time through \c hour and \c minute
    properties, like this:

    \snippet examples/declarative/cppextensions/plugins/plugin.cpp 0
    \dots

    To make this class available as a QML type, create a plugin that registers
    this type with a specific \l {QML Modules}{module} using qmlRegisterType(). For this example the plugin
    module will be named \c com.nokia.TimeExample (as defined in the project
    file further below).

    \snippet examples/declarative/cppextensions/plugins/plugin.cpp plugin
    \codeline
    \snippet examples/declarative/cppextensions/plugins/plugin.cpp export

    This registers the \c TimeModel class with the 1.0 version of this
    plugin library, as a QML type called \c Time. The Q_ASSERT statement
    ensures the module is imported correctly by any QML components that use this plugin.

    The project file defines the project as a plugin library and specifies
    it should be built into the \c com/nokia/TimeExample directory:

    \code
    TEMPLATE = lib
    CONFIG += qt plugin
    QT += declarative

    DESTDIR = com/nokia/TimeExample
    TARGET = qmlqtimeexampleplugin
    ...
    \endcode

    Finally, a \l{Writing a qmldir file}{qmldir file} is required in the \c com/nokia/TimeExample directory
    that describes the plugin. This directory includes a \c Clock.qml file that
    should be bundled with the plugin, so it needs to be specified in the \c qmldir
    file:

    \quotefile examples/declarative/cppextensions/plugins/com/nokia/TimeExample/qmldir

    Once the project is built and installed, the new \c Time element can be
    used by any QML component that imports the \c com.nokia.TimeExample module:

    \snippet examples/declarative/cppextensions/plugins/plugins.qml 0

    The full source code is available in the \l {declarative/cppextensions/plugins}{plugins example}.

    The \l {Tutorial: Writing QML extensions with C++} also contains a chapter
    on creating QML plugins.

    \sa QDeclarativeEngine::importPlugin(), {How to Create Qt Plugins}
*/

/*!
    \fn void QDeclarativeExtensionPlugin::registerTypes(const char *uri)

    Registers the QML types in the given \a uri. Subclasses should implement
    this to call qmlRegisterType() for all types which are provided by the extension
    plugin.

    The \a uri is an identifier for the plugin generated by the QML engine
    based on the name and path of the extension's plugin library.
*/

/*!
    Constructs a QML extension plugin with the given \a parent.

    Note that this constructor is invoked automatically by the
    plugin loader, so there is no need for calling it
    explicitly.
*/
QDeclarativeExtensionPlugin::QDeclarativeExtensionPlugin(QObject *parent)
    : QObject(parent)
{
}

/*!
  \internal
 */
QDeclarativeExtensionPlugin::~QDeclarativeExtensionPlugin()
{
}

/*!
    \fn void QDeclarativeExtensionPlugin::initializeEngine(QDeclarativeEngine *engine, const char *uri)

    Initializes the extension from the \a uri using the \a engine. Here an application
    plugin might, for example, expose some data or objects to QML,
    as context properties on the engine's root context.
*/

void QDeclarativeExtensionPlugin::initializeEngine(QDeclarativeEngine *engine, const char *uri)
{
    Q_UNUSED(engine);
    Q_UNUSED(uri);
}

QT_END_NAMESPACE
