/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
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

#ifndef IMAGEANALYZER_H
#define IMAGEANALYZER_H

#include <QFutureWatcher>
#include <QtWidgets>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkReply;
class QNetworkDiskCache;
QT_END_NAMESPACE

//! [ ImageAnalyzer - public interface ]
class ImageAnalyzer : public QObject
{
    Q_OBJECT
public:
    ImageAnalyzer(QNetworkDiskCache * netcache, QObject * parent=0);

    QRgb lastResults();
    float lastRed();
    float lastGreen();
    float lastBlue();
    bool isBusy();
    Q_PROPERTY(bool busy READ isBusy);
    Q_PROPERTY(float red READ lastRed);
    Q_PROPERTY(float green READ lastGreen);
    Q_PROPERTY(float blue READ lastBlue);
    ~ImageAnalyzer();

public slots:
    /*! initiates analysis of all the urls in the list */
    void startAnalysis(const QStringList & urls);

signals:
    void finishedAnalysis();
    void updateProgress(int completed, int total);
    //! [ ImageAnalyzer - public interface ]

private slots:
    void handleReply(QNetworkReply*);
    void doneProcessing();
    void progressStatus(int);

private:
    QRgb processImages();
    void fetchURLs();
    void queueImage(QImage img);

    //! [ ImageAnalyzer - private members ]
private:
    QNetworkAccessManager* m_network;
    QNetworkDiskCache* m_cache;
    QStringList m_URLQueue;
    QList<QImage> m_imageQueue;
    int m_outstandingFetches;
    QFutureWatcher<QRgb> * m_watcher;
    //! [ ImageAnalyzer - private members ]
};

QRgb averageRGB(const QImage &img);

#endif
