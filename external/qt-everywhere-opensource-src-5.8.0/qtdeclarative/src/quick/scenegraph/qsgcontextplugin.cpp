/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQuick module of the Qt Toolkit.
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

#include "qsgcontextplugin_p.h"
#include <QtQuick/private/qsgcontext_p.h>
#include <QtGui/qguiapplication.h>
#include <QtCore/private/qfactoryloader_p.h>
#include <QtCore/qlibraryinfo.h>

// Built-in adaptations
#include <QtQuick/private/qsgsoftwareadaptation_p.h>
#ifndef QT_NO_OPENGL
#include <QtQuick/private/qsgdefaultcontext_p.h>
#endif

QT_BEGIN_NAMESPACE

Q_DECLARE_LOGGING_CATEGORY(QSG_LOG_INFO)

QSGContextPlugin::QSGContextPlugin(QObject *parent)
    : QObject(parent)
{
}

QSGContextPlugin::~QSGContextPlugin()
{
}

#ifndef QT_NO_LIBRARY
Q_GLOBAL_STATIC_WITH_ARGS(QFactoryLoader, loader,
    (QSGContextFactoryInterface_iid, QLatin1String("/scenegraph")))
#endif

struct QSGAdaptationBackendData
{
    QSGAdaptationBackendData();

    bool tried;
    QSGContextFactoryInterface *factory;
    QString name;
    QSGContextFactoryInterface::Flags flags;

    QVector<QSGContextFactoryInterface *> builtIns;

    QString quickWindowBackendRequest;
};

QSGAdaptationBackendData::QSGAdaptationBackendData()
    : tried(false)
    , factory(nullptr)
    , flags(0)
{
    // Fill in the table with the built-in adaptations.
    builtIns.append(new QSGSoftwareAdaptation);
}

Q_GLOBAL_STATIC(QSGAdaptationBackendData, qsg_adaptation_data)

// This only works when the backend is loaded (contextFactory() was called),
// otherwise the return value is 0.
//
// Note that the default (OpenGL) implementation always results in 0, custom flags
// can only be returned from the other (either compiled-in or plugin-based) backends.
QSGContextFactoryInterface::Flags qsg_backend_flags()
{
    return qsg_adaptation_data()->flags;
}

QSGAdaptationBackendData *contextFactory()
{
    QSGAdaptationBackendData *backendData = qsg_adaptation_data();

    if (!backendData->tried) {
        backendData->tried = true;

        const QStringList args = QGuiApplication::arguments();
        QString requestedBackend = backendData->quickWindowBackendRequest; // empty or set via QQuickWindow::setBackend()

        for (int index = 0; index < args.count(); ++index) {
            if (args.at(index).startsWith(QLatin1String("--device="))) {
                requestedBackend = args.at(index).mid(9);
                break;
            }
        }

        if (requestedBackend.isEmpty() && qEnvironmentVariableIsSet("QMLSCENE_DEVICE"))
            requestedBackend = QString::fromLocal8Bit(qgetenv("QMLSCENE_DEVICE"));

        // A modern alternative. Scenegraph adaptations can represent backends
        // for different graphics APIs as well, instead of being specific to
        // some device or platform.
        if (requestedBackend.isEmpty() && qEnvironmentVariableIsSet("QT_QUICK_BACKEND"))
            requestedBackend = QString::fromLocal8Bit(qgetenv("QT_QUICK_BACKEND"));

#ifdef QT_NO_OPENGL
        // If this is a build without OpenGL, and no backend has been set
        // default to the software renderer
        if (requestedBackend.isEmpty())
            requestedBackend = QString::fromLocal8Bit("software");
#endif

        if (!requestedBackend.isEmpty()) {
            qCDebug(QSG_LOG_INFO) << "Loading backend" << requestedBackend;

            // First look for a built-in adaptation.
            for (QSGContextFactoryInterface *builtInBackend : qAsConst(backendData->builtIns)) {
                if (builtInBackend->keys().contains(requestedBackend)) {
                    backendData->factory = builtInBackend;
                    backendData->name = requestedBackend;
                    backendData->flags = backendData->factory->flags(requestedBackend);
                    break;
                }
            }

#ifndef QT_NO_LIBRARY
            // Then try the plugins.
            if (!backendData->factory) {
                const int index = loader()->indexOf(requestedBackend);
                if (index != -1)
                    backendData->factory = qobject_cast<QSGContextFactoryInterface*>(loader()->instance(index));
                if (backendData->factory) {
                    backendData->name = requestedBackend;
                    backendData->flags = backendData->factory->flags(requestedBackend);
                }
                if (!backendData->factory) {
                    qWarning("Could not create scene graph context for backend '%s'"
                             " - check that plugins are installed correctly in %s",
                             qPrintable(requestedBackend),
                             qPrintable(QLibraryInfo::location(QLibraryInfo::PluginsPath)));
                }
            }
#endif // QT_NO_LIBRARY
        }
    }

    return backendData;
}



/*!
    \fn QSGContext *QSGContext::createDefaultContext()

    Creates a default scene graph context for the current hardware.
    This may load a device-specific plugin.
*/
QSGContext *QSGContext::createDefaultContext()
{
    QSGAdaptationBackendData *backendData = contextFactory();
    if (backendData->factory)
        return backendData->factory->create(backendData->name);
#ifndef QT_NO_OPENGL
    return new QSGDefaultContext();
#else
    return nullptr;
#endif
}



/*!
    Calls into the scene graph adaptation if available and creates a texture
    factory. The primary purpose of this function is to reimplement hardware
    specific asynchronous texture frameskip-less uploads that can happen on
    the image providers thread.
 */

QQuickTextureFactory *QSGContext::createTextureFactoryFromImage(const QImage &image)
{
    QSGAdaptationBackendData *backendData = contextFactory();
    if (backendData->factory)
        return backendData->factory->createTextureFactoryFromImage(image);
    return 0;
}


/*!
    Calls into the scene graph adaptation if available and creates a hardware
    specific window manager.
 */

QSGRenderLoop *QSGContext::createWindowManager()
{
    QSGAdaptationBackendData *backendData = contextFactory();
    if (backendData->factory)
        return backendData->factory->createWindowManager();
    return 0;
}

void QSGContext::setBackend(const QString &backend)
{
    QSGAdaptationBackendData *backendData = qsg_adaptation_data();
    if (backendData->tried)
        qWarning("Scenegraph already initialized, setBackend() request ignored");

    backendData->quickWindowBackendRequest = backend;
}

QT_END_NAMESPACE
