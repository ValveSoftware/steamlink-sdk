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


#include <QtCore/QDirIterator>
#include <QtTest/QtTest>

#include <QtXmlPatterns/QXmlQuery>
#include <QtXmlPatterns/QXmlSerializer>
#include "../qxmlquery/TestFundament.h"

/*!
 \class tst_CheckXMLFiles
 \internal
 \since 4.4
 \brief Checks whether the XML files found in $QTDIR are well-formed.
 */
class tst_CheckXMLFiles : public QObject
                        , private TestFundament
{
    Q_OBJECT

private Q_SLOTS:
    void checkXMLFiles() const;
    void checkXMLFiles_data() const;
};

void tst_CheckXMLFiles::checkXMLFiles() const
{
    QFETCH(QString, file);

    QXmlQuery query;
    query.setQuery(QLatin1String("doc-available('") + inputFileAsURI(file).toString() + QLatin1String("')"));
    QVERIFY(query.isValid());

    /* We don't care about the result, we only want to ensure the files can be parsed. */
    QByteArray dummy;
    QBuffer buffer(&dummy);
    QVERIFY(buffer.open(QIODevice::WriteOnly));
    QXmlSerializer serializer(query, &buffer);

    /* This is the important one. */
    QVERIFY(query.evaluateTo(&serializer));
}

void tst_CheckXMLFiles::checkXMLFiles_data() const
{
    QTest::addColumn<QString>("file");

    QStringList patterns;
    /* List possible XML files in Qt. */
    patterns.append(QLatin1String("*.xml"));
    patterns.append(QLatin1String("*.gccxml"));
    patterns.append(QLatin1String("*.svg"));
    patterns.append(QLatin1String("*.ui"));
    patterns.append(QLatin1String("*.qrc"));
    patterns.append(QLatin1String("*.ts"));
    /* We don't do HTML files currently because so many of them in 3rd party are broken. */
    patterns.append(QLatin1String("*.xhtml"));

    QDirIterator it(QLatin1String(SOURCETREE), patterns, QDir::AllEntries, QDirIterator::Subdirectories);
    while(it.hasNext())
    {
        it.next();
        QTest::newRow(it.filePath().toUtf8().constData()) << it.filePath();
    }
}

QTEST_MAIN(tst_CheckXMLFiles)

#include "tst_checkxmlfiles.moc"

// vim: et:ts=4:sw=4:sts=4
