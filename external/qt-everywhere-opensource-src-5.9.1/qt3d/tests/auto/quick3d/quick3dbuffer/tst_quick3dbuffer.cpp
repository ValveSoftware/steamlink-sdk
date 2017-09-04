/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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


#include <QtTest/QTest>
#include <Qt3DQuickRender/private/quick3dbuffer_p.h>
#include <QObject>
#include <QVector3D>

namespace {

bool writeBinaryFile(const QString filePath, const void *data, int byteSize)
{
    QFile f(filePath);
    if (f.open(QIODevice::WriteOnly))
        return f.write(reinterpret_cast<const char *>(data), byteSize) == byteSize;
    return false;
}

} // anonymous

class tst_Quick3DBuffer : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkInvalidBinaryFile()
    {
        // GIVEN
        Qt3DRender::Render::Quick::Quick3DBuffer buf;

        // WHEN
        QVariant data = buf.readBinaryFile(QUrl::fromLocalFile(QLatin1String("this_should_not_exist.bin")));

        // THEN
        QCOMPARE(data.userType(), static_cast<int>(QVariant::ByteArray));
        QVERIFY(data.value<QByteArray>().isEmpty());
    }

    void checkValidBinaryFile()
    {
        // GIVEN
        Qt3DRender::Render::Quick::Quick3DBuffer buf;
        QVector<QVector3D> dataArray = QVector<QVector3D>()
                << QVector3D(327.0f, 350.0f, 355.0f)
                << QVector3D(383.0f, 427.0f, 454.0f);

        const int bufferByteSize = dataArray.size() * sizeof(QVector3D);
        const QLatin1String filePath("binary_data.bin");
        const bool writingSucceeded = writeBinaryFile(filePath, dataArray.constData(), bufferByteSize);
        Q_ASSERT(writingSucceeded);

        // WHEN
        const QUrl path = QUrl::fromLocalFile(filePath);
        QVariant data = buf.readBinaryFile(path);

        // THEN
        QCOMPARE(data.userType(), static_cast<int>(QVariant::ByteArray));
        const QByteArray byteArray = data.value<QByteArray>();
        QCOMPARE(byteArray.size(), bufferByteSize);
        QVERIFY(memcmp(byteArray, dataArray.constData(), bufferByteSize) == 0);
    }

};

QTEST_MAIN(tst_Quick3DBuffer)

#include "tst_quick3dbuffer.moc"
