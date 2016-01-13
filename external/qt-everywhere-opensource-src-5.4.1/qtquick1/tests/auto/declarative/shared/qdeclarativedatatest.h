/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

#ifndef QDECLARATIVETESTUTILS_H
#define QDECLARATIVETESTUTILS_H

#include <QtCore/QDir>
#include <QtCore/QUrl>
#include <QtCore/QCoreApplication>
#include <QtTest/QTest>

QT_FORWARD_DECLARE_CLASS(QDeclarativeComponent)
QT_FORWARD_DECLARE_CLASS(QDeclarativeEngine)

/* Base class for tests with data that are located in a "data" subfolder. */

class QDeclarativeDataTest : public QObject
{
    Q_OBJECT
public:
    QDeclarativeDataTest();
    virtual ~QDeclarativeDataTest();

    QString testFile(const QString &fileName) const;
    inline QString testFile(const char *fileName) const
        { return testFile(QLatin1String(fileName)); }
    inline QUrl testFileUrl(const QString &fileName) const
        { return QUrl::fromLocalFile(testFile(fileName)); }
    inline QUrl testFileUrl(const char *fileName) const
        { return testFileUrl(QLatin1String(fileName)); }

    inline QString dataDirectory() const { return m_dataDirectory; }
    inline QUrl dataDirectoryUrl() const { return m_dataDirectoryUrl; }

    inline QString importsDirectory() const { return m_importsDirectory; }
    inline QUrl importsDirectoryUrl() const { return m_importsDirectoryUrl; }

    inline QString directory() const  { return m_directory; }

    static inline QDeclarativeDataTest *instance() { return m_instance; }

    static QByteArray msgComponentError(const QDeclarativeComponent &,
                                        const QDeclarativeEngine *engine = 0);

public slots:
    virtual void initTestCase();

private:
    static QDeclarativeDataTest *m_instance;

    const QString m_dataDirectory;
    const QString m_importsDirectory;
    const QUrl m_dataDirectoryUrl;
    const QUrl m_importsDirectoryUrl;
    QString m_directory;
};

#endif // QDECLARATIVETESTUTILS_H
