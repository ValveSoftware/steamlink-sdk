/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the tools applications of the Qt Toolkit.
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

#ifndef APPXPHONEENGINE_H
#define APPXPHONEENGINE_H

#include "appxengine.h"
#include "runnerengine.h"
#include "runner.h"

#include <QtCore/QScopedPointer>

QT_USE_NAMESPACE

class AppxPhoneEnginePrivate;
class AppxPhoneEngine : public AppxEngine
{
public:
    static bool canHandle(Runner *runner);
    static RunnerEngine *create(Runner *runner);
    static QStringList deviceNames();

    bool install(bool removeFirst = false) Q_DECL_OVERRIDE;
    bool remove() Q_DECL_OVERRIDE;
    bool start() Q_DECL_OVERRIDE;
    bool enableDebugging(const QString &debuggerExecutable,
                        const QString &debuggerArguments) Q_DECL_OVERRIDE;
    bool disableDebugging() Q_DECL_OVERRIDE;
    bool suspend() Q_DECL_OVERRIDE;
    bool waitForFinished(int secs) Q_DECL_OVERRIDE;
    bool stop() Q_DECL_OVERRIDE;

    QString devicePath(const QString &relativePath) const Q_DECL_OVERRIDE;
    bool sendFile(const QString &localFile, const QString &deviceFile) Q_DECL_OVERRIDE;
    bool receiveFile(const QString &deviceFile, const QString &localFile) Q_DECL_OVERRIDE;

private:
    explicit AppxPhoneEngine(Runner *runner);
    ~AppxPhoneEngine();

    QString extensionSdkPath() const;
    bool installPackage(IAppxManifestReader *reader, const QString &filePath) Q_DECL_OVERRIDE;

    bool connect();

    friend struct QScopedPointerDeleter<AppxPhoneEngine>;
    Q_DECLARE_PRIVATE(AppxPhoneEngine)
};

#endif // APPXPHONEENGINE_H
