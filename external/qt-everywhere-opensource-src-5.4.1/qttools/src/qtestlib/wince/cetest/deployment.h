/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the tools applications of the Qt Toolkit.
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

#ifndef DEPLOYMENT_INCL
#define DEPLOYMENT_INCL

#include <qstring.h>
#include <qlist.h>
#include <project.h>

class AbstractRemoteConnection;

struct CopyItem
{
    CopyItem(const QString& f, const QString& t) : from(f) , to(t) { }
    QString from;
    QString to;
};
typedef QList<CopyItem> DeploymentList;

class DeploymentHandler
{
public:
    inline void setConnection(AbstractRemoteConnection*);
    inline AbstractRemoteConnection* connection() const;
    bool deviceCopy(const DeploymentList &deploymentList);
    bool deviceDeploy(const DeploymentList &deploymentList);
    void cleanup(const DeploymentList &deploymentList);
    static void initProjectDeploy(QMakeProject* project, DeploymentList &deploymentList, const QString &testPath = "\\Program Files\\qt_test");
    static void initQtDeploy(QMakeProject* project, DeploymentList &deploymentList, const QString &testPath = "\\Program Files\\qt_test");
private:
    AbstractRemoteConnection* m_connection;
};

inline void DeploymentHandler::setConnection(AbstractRemoteConnection *connection) { m_connection = connection; }
inline AbstractRemoteConnection* DeploymentHandler::connection() const { return m_connection; }
#endif