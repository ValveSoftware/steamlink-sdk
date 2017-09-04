/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include <qqmlextensionplugin.h>
#include <qqmlengine.h>
#include <sharedimageprovider.h>


/*!
    \qmlmodule Qt.labs.sharedimage 1
    \title Qt Quick Shared Image Provider
    \ingroup qmlmodules
    \brief Adds an image provider which utilizes shared CPU memory

    \section2 Summary

    This module provides functionality to save memory in use cases where
    several Qt Quick applications use the same local image files. It does this
    by placing the decoded QImage data in shared system memory, making it
    accessible to all the processes (see QSharedMemory).

    This module only shares CPU memory. It does not provide sharing of GPU
    memory or textures.

    \section2 Usage

    To use this module, import it like this:
    \code
    import Qt.labs.sharedimage 1.0
    \endcode

    The sharing functionality is provided through a QQuickImageProvider. Use
    the "image:" scheme for the URL source of the image, followed by the
    identifier \e shared, followed by the image file path. For example:

    \code
    Image { source: "image://shared/usr/share/wallpapers/mybackground.jpg" }
    \endcode

    This will look for the file \e /usr/share/wallpapers/mybackground.jpg.
    The first process that does this will read the image file
    using normal Qt image loading. The decoded image data will then be placed
    in shared memory, using the full file path as key. Later processes
    requesting the same image will discover that the data is already available
    in shared memory. They will then use that instead of loading the image file
    again.

    The shared image data will be kept available until the last process has deleted
    its last reference to the shared image, at which point it is automatically released.

    If system memory sharing is not available, the shared image provider falls
    back to normal, unshared image loading.

    The file path must be absolute. To use a relative path, make it absolute
    using \e Qt.resolvedUrl() and replace the URL scheme. For example:

    \code
    ...
    property string imagePrefix: Qt.resolvedUrl("../myimages/").replace("file://", "image://shared/")
    Image { source: imagePrefix + "myimage.png" }
    \endcode

    The shared image module does not provide any directly usable QML types.
*/

QT_BEGIN_NAMESPACE

class QtQuickSharedImagePlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    QtQuickSharedImagePlugin(QObject *parent = 0) : QQmlExtensionPlugin(parent) {}

    void registerTypes(const char *uri) override
    {
        Q_ASSERT(uri == QStringLiteral("Qt.labs.sharedimage"));
        qmlRegisterModule(uri, 1, 0);
    }

    void initializeEngine(QQmlEngine *engine, const char *uri) override
    {
        Q_UNUSED(uri);
        engine->addImageProvider("shared", new SharedImageProvider);
    }
};

QT_END_NAMESPACE

#include "plugin.moc"
