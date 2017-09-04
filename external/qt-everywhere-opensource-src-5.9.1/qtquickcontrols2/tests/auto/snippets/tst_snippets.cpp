/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
#include <QtQuickControls2>

typedef QPair<QString, QString> QStringPair;

class tst_Snippets : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void verify();
    void verify_data();

private:
    void loadSnippet(const QString &source);

    bool takeScreenshots;
    QMap<QString, QStringPair> snippetPaths;
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

    QDir screenshotsDir(QDir::current().filePath("screenshots"));

    takeScreenshots = qgetenv("SCREENSHOTS").toInt();
    if (takeScreenshots)
        QVERIFY(screenshotsDir.exists() || QDir::current().mkpath("screenshots"));

    snippetPaths = findSnippets(snippetsDir, screenshotsDir);
    QVERIFY(!snippetPaths.isEmpty());
}

Q_DECLARE_METATYPE(QList<QQmlError>)

void tst_Snippets::verify()
{
    QFETCH(QString, input);
    QFETCH(QString, output);

    QQmlEngine engine;
    QQmlComponent component(&engine);

    qRegisterMetaType<QList<QQmlError> >();
    QSignalSpy warnings(&engine, SIGNAL(warnings(QList<QQmlError>)));
    QVERIFY(warnings.isValid());

    QUrl url = QUrl::fromLocalFile(input);
    component.loadUrl(url);

    QObject *root = component.create();
    QVERIFY(root);

    QCOMPARE(component.status(), QQmlComponent::Ready);
    QVERIFY(component.errors().isEmpty());

    QVERIFY(warnings.isEmpty());

    if (takeScreenshots) {
        const QString currentDataTag = QLatin1String(QTest::currentDataTag());
        static const QString applicationStyle = QQuickStyle::name().isEmpty() ? "Default" : QQuickStyle::name();
        static const QStringList availableStyles = QQuickStyle::availableStyles();

        bool isStyledSnippet = false;
        const QString snippetStyle = currentDataTag.section("-", 1, 1);
        for (const QString &availableStyle : availableStyles) {
            if (!snippetStyle.compare(availableStyle, Qt::CaseInsensitive)) {
                if (applicationStyle != availableStyle)
                    QSKIP(qPrintable(QString("%1 style specific snippet. Running with the %2 style.").arg(availableStyle, applicationStyle)));
                isStyledSnippet = true;
            }
        }

        if (!isStyledSnippet && !applicationStyle.isEmpty()) {
            int index = output.indexOf("-", output.lastIndexOf("/"));
            if (index != -1)
                output.insert(index, "-" + applicationStyle.toLower());
        }

        QQuickWindow *window = qobject_cast<QQuickWindow *>(root);
        if (!window) {
            QQuickView *view = new QQuickView;
            view->setContent(url, &component, root);
            window = view;
        }

        window->show();
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(window));

        QSharedPointer<QQuickItemGrabResult> result = window->contentItem()->grabToImage();
        QSignalSpy spy(result.data(), SIGNAL(ready()));
        QVERIFY(spy.isValid());
        QVERIFY(spy.wait());
        QVERIFY(result->saveToFile(output));

        window->close();
    }
}

void tst_Snippets::verify_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("output");

    QMap<QString, QStringPair>::const_iterator it;
    for (it = snippetPaths.constBegin(); it != snippetPaths.constEnd(); ++it)
        QTest::newRow(qPrintable(it.key())) << it.value().first << it.value().second;
}

QTEST_MAIN(tst_Snippets)

#include "tst_snippets.moc"
