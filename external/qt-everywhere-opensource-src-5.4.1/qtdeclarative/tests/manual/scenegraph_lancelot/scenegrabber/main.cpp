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

#include <QtCore/QTimer>
#include <QtCore/QDebug>
#include <QtCore/QFileInfo>
#include <QtGui/QGuiApplication>
#include <QtGui/QImage>

#include <QtQuick/QQuickView>
#include <QtQuick/QQuickItem>

// Timeout values:

// A valid screen grab requires the scene to not change
// for SCENE_STABLE_TIME ms (default 500)
#define SCENE_STABLE_TIME  500

// Give up after SCENE_TIMEOUT ms
#define SCENE_TIMEOUT     16000


//#define GRABBERDEBUG

class GrabbingView : public QQuickView
{
    Q_OBJECT

public:
    GrabbingView(const QString &outputFile)
        : ofile(outputFile), frames(0), isGrabbing(false)
    {
        connect(this, SIGNAL(afterRendering()), SLOT(renderingDone()));
        QTimer::singleShot(SCENE_TIMEOUT, this, SLOT(timedOut()));
        stableSceneTimer.setSingleShot(true);
        connect(&stableSceneTimer, SIGNAL(timeout()), SLOT(sceneStabilized()));
    }

private slots:
    void renderingDone()
    {
        if (isGrabbing)
            return;
        isGrabbing = true;
        frames++;
#ifdef GRABBERDEBUG
        printf("...frame %i\n", frames);
#endif
        QImage img = grabWindow();
        //qDebug() << "Rendering done, grab is" << !img.isNull() << "timer valid:" << stableSceneTimer.isActive()  << "same as last:" << (img == lastGrab);
        if (!img.isNull() && img != lastGrab) {
            lastGrab = img;
            stableSceneTimer.start(SCENE_STABLE_TIME);
        }
        isGrabbing = false;
    }

    void sceneStabilized()
    {
#ifdef GRABBERDEBUG
        printf("...sceneStabilized IN\n");
#endif
        if (ofile == "-") {   // Write to stdout
            QFile of;
            if (!of.open(1, QIODevice::WriteOnly) || !lastGrab.save(&of, "ppm")) {
                qWarning() << "Error: failed to write grabbed image to stdout.";
                QGuiApplication::exit(2);
                return;
            }
        } else {
            if (!lastGrab.save(ofile)) {
                qWarning() << "Error: failed to store grabbed image to" << ofile;
                QGuiApplication::exit(2);
                return;
            }
        }

        QGuiApplication::exit(0);
#ifdef GRABBERDEBUG
        printf("...sceneStabilized OUT\n");
#endif
    }

    void timedOut()
    {
        qWarning() << "Error: timed out waiting for scene to stabilize." << frames << "frame(s) received. Last grab was" << (lastGrab.isNull() ? "invalid." : "valid.");
        QGuiApplication::exit(3);
    }

private:
    QImage lastGrab;
    QTimer stableSceneTimer;
    QString ofile;
    int frames;
    bool isGrabbing;
};


extern uint qt_qhash_seed;

int main(int argc, char *argv[])
{
    qt_qhash_seed = 0;

    QGuiApplication a(argc, argv);

    // Parse command line
    QString ifile, ofile;
    bool noText = false;
    QStringList args = a.arguments();
    int i = 0;
    bool argError = false;
    while (++i < args.size()) {
        QString arg = args.at(i);
        if ((arg == "-o") && (i < args.size()-1)) {
            ofile = args.at(++i);
        }
        else if (arg == "-notext") {
            noText = true;
        }
        else if (arg == "--cache-distance-fields") {
            ;
        }
        else if (ifile.isEmpty()) {
            ifile = arg;
        }
        else {
            argError = true;
            break;
        }
    }
    if (argError || ifile.isEmpty() || ofile.isEmpty()) {
        qWarning() << "Usage:" << args.at(0).toLatin1().constData() << "[-notext] <qml-infile> -o <outfile or - for ppm on stdout>";
        return 1;
    }

    QFileInfo ifi(ifile);
    if (!ifi.exists() || !ifi.isReadable() || !ifi.isFile()) {
        qWarning() << args.at(0).toLatin1().constData() << " error: unreadable input file" << ifile;
        return 1;
    }
    // End parsing

    GrabbingView v(ofile);
    v.setSource(QUrl::fromLocalFile(ifile));

    if (noText) {
        QList<QQuickItem*> items = v.rootObject()->findChildren<QQuickItem*>();
        foreach (QQuickItem *item, items) {
            if (QByteArray(item->metaObject()->className()).contains("Text"))
                item->setVisible(false);
        }
    }

    v.show();

    int retVal = a.exec();
#ifdef GRABBERDEBUG
    printf("...retVal=%i\n", retVal);
#endif

    return retVal;
}

#include "main.moc"
