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

#ifndef USER_SCRIPT_H
#define USER_SCRIPT_H

#include "qtwebenginecoreglobal.h"

#include <QtCore/QAtomicInt>
#include <QtCore/QScopedPointer>
#include <QtCore/QSharedData>
#include <QtCore/QString>

struct UserScriptData;

namespace QtWebEngineCore {

class UserResourceControllerHost;

class QWEBENGINE_EXPORT UserScript : public QSharedData {
public:
    enum InjectionPoint {
        AfterLoad,
        DocumentLoadFinished,
        DocumentElementCreation
    };

    UserScript();
    UserScript(const UserScript &other);
    ~UserScript();
    UserScript &operator=(const UserScript &other);

    bool isNull() const;

    QString name() const;
    void setName(const QString &);

    QString sourceCode() const;
    void setSourceCode(const QString &);

    InjectionPoint injectionPoint() const;
    void setInjectionPoint(InjectionPoint);

    uint worldId() const;
    void setWorldId(uint id);

    bool runsOnSubFrames() const;
    void setRunsOnSubFrames(bool on);

    bool operator==(const UserScript &) const;

    static int validUserScriptSchemes();

private:
    void initData();
    UserScriptData &data() const;
    void parseMetadataHeader();
    friend class UserResourceControllerHost;

    QScopedPointer<UserScriptData> scriptData;
    QString m_name;
};

} // namespace QtWebEngineCore

#endif // USER_SCRIPT_H
