/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "audiodecoder.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>

#include <stdio.h>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QTextStream cout(stdout, QIODevice::WriteOnly);
    if (app.arguments().size() < 2) {
        cout << "Usage: audiodecoder [-p] [-pd] SOURCEFILE [TARGETFILE]" << endl;
        cout << "Set -p option if you want to play output file." << endl;
        cout << "Set -pd option if you want to play output file and delete it after successful playback." << endl;
        cout << "Default TARGETFILE name is \"out.wav\" in the same directory as the source file." << endl;
        return 0;
    }

    bool isPlayback = false;
    bool isDelete = false;

    if (app.arguments().at(1) == "-p")
        isPlayback = true;
    else if (app.arguments().at(1) == "-pd") {
        isPlayback = true;
        isDelete = true;
    }

    QFileInfo sourceFile;
    QFileInfo targetFile;

    int sourceFileIndex = (isPlayback || isDelete) ? 2 : 1;
    if (app.arguments().size() <= sourceFileIndex) {
        cout << "Error: source filename is not specified." << endl;
        return 0;
    }
    sourceFile.setFile(app.arguments().at(sourceFileIndex));
    if (app.arguments().size() > sourceFileIndex + 1)
        targetFile.setFile(app.arguments().at(sourceFileIndex + 1));
    else
        targetFile.setFile(sourceFile.dir().absoluteFilePath("out.wav"));

    AudioDecoder decoder(isPlayback, isDelete);
    QObject::connect(&decoder, SIGNAL(done()), &app, SLOT(quit()));
    decoder.setSourceFilename(sourceFile.absoluteFilePath());
    decoder.setTargetFilename(targetFile.absoluteFilePath());
    decoder.start();

    return app.exec();
}
