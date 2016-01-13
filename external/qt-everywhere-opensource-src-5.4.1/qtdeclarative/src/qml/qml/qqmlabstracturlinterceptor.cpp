/****************************************************************************
**
** Copyright (C) 2013 Research In Motion.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtQml module of the Qt Toolkit.
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

/*!
 \class QQmlAbstractUrlInterceptor
 \inmodule QtQml
 \brief allows you to control QML file loading.

 QQmlAbstractUrlInterceptor is an interface which can be used to alter URLs
 before they are used by the QML engine. This is primarily useful for altering
 file urls into other file urls, such as selecting different graphical assets
 for the current platform.

 Relative URLs are intercepted after being resolved against the file path of the
 current QML context. URL interception also occurs after setting the base path for
 a loaded QML file. This means that the content loaded for that QML file uses the
 intercepted URL, but inside the file the pre-intercepted URL is used for resolving
 relative paths. This allows for interception of .qml file loading without needing
 all paths (or local types) inside intercepted content to insert a different relative path.

 Compared to setNetworkAccessManagerFactory, QQmlAbstractUrlInterceptor affects all URLs
 and paths, including local files and embedded resource files. QQmlAbstractUrlInterceptor
 is synchronous, and for asynchronous files must return a url with an asynchronous scheme
 (such as http or a custom scheme handled by your own custom QNetworkAccessManager). You
 can use a QQmlAbstractUrlInterceptor to change file URLs into networked URLs which are
 handled by your own custom QNetworkAccessManager.

 To implement support for a custom networked scheme, see setNetworkAccessManagerFactory.
*/

/*
 \enum QQmlAbstractUrlInterceptor::DataType

 Specifies where URL interception is taking place.

 Because QML loads qmldir files for locating types, there are two URLs involved in loading a QML type. The URL of the (possibly implicit) qmldir used for locating the type and the URL of the file which defines the type. Intercepting
 both leads to either complex URL replacement or double URL replacements for the same file.

 \value QmldirFile The URL being intercepted is for a Qmldir file. Intercepting this, but not the QmlFile, allows for swapping out entire sub trees.
 \value JavaScriptFile The URL being intercepted is an import for a Javascript file.
 \value QmlFile The URL being intercepted is for a Qml file. Intercepting this, but not the Qmldir file, leaves the base dir of a QML file untouched and acts like replacing the file with another file.
 \value UrlString The URL being intercepted is a url property in a QML file, and not being used to load a file through the engine.

*/

/*!
 \fn QUrl QQmlAbstractUrlInterceptor::intercept(const QUrl& url, DataType type)

 A pure virtual function where you can intercept the \a url. The returned value is taken as the
 new value for the url. The type of url being intercepted is given by the \a type variable.

 Your implementation of this function must be thread-safe, as it can be called from multiple threads
 at the same time.
*/
