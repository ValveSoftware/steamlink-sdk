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

#include <QtTest/QtTest>
#include <QtCore/qt_windows.h>
#include <QtCore/QFileInfo>
#include <QtGui/QPixmap>
#include <QtGui/QImage>
#include <QtWinExtras/QtWin>

class tst_QPixmap : public QObject
{
    Q_OBJECT

public:
    tst_QPixmap() : m_dataDirectory(QFINDTESTDATA("data")) {}

private slots:
    void initTestCase();

    void toHBITMAP_data();
    void toHBITMAP();
    void fromHBITMAP_data();
    void fromHBITMAP();

    void toHICON_data();
    void toHICON();
    void fromHICON_data();
    void fromHICON();

private:
    const QString m_dataDirectory;
};

void tst_QPixmap::initTestCase()
{
    QVERIFY(!m_dataDirectory.isEmpty());
}

void tst_QPixmap::toHBITMAP_data()
{
    QTest::addColumn<int>("red");
    QTest::addColumn<int>("green");
    QTest::addColumn<int>("blue");

    QTest::newRow("red")   << 255 << 0 << 0;
    QTest::newRow("green") << 0 << 255 << 0;
    QTest::newRow("blue")  << 0 << 0 << 255;
}

void tst_QPixmap::toHBITMAP()
{
    QFETCH(int, red);
    QFETCH(int, green);
    QFETCH(int, blue);

    QPixmap pm(100, 100);
    pm.fill(QColor(red, green, blue));

    const HBITMAP bitmap = QtWin::toHBITMAP(pm);

    QVERIFY(bitmap != 0);

    // Verify size
    BITMAP bitmapInfo;
    memset(&bitmapInfo, 0, sizeof(BITMAP));

    QVERIFY(GetObject(bitmap, sizeof(BITMAP), &bitmapInfo));

    QCOMPARE(LONG(100), bitmapInfo.bmWidth);
    QCOMPARE(LONG(100), bitmapInfo.bmHeight);

    const HDC displayDc = GetDC(0);
    const HDC bitmapDc = CreateCompatibleDC(displayDc);

    const HBITMAP nullBitmap = static_cast<HBITMAP>(SelectObject(bitmapDc, bitmap));

    const COLORREF pixel = GetPixel(bitmapDc, 0, 0);
    QCOMPARE(int(GetRValue(pixel)), red);
    QCOMPARE(int(GetGValue(pixel)), green);
    QCOMPARE(int(GetBValue(pixel)), blue);

    // Clean up
    SelectObject(bitmapDc, nullBitmap);
    DeleteObject(bitmap);
    DeleteDC(bitmapDc);
    ReleaseDC(0, displayDc);
}

void tst_QPixmap::fromHBITMAP_data()
{
    toHBITMAP_data();
}

void tst_QPixmap::fromHBITMAP()
{
    QFETCH(int, red);
    QFETCH(int, green);
    QFETCH(int, blue);

    const HDC displayDc = GetDC(0);
    const HDC bitmapDc = CreateCompatibleDC(displayDc);
    const HBITMAP bitmap = CreateCompatibleBitmap(displayDc, 100, 100);
    SelectObject(bitmapDc, bitmap);

    SelectObject(bitmapDc, GetStockObject(NULL_PEN));
    const HGDIOBJ oldBrush = SelectObject(bitmapDc, CreateSolidBrush(RGB(red, green, blue)));
    Rectangle(bitmapDc, 0, 0, 100, 100);

    const QPixmap pixmap = QtWin::fromHBITMAP(bitmap);
    QCOMPARE(pixmap.width(), 100);
    QCOMPARE(pixmap.height(), 100);

    const QImage image = pixmap.toImage();
    const QRgb pixel = image.pixel(0, 0);
    QCOMPARE(qRed(pixel), red);
    QCOMPARE(qGreen(pixel), green);
    QCOMPARE(qBlue(pixel), blue);

    DeleteObject(SelectObject(bitmapDc, oldBrush));
    DeleteObject(SelectObject(bitmapDc, bitmap));
    DeleteDC(bitmapDc);
    ReleaseDC(0, displayDc);
}

static bool compareImages(const QImage &actual, const QImage &expected,
                          QByteArray *errorMessage)
{
    if (actual.size() != expected.size()) {
        QString s;
        QDebug(&s) << "Size mismatch, actual: " << actual.size() << " expected: " << expected.size();
        *errorMessage = s.toLocal8Bit();
        return false;
    }
    if (actual.format() != expected.format()) {
        *errorMessage = QByteArrayLiteral("Format mismatch, actual: ")
            + QByteArray::number(actual.format())
            + QByteArrayLiteral(", expected: ") + QByteArray::number(expected.format());
        return false;
    }

    static const int fuzz = 1;

    for (int y = 0; y < actual.height(); ++y) {
        for (int x = 0; x < expected.width(); ++x) {
            const QRgb actualRgb = actual.pixel(x, y);
            const QRgb expectedRgb = expected.pixel(x, y);
            const bool pixelMatches =
                qAbs(qRed(actualRgb) - qRed(expectedRgb)) <= fuzz
                && qAbs(qGreen(actualRgb) - qGreen(expectedRgb)) <= fuzz
                && qAbs(qBlue(actualRgb) - qBlue(expectedRgb)) <= fuzz
                && qAbs(qAlpha(actualRgb) - qAlpha(expectedRgb)) <= fuzz;
            if (!pixelMatches) {
                QString s;
                QDebug(&s) << "Pixmal mismatch at " << x << ',' << y << " actual: "
                           << QColor(actualRgb) << " expected: " << QColor(expectedRgb);
                *errorMessage = s.toLocal8Bit();
                return false;
            }
        }
    }
    return true;
}

static inline QString pngFileName(const QString &image, int width, int height)
{
    return image + QLatin1Char('_') + QString::number(width)
        + QLatin1Char('x') + QString::number(height) + QStringLiteral(".png");
}

void tst_QPixmap::toHICON_data()
{
    QTest::addColumn<QString>("image");
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("height");

    QTest::newRow("32bpp_16x16") << m_dataDirectory + QStringLiteral("/icon_32bpp") << 16 << 16;
    QTest::newRow("32bpp_32x32") << m_dataDirectory + QStringLiteral("/icon_32bpp") << 32 << 32;
    QTest::newRow("32bpp_48x48") << m_dataDirectory + QStringLiteral("/icon_32bpp") << 48 << 48;
    QTest::newRow("32bpp_256x256") << m_dataDirectory + QStringLiteral("/icon_32bpp") << 256 << 256;

    QTest::newRow("8bpp_16x16") << m_dataDirectory + QStringLiteral("/icon_8bpp") << 16 << 16;
    QTest::newRow("8bpp_32x32") << m_dataDirectory + QStringLiteral("/icon_8bpp") << 32 << 32;
    QTest::newRow("8bpp_48x48") << m_dataDirectory + QStringLiteral("/icon_8bpp") << 48 << 48;
}

void tst_QPixmap::toHICON()
{
    QFETCH(int, width);
    QFETCH(int, height);
    QFETCH(QString, image);

    QPixmap empty(width, height);
    empty.fill(Qt::transparent);

    const HDC displayDc = GetDC(0);
    const HDC bitmapDc = CreateCompatibleDC(displayDc);
    const HBITMAP bitmap = QtWin::toHBITMAP(empty, QtWin::HBitmapAlpha);
    SelectObject(bitmapDc, bitmap);

    const QString imageFileName = pngFileName(image, width, height);
    QVERIFY2(QFileInfo(imageFileName).exists(), qPrintable(imageFileName));

    const QImage imageFromFile = QImage(imageFileName).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QVERIFY(!imageFromFile.isNull());

    const HICON icon = QtWin::toHICON(QPixmap::fromImage(imageFromFile));

    DrawIconEx(bitmapDc, 0, 0, icon, width, height, 0, 0, DI_NORMAL);

    DestroyIcon(icon);
    DeleteDC(bitmapDc);

    const QImage imageFromHICON = QtWin::fromHBITMAP(bitmap, QtWin::HBitmapAlpha).toImage();
    QVERIFY(!imageFromHICON.isNull());

    ReleaseDC(0, displayDc);

    // fuzzy comparison must be used, as the pixel values change slightly during conversion
    // between QImage::Format_ARGB32 and QImage::Format_ARGB32_Premultiplied, or elsewhere
    QByteArray errorMessage;
    QVERIFY2(compareImages(imageFromHICON, imageFromFile, &errorMessage), errorMessage.constData());
}

void tst_QPixmap::fromHICON_data()
{
    toHICON_data();
}

void tst_QPixmap::fromHICON()
{
    QFETCH(int, width);
    QFETCH(int, height);
    QFETCH(QString, image);

    const QString iconFileName = image + QStringLiteral(".ico");
    QVERIFY2(QFileInfo(iconFileName).exists(), qPrintable(iconFileName));

    const HICON icon =
        static_cast<HICON>(LoadImage(0, reinterpret_cast<const wchar_t *>(iconFileName.utf16()),
                                     IMAGE_ICON, width, height, LR_LOADFROMFILE));
    const QImage imageFromHICON = QtWin::fromHICON(icon).toImage();
    DestroyIcon(icon);

    const QString imageFileName = pngFileName(image, width, height);
    QVERIFY2(QFileInfo(imageFileName).exists(), qPrintable(imageFileName));

    const QImage imageFromFile = QImage(imageFileName).convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QVERIFY(!imageFromFile.isNull());

    // fuzzy comparison must be used, as the pixel values change slightly during conversion
    // between QImage::Format_ARGB32 and QImage::Format_ARGB32_Premultiplied, or elsewhere
    QByteArray errorMessage;
    QVERIFY2(compareImages(imageFromHICON, imageFromFile, &errorMessage), errorMessage.constData());
}

QTEST_MAIN(tst_QPixmap)

#include "tst_qpixmap.moc"
