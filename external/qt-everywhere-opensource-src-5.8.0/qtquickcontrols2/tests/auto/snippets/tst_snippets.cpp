/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <QtTest>
#include <QtQuick>

typedef QPair<QString, QString> QStringPair;

class tst_Snippets : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void verify();
    void verify_data();

    void screenshots();
    void screenshots_data();

private:
    QMap<QString, QStringPair> snippetPaths;
    QMap<QString, QStringPair> screenshotSnippetPaths;
};

static QMap<QString, QStringPair> findSnippets(const QDir &inputDir, const QDir &outputDir = QDir())
{
    QMap<QString, QStringPair> snippetPaths;
    QDirIterator it(inputDir.path(), QStringList() << "qtquick*.qml" << "qtlabs*.qml", QDir::Files | QDir::Readable);
    while (it.hasNext()) {
        QFileInfo fi(it.next());
        const QString outDirPath = !outputDir.path().isEmpty() ? outputDir.filePath(fi.baseName() + ".png") : QString();
        snippetPaths.insert(fi.baseName(), qMakePair(fi.filePath(), outDirPath));
    }
    return snippetPaths;
}

void tst_Snippets::initTestCase()
{
    qInfo() << "Snippets are taken from" << QQC2_SNIPPETS_PATH;

    QDir snippetsDir(QQC2_SNIPPETS_PATH);
    QVERIFY(!snippetsDir.path().isEmpty());

    snippetPaths = findSnippets(snippetsDir);
    QVERIFY(!snippetPaths.isEmpty());

    QDir screenshotOutputDir(QDir::current().filePath("screenshots"));
    QVERIFY(screenshotOutputDir.exists() || QDir::current().mkpath("screenshots"));

    QDir screenshotSnippetsDir(QQC2_SNIPPETS_PATH "/screenshots");
    QVERIFY(!screenshotSnippetsDir.path().isEmpty());

    screenshotSnippetPaths = findSnippets(screenshotSnippetsDir, screenshotOutputDir);
    QVERIFY(!screenshotSnippetPaths.isEmpty());
}

Q_DECLARE_METATYPE(QList<QQmlError>)

static void loadAndShow(QQuickView *view, const QString &source)
{
    qRegisterMetaType<QList<QQmlError> >();
    QSignalSpy warnings(view->engine(), SIGNAL(warnings(QList<QQmlError>)));
    QVERIFY(warnings.isValid());

    view->setSource(QUrl::fromLocalFile(source));
    QCOMPARE(view->status(), QQuickView::Ready);
    QVERIFY(view->errors().isEmpty());
    QVERIFY(view->rootObject());

    QVERIFY(warnings.isEmpty());

    view->show();
    view->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(view));
}

void tst_Snippets::verify()
{
    QFETCH(QString, input);

    QQuickView view;
    loadAndShow(&view, input);
    QGuiApplication::processEvents();
}

void tst_Snippets::verify_data()
{
    QTest::addColumn<QString>("input");

    QMap<QString, QStringPair>::const_iterator it;
    for (it = snippetPaths.constBegin(); it != snippetPaths.constEnd(); ++it)
        QTest::newRow(qPrintable(it.key())) << it.value().first;
}

void tst_Snippets::screenshots()
{
    QFETCH(QString, input);
    QFETCH(QString, output);

    QQuickView view;
    loadAndShow(&view, input);

    QSharedPointer<QQuickItemGrabResult> result = view.contentItem()->grabToImage();
    QSignalSpy spy(result.data(), SIGNAL(ready()));
    QVERIFY(spy.isValid());
    QVERIFY(spy.wait());
    QVERIFY(result->saveToFile(output));

    QGuiApplication::processEvents();
}

void tst_Snippets::screenshots_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("output");

    QMap<QString, QStringPair>::const_iterator it;
    for (it = screenshotSnippetPaths.constBegin(); it != screenshotSnippetPaths.constEnd(); ++it)
        QTest::newRow(qPrintable(it.key())) << it.value().first << it.value().second;
}

QTEST_MAIN(tst_Snippets)

#include "tst_snippets.moc"
