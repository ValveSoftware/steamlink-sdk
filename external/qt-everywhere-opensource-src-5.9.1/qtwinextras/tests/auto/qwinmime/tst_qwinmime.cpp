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
#include <QWinMime>
#include <QtTest/QtTest>
#include <QtGui/QClipboard>
#include <QtGui/QPixmap>
#include <QtCore/QVariant>

class TestMime : public QWinMime
{
public:
    TestMime(bool verbose = false) : formatsForMimeCalled(false), m_verbose(verbose) {}

    bool canConvertFromMime(const FORMATETC &, const QMimeData *mimeData) const Q_DECL_OVERRIDE
    {
        if (m_verbose)
            qDebug() << Q_FUNC_INFO << mimeData->formats();
        return false;
    }

    bool convertFromMime(const FORMATETC &, const QMimeData *, STGMEDIUM *) const Q_DECL_OVERRIDE
    {
        if (m_verbose)
            qDebug() << Q_FUNC_INFO;
        return false;
    }

    QVector<FORMATETC> formatsForMime(const QString &mimeType, const QMimeData *mimeData) const Q_DECL_OVERRIDE
    {
        formatsForMimeCalled = true;
        if (m_verbose)
            qDebug() << Q_FUNC_INFO << mimeType << mimeData->formats();
        return QVector<FORMATETC>();
    }

    bool canConvertToMime(const QString &mimeType, IDataObject *) const Q_DECL_OVERRIDE
    {
        if (m_verbose)
            qDebug() << Q_FUNC_INFO << mimeType;
        return false;
    }

    QVariant convertToMime(const QString &mimeType, IDataObject *, QVariant::Type preferredType) const Q_DECL_OVERRIDE
    {
        if (m_verbose)
            qDebug() << Q_FUNC_INFO << mimeType << preferredType;
        return QVariant();
    }

    QString mimeForFormat(const FORMATETC &) const Q_DECL_OVERRIDE
    {
        if (m_verbose)
            qDebug() << Q_FUNC_INFO;
        return QString();
    }

    mutable bool formatsForMimeCalled;

private:
    const bool m_verbose;
};

class tst_QWinMime : public QObject
{
    Q_OBJECT

private slots:
    void testRegisterType();
    void testWinMime_data();
    void testWinMime();
};

void tst_QWinMime::testRegisterType()
{
    const int type = QWinMime::registerMimeType("foo/bar");
    QVERIFY2(type >= 0, QByteArray::number(type));
}

void tst_QWinMime::testWinMime_data()
{
    QTest::addColumn<QVariant>("data");
    QTest::newRow("string") << QVariant(QStringLiteral("bla"));
    QPixmap pm(10, 10);
    pm.fill(Qt::black);
    QTest::newRow("pixmap") << QVariant(pm);
}

void tst_QWinMime::testWinMime()
{
    QFETCH(QVariant, data);
    // Basic smoke test for crashes, copy some text into clipboard and check whether
    // the test implementation is called.
    TestMime testMime;
    QClipboard *clipboard = QApplication::clipboard();
    switch (data.type()) {
    case QMetaType::QString:
        clipboard->setText(data.toString());
        break;
    case QMetaType::QPixmap:
        clipboard->setPixmap(data.value<QPixmap>());
        break;
    default:
        break;
    }
    QTRY_VERIFY(testMime.formatsForMimeCalled);
}

QTEST_MAIN(tst_QWinMime)

#include "tst_qwinmime.moc"
