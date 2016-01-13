/****************************************************************************
**
** Copyright (C) 2012 - 2013 BlackBerry Limited. All rights reserved.
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#ifndef BB_CORE_PPSOBJECT_HPP
#define BB_CORE_PPSOBJECT_HPP

#include <bb/Global>
#include <bb/PpsAttribute>
#include <bb/PpsOpenMode>

#include <QMap>
#include <QObject>
#include <QVariantMap>

namespace bb
{
class PpsObjectPrivate;

class BB_CORE_EXPORT PpsObject : public QObject
{
    Q_OBJECT

public:
    explicit PpsObject(const QString &path, QObject *parent = 0);
    virtual ~PpsObject();

    int error() const;
    QString errorString() const;

    bool isReadyReadEnabled() const;
    bool isBlocking() const;
    bool setBlocking(bool enable);
    bool isOpen() const;
    bool open(PpsOpenMode::Types mode = PpsOpenMode::PublishSubscribe);
    bool close();
    QByteArray read(bool * ok = 0);
    bool write(const QByteArray &byteArray);
    bool remove();

    static QVariantMap decode( const QByteArray &rawData, bool * ok = 0 );
    static QMap<QString, PpsAttribute> decodeWithFlags( const QByteArray &rawData, bool * ok = 0 );
    static QByteArray encode( const QVariantMap &ppsData, bool * ok = 0 );

public Q_SLOTS:
    void setReadyReadEnabled(bool enable);

Q_SIGNALS:
    void readyRead();

private:
//!@cond PRIVATE
    QScopedPointer<PpsObjectPrivate> d_ptr;
    Q_DECLARE_PRIVATE(PpsObject)
    Q_DISABLE_COPY(PpsObject)
//!@endcond PRIVATE
};

} // namespace bb

#endif // BB_CORE_PPSOBJECT_HPP
