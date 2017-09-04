/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef QGEOCODEJSONPARSER_H
#define QGEOCODEJSONPARSER_H

#include <QtPositioning/QGeoShape>
#include <QtPositioning/QGeoLocation>

#include <QtCore/QObject>
#include <QtCore/QRunnable>
#include <QtCore/QJsonDocument>
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QList>

QT_BEGIN_NAMESPACE

class QGeoCodeJsonParser : public QObject, public QRunnable
{
    Q_OBJECT

public:
    void setBounds(const QGeoShape &bounds);
    void parse(const QByteArray &data);
    void run();

signals:
    void results(const QList<QGeoLocation> &locations);
    void error(const QString &errorString);

private:
    QJsonDocument m_document;
    QByteArray m_data;
    QGeoShape m_bounds;
    QList<QGeoLocation> m_results;
    QString m_errorString;
};

QT_END_NAMESPACE

#endif
