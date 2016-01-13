/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtNfc module of the Qt Toolkit.
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

#ifndef QDECLARATIVENEARFIELD_P_H
#define QDECLARATIVENEARFIELD_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QtCore/QObject>
#include <QtQml/qqml.h>
#include <QtQml/QQmlParserStatus>
#include <QtNfc/QNearFieldManager>

#include "qqmlndefrecord.h"


QT_USE_NAMESPACE

class QDeclarativeNdefFilter;
class QDeclarativeNearField : public QObject, public QQmlParserStatus
{
    Q_OBJECT

    Q_PROPERTY(QQmlListProperty<QQmlNdefRecord> messageRecords READ messageRecords NOTIFY messageRecordsChanged)
    Q_PROPERTY(QQmlListProperty<QDeclarativeNdefFilter> filter READ filter NOTIFY filterChanged)
    Q_PROPERTY(bool orderMatch READ orderMatch WRITE setOrderMatch NOTIFY orderMatchChanged)

    Q_INTERFACES(QQmlParserStatus)

public:
    explicit QDeclarativeNearField(QObject *parent = 0);

    QQmlListProperty<QQmlNdefRecord> messageRecords();

    QQmlListProperty<QDeclarativeNdefFilter> filter();

    bool orderMatch() const;
    void setOrderMatch(bool on);

    // From QDeclarativeParserStatus
    void classBegin() { }
    void componentComplete();

signals:
    void messageRecordsChanged();
    void filterChanged();
    void orderMatchChanged();

private slots:
    void _q_handleNdefMessage(const QNdefMessage &message);

private:
    QList<QQmlNdefRecord *> m_message;
    QList<QDeclarativeNdefFilter *> m_filterList;
    bool m_orderMatch;
    bool m_componentCompleted;
    bool m_messageUpdating;

    QNearFieldManager *m_manager;
    int m_messageHandlerId;

    void registerMessageHandler();

    static void append_messageRecord(QQmlListProperty<QQmlNdefRecord> *list,
                                     QQmlNdefRecord *record);
    static int count_messageRecords(QQmlListProperty<QQmlNdefRecord> *list);
    static QQmlNdefRecord *at_messageRecord(QQmlListProperty<QQmlNdefRecord> *list,
                                                    int index);
    static void clear_messageRecords(QQmlListProperty<QQmlNdefRecord> *list);

    static void append_filter(QQmlListProperty<QDeclarativeNdefFilter> *list,
                              QDeclarativeNdefFilter *filter);
    static int count_filters(QQmlListProperty<QDeclarativeNdefFilter> *list);
    static QDeclarativeNdefFilter *at_filter(QQmlListProperty<QDeclarativeNdefFilter> *list,
                                             int index);
    static void clear_filter(QQmlListProperty<QDeclarativeNdefFilter> *list);
};

#endif // QDECLARATIVENEARFIELD_P_H
