/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#ifndef QQMLINSPECTORCLIENT_H
#define QQMLINSPECTORCLIENT_H

#include <private/qqmldebugclient_p.h>

class QQmlInspectorClient : public QQmlDebugClient
{
    Q_OBJECT

public:
    QQmlInspectorClient(QQmlDebugConnection *connection);

    int setInspectToolEnabled(bool enabled);
    int setShowAppOnTop(bool showOnTop);
    int setAnimationSpeed(qreal speed);
    int select(const  QList<int> &objectIds);
    int createObject(const QString &qml, int parentId, const QStringList &imports,
                     const QString &filename);
    int moveObject(int childId, int newParentId);
    int destroyObject(int objectId);

signals:
    void responseReceived(int requestId, bool result);

protected:
    void messageReceived(const QByteArray &message);

private:
    int m_lastRequestId;
};

#endif // QQMLINSPECTORCLIENT_H
