/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Petroules Corporation.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the JP2 plugins in the Qt ImageFormats module.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qjp2handler_p.h"

#include "qimage.h"
#include "qvariant.h"
#include "qcolor.h"

#include <jasper/jasper.h>

QT_BEGIN_NAMESPACE

class QJp2HandlerPrivate
{
    Q_DECLARE_PUBLIC(QJp2Handler)
    Q_DISABLE_COPY(QJp2HandlerPrivate)
public:
    int writeQuality;
    QByteArray subType;
    QJp2Handler *q_ptr;
    QJp2HandlerPrivate(QJp2Handler *q_ptr);
};

enum SubFormat { Jp2Format, J2kFormat };

/*
    \class Jpeg2000JasperReader
    \brief Jpeg2000JasperReader implements reading and writing of JPEG 2000
    image files.

    \internal

    This class is designed to be used together with the an QImageIO IOHandler,
    and it should probably not be necessary to instantiate it directly.

    Internally it used the Jasper library for coding the image data.
*/
class Jpeg2000JasperReader
{
public:
    Jpeg2000JasperReader(QIODevice *iod, const SubFormat format = Jp2Format);

    ~Jpeg2000JasperReader();

    bool read(QImage *pImage);
    bool write(const QImage &image, int quality);
private:
    typedef void (Jpeg2000JasperReader::*ScanlineFunc)(jas_seqent_t** const, uchar*);
    typedef void (Jpeg2000JasperReader::*ScanlineFuncWrite)(jas_matrix_t**, uchar*);

    void copyJasperQt(ScanlineFunc scanlinecopier);
    void copyJasperQtGeneric();
    void copyScanlineJasperQtRGB(jas_seqent_t ** const jasperRow, uchar *qtScanLine);
    void copyScanlineJasperQtRGBA(jas_seqent_t ** const jasperRow, uchar *qtScanLine);
    void copyScanlineJasperQtGray(jas_seqent_t ** const jasperRow, uchar *qtScanLine);
    void copyScanlineJasperQtGrayA(jas_seqent_t ** const jasperRow, uchar *qtScanLine);

    void copyQtJasper(const ScanlineFuncWrite scanlinecopier);
    void copyScanlineQtJasperRGB(jas_matrix_t ** jasperRow, uchar *qtScanLine);
    void copyScanlineQtJasperRGBA(jas_matrix_t ** jasperRow, uchar *qtScanLine);
    void copyScanlineQtJasperColormapRGB(jas_matrix_t ** jasperRow, uchar *qtScanLine);
    void copyScanlineQtJasperColormapRGBA(jas_matrix_t ** jasperRow, uchar *qtScanLine);
    void copyScanlineQtJasperColormapGrayscale(jas_matrix_t ** jasperRow, uchar *qtScanLine);
    void copyScanlineQtJasperColormapGrayscaleA(jas_matrix_t ** jasperRow, uchar *qtScanLine);

    bool attemptColorspaceChange(int wantedColorSpace);
    bool createJasperMatrix(jas_matrix_t **&matrix);
    bool freeJasperMatrix(jas_matrix_t **matrix);
    void printColorSpaceError();
    jas_image_cmptparm_t createComponentMetadata(const int width, const int height);
    jas_image_t *newRGBAImage(const int width, const int height, bool alpha);
    jas_image_t *newGrayscaleImage(const int width, const int height, bool alpha);
    bool decodeColorSpace(int clrspc, QString &family, QString &specific);
    void printMetadata(jas_image_t *image);

    bool jasperOk;

    QIODevice *ioDevice;
    QImage qtImage;
    SubFormat format;

    // Qt image properties
    int qtWidth;
    int qtHeight;
    int qtDepth;
    int qtNumComponents;

    jas_image_t *jasper_image;
    // jasper image properties
    int jasNumComponents;
    int jasComponentPrecicion[4];
    int computedComponentWidth ;
    int computedComponentHeight;
    int computedComponentHorizontalSubsampling;
    int computedComponentVerticalSubsampling;
    int jasperColorspaceFamily;
    // maps color to component (ex: colorComponentMapping[RED]
    // gives the component that contains the red color)
    int colorComponentMapping[4];
    bool hasAlpha;
};

QJp2HandlerPrivate::QJp2HandlerPrivate(QJp2Handler *q_ptr)
    : writeQuality(100), subType("jp2"), q_ptr(q_ptr)
{
}

/*!
    \class QJp2Handler
    \brief The QJp2Handler class provides support for reading and writing
    JPEG 2000 image files with the Qt plugin system.
    Currently, it only supports dynamically-loaded plugins.

    JPEG files comes in two subtypes: the JPEG 2000 file format (\c .jp2) and
    the JPEG 2000 code stream format (\c .j2k, \c .jpc, or \c .j2c).
    QJp2Handler can read and write both types.

    To select a subtype, use the setOption() function with the
    QImageIOHandler::SubType option and a parameter value of "jp2" or "j2k".

    To set the image quality when writing, you can use setOption() with the
    QImageIOHandler::Quality option, or QImageWriter::setQuality() if your are
    using a QImageWriter object to write the image. The image quality is
    specified as an int in the range 0 to 100. 0 means maximum compression and
    99 means minimum compression. 100 selects lossless encoding, and this is the
    default value.

    The JPEG handler is only available as a plugin,
    and this enables you to use normal QImage and QPixmap functions to read and
    write images. For example:

    \code
        myLabel->setPixmap(QPixmap("myimage.jp2"));

        QPixmap myPixmap;
        myPixmap.save("myimage.jp2", "JP2");
    \endcode
*/

/*!
    Constructs an instance of QJp2Handler.
*/
QJp2Handler::QJp2Handler()
    : d_ptr(new QJp2HandlerPrivate(this))
{
}

/*!
    Destructor for QJp2Handler.
*/
QJp2Handler::~QJp2Handler()
{

}

/*!
 Verifies if some values (magic bytes) are set as expected in the
 header of the file. If the magic bytes were found, we assume that we
 can read the file. The function will assume that the \a iod is
 pointing to the beginning of the JPEG 2000 header. (i.e. it will for
 instance not seek to the beginning of a file before reading).

 If \a subType is not 0, it will contain "jp2" or "j2k" upon
 successful return.
*/
bool QJp2Handler::canRead(QIODevice *iod, QByteArray *subType)
{
    bool bCanRead = false;
    if (iod) {
        const QByteArray header = iod->peek(12);
        if (header.startsWith(QByteArrayLiteral("\000\000\000\fjP  \r\n\207\n"))) {
            // Jp2 is the JPEG 2000 file format
            bCanRead = true;
            if (subType)
                *subType = QByteArray("jp2");
        } else if (header.startsWith(QByteArrayLiteral("\377\117\377\121\000"))) {
            // J2c is the JPEG 2000 code stream
            bCanRead = true;
            if (subType)
                *subType = QByteArray("j2k");
        }
    }
    return bCanRead;
}

/*! \reimp
*/
bool QJp2Handler::canRead() const
{
    QByteArray subType;
    if (canRead(device(), &subType)) {
        setFormat(subType);
        return true;
    }
    return false;
}

/*! \reimp
*/
bool QJp2Handler::read(QImage *image)
{
    Jpeg2000JasperReader reader(device());
    return reader.read(image);
}

/*! \reimp
*/
bool QJp2Handler::write(const QImage &image)
{
    Q_D(const QJp2Handler);
    SubFormat subFormat;
    if (d->subType == QByteArray("jp2"))
        subFormat = Jp2Format;
    else
        subFormat = J2kFormat;

    Jpeg2000JasperReader writer(device(), subFormat);
    return writer.write(image, d->writeQuality);
}

/*!
    Get the value associated with \a option.
    \sa setOption()
*/
QVariant QJp2Handler::option(ImageOption option) const
{
    Q_D(const QJp2Handler);
    if (option == Quality) {
        return QVariant(d->writeQuality);
    } else if (option == SubType) {
        return QVariant(d->subType);
    }
    return QVariant();
}

/*!
    The JPEG 2000 handler supports two options.
    Set \a option to QImageIOHandler::Quality to balance between quality and the
    compression level, where \a value should be an integer in the interval
    [0-100]. 0 is maximum compression (low quality) and 100 is lossless
    compression (high quality).

    Set \a option to QImageIOHandler::Subtype to choose the subtype,
    where \a value should be a string equal to either "jp2" or "j2k"
    \sa option()
*/
void QJp2Handler::setOption(ImageOption option, const QVariant &value)
{
    Q_D(QJp2Handler);
    if (option == Quality) {
        bool ok;
        const int quality = value.toInt(&ok);
        if (ok)
            d->writeQuality = quality;
    } else if (option == SubType) {
        const QByteArray subTypeCandidate = value.toByteArray();
        // Test for default Jpeg2000 file format (jp2), or stream format (j2k).
        if (subTypeCandidate == QByteArrayLiteral("jp2") ||
            subTypeCandidate == QByteArrayLiteral("j2k"))
            d->subType = subTypeCandidate;
    }
}

/*!
    This function will return true if \a option is set to either
    QImageIOHandler::Quality or QImageIOHandler::Subtype.
*/
bool QJp2Handler::supportsOption(ImageOption option) const
{
    return (option == Quality || option == SubType);
}

/*!
    Return the common identifier of the format.
    For JPEG 2000 this will return "jp2".
 */
QByteArray QJp2Handler::name() const
{
    return QByteArrayLiteral("jp2");
}

/*!
    Automatic resource handling for a jas_image_t*.
*/
class ScopedJasperImage
{
public:
    // Take reference to the pointer here, because the pointer
    // may change when we change color spaces.
    ScopedJasperImage(jas_image_t *&image):image(image) { }
    ~ScopedJasperImage() { jas_image_destroy(image); }
private:
    jas_image_t *&image;
};

/*! \internal
    Construct a Jpeg2000JasperReader using the provided \a imageIO.
    Note that currently the jasper library is initialized in this constructor,
    (and freed in the destructor) which means that:
    - Only one instance of this class may exist at one time
    - No thread safety
*/
Jpeg2000JasperReader::Jpeg2000JasperReader(QIODevice *iod, SubFormat format)
    : jasperOk(true), ioDevice(iod), format(format), hasAlpha(false)
{
    if (jas_init()) {
        jasperOk = false;
        qDebug("Jasper Library initialization failed");
    }
}

Jpeg2000JasperReader::~Jpeg2000JasperReader()
{
    if (jasperOk)
        jas_cleanup();
}

/*! \internal
    Opens the file data and attempts to decode it using the Jasper library.
    Returns true if successful, false on failure
*/
bool Jpeg2000JasperReader::read(QImage *pImage)
{
    if (!jasperOk)
        return false;

    /*
        Reading proceeds approximately as follows:
        1. Open stream and decode using Jasper
        2. Get image metadata
        3. Change colorspace if necessary
        4. Create a QImage of the appropriate type (32-bit for RGB,
           8-bit for grayscale)
        5. Copy image data from Jasper to the QImage

        When copying the image data from the Jasper data structures to the
        QImage, a generic copy function (copyJasperQt) iterates through the
        scanlines and calls the provided (via the scanlineCopier argument)
        scanline copy function for each scanline. The scanline copy function
        selected according to image metadata such as color space and the
        presence of an alpha channel.
    */
    QByteArray fileContents = ioDevice->readAll();
    jas_stream_t *imageData = jas_stream_memopen(fileContents.data(),
                                                 fileContents.size());
    jasper_image = jas_image_decode(imageData, jas_image_getfmt(imageData), 0);
    jas_stream_close(imageData);
    if (!jasper_image) {
        qDebug("Jasper library can't decode Jpeg2000 image data");
        return false;
    }
    ScopedJasperImage scopedImage(jasper_image);
    //printMetadata(jasper_image);

    qtWidth = jas_image_width(jasper_image);
    qtHeight = jas_image_height(jasper_image);
    jasNumComponents = jas_image_numcmpts(jasper_image);
    jasperColorspaceFamily = jas_clrspc_fam(jas_image_clrspc(jasper_image));

    bool needColorspaceChange = false;
    if (jasperColorspaceFamily != JAS_CLRSPC_FAM_RGB &&
        jasperColorspaceFamily != JAS_CLRSPC_FAM_GRAY)
        needColorspaceChange = true;

    // Get per-component data
    int c;
    for (c = 0; c < jasNumComponents; ++c) {
        jasComponentPrecicion[c] = jas_image_cmptprec(jasper_image, c);

        // Test for precision
        if (jasComponentPrecicion[c] > 8 || jasComponentPrecicion[c] < 8)
            needColorspaceChange = true;

        // Test for subsampling
        if (jas_image_cmpthstep(jasper_image, c) != 1 ||
            jas_image_cmptvstep(jasper_image, c) != 1)
            needColorspaceChange = true;

        // Test for signed components
        if (jas_image_cmptsgnd(jasper_image, c) != 0)
            needColorspaceChange = true;
    }

    /*
        If we encounter a different color space than RGB
        (such as XYZ or YCbCr) we change that to RGB.
        Also, if any component has "funny" metadata (such as precicion != 8 bits
        or subsampling != 1) we also do a colorspace
        change in order to convert it to something we can load.
    */

    bool decodeOk = true;
    if (needColorspaceChange)
        decodeOk = attemptColorspaceChange(JAS_CLRSPC_SRGB);

    if (!decodeOk) {
        printColorSpaceError();
        return false;
    }

    // Image metadata may have changed, get from Jasper.
    qtWidth = jas_image_width(jasper_image);
    qtHeight = jas_image_height(jasper_image);
    jasNumComponents = jas_image_numcmpts(jasper_image);
    jasperColorspaceFamily = jas_clrspc_fam(jas_image_clrspc(jasper_image));
    for (c = 0; c < jasNumComponents; ++c) {
        jasComponentPrecicion[c] = jas_image_cmptprec(jasper_image, c);
    }

    if (jasperColorspaceFamily != JAS_CLRSPC_FAM_RGB &&
        jasperColorspaceFamily != JAS_CLRSPC_FAM_GRAY) {
        qDebug("The Qt JPEG 2000 reader was unable to convert colorspace to RGB or grayscale");
        return false;
    }

    // If a component has a subsampling factor != 1, we can't trust
    // jas_image_height/width, so we need to figure it out ourselves
    bool oddComponentSubsampling = false;
    for (c = 0; c < jasNumComponents; ++c) {
        if (jas_image_cmpthstep(jasper_image, c) != 1 ||
            jas_image_cmptvstep(jasper_image, c) != 1) {
            oddComponentSubsampling = true;
        }
    }

    if (oddComponentSubsampling) {
        // Check if all components have the same vertical/horizontal dim and
        // subsampling
        computedComponentWidth = jas_image_cmptwidth(jasper_image, 0);
        computedComponentHeight = jas_image_cmptheight(jasper_image, 0);
        computedComponentHorizontalSubsampling = jas_image_cmpthstep(jasper_image, 0);
        computedComponentVerticalSubsampling = jas_image_cmptvstep(jasper_image, 0);

        for (c = 1; c < jasNumComponents; ++c) {
            if (computedComponentWidth != jas_image_cmptwidth(jasper_image, c) ||
                computedComponentWidth != jas_image_cmptwidth(jasper_image, c) ||
                computedComponentHorizontalSubsampling != jas_image_cmpthstep(jasper_image, c) ||
                computedComponentVerticalSubsampling != jas_image_cmptvstep(jasper_image, c)) {
                qDebug("The Qt JPEG 2000 reader does not support images where "
                       "component geometry differs from image geometry");
                return false;
            }
        }
        qtWidth = computedComponentWidth * computedComponentHorizontalSubsampling;
        qtHeight = computedComponentHeight * computedComponentVerticalSubsampling;
    }

    // Sanity check each component
    for (c = 0; c < jasNumComponents; ++c) {
        // Test for precision
        if (jasComponentPrecicion[c]>8 || jasComponentPrecicion[c]<8) {
            qDebug("The Qt JPEG 2000 reader does not support components with "
                   "precision != 8");
            decodeOk = false;
        }
#if 0
        // Test the subsampling factor (space between pixels on the image grid)
        if (oddComponentSubsampling) {
            qDebug("The Qt JPEG 2000 reader does not support components with "
                   "a subsampling factor != 1 (yet)");
            decodeOk = false;
        }
#endif
        // Test for signed components
        if (jas_image_cmptsgnd(jasper_image, c) != 0) {
            qDebug("Qt JPEG 2000 reader does not support signed components");
            decodeOk = false;
        }

        // Test for component/image geomoetry mismach.
        // If oddComponentSubsampling, then this is already taken care of above.
        if (!oddComponentSubsampling)
            if (jas_image_cmpttlx(jasper_image,c) != 0 ||
                jas_image_cmpttly(jasper_image,c) != 0 ||
                jas_image_cmptbrx(jasper_image,c) != jas_image_brx(jasper_image) ||
                jas_image_cmptbry(jasper_image,c) != jas_image_bry(jasper_image) ||
                jas_image_cmptwidth (jasper_image, c) != jas_image_width (jasper_image) ||
                jas_image_cmptheight(jasper_image, c) != jas_image_height(jasper_image )) {
                qDebug("The Qt JPEG 2000 reader does not support images where "
                       "component geometry differs from image geometry");
                printMetadata(jasper_image);
                decodeOk = false;
            }
    }
    if (!decodeOk)
        return false;

    // At this point, the colorspace should be either RGB or grayscale,
    // and each component should have eight bits of precision and
    // no unsupported geometry.
    //printMetadata(jasper_image);

    // Get color components
    jasperColorspaceFamily = jas_clrspc_fam(jas_image_clrspc(jasper_image));
    if (jasperColorspaceFamily == JAS_CLRSPC_FAM_RGB) {
        if (jasNumComponents > 4)
            qDebug("JPEG 2000 reader expected 3 or 4 components, got %d",
                   jasNumComponents);

        // Set up mapping from R,G,B -> component num.
        colorComponentMapping[0] = jas_image_getcmptbytype(jasper_image,
                                                           JAS_IMAGE_CT_RGB_R);
        colorComponentMapping[1] = jas_image_getcmptbytype(jasper_image,
                                                           JAS_IMAGE_CT_RGB_G);
        colorComponentMapping[2] = jas_image_getcmptbytype(jasper_image,
                                                           JAS_IMAGE_CT_RGB_B);
        qtNumComponents = 3;
    } else if (jasperColorspaceFamily == JAS_CLRSPC_FAM_GRAY) {
        if (jasNumComponents > 2)
            qDebug("JPEG 2000 reader expected 1 or 2 components, got %d",
                   jasNumComponents);
        colorComponentMapping[0] = jas_image_getcmptbytype(jasper_image,
                                    JAS_IMAGE_CT_COLOR(JAS_IMAGE_CT_GRAY_Y));
        qtNumComponents = 1;
    } else {
        printColorSpaceError();
        return false;
    }

    // Get alpha component if one exists. Due to the lack of test images,
    // loading images with alpha channels is a bit untested. It works
    // with images saved with this implementation though.
    const int posibleAlphaComponent1 = 3;
    const int posibleAlphaComponent2 = 48;

    if (jasNumComponents == qtNumComponents + 1) {
        colorComponentMapping[qtNumComponents] = jas_image_getcmptbytype(jasper_image, posibleAlphaComponent1);
        if (colorComponentMapping[qtNumComponents] < 0) {
            colorComponentMapping[qtNumComponents] = jas_image_getcmptbytype(jasper_image, posibleAlphaComponent2);
        }
        if (colorComponentMapping[qtNumComponents] > 0) {
            hasAlpha = true;
            qtNumComponents++;
        }
    }

    // Check for missing components
    for (c = 0; c < qtNumComponents; ++c) {
        if (colorComponentMapping[c] < 0) {
            qDebug("JPEG 2000 reader missing a color component");
            return false;
        }
    }

    // Create a QImage of the correct type
    if (jasperColorspaceFamily == JAS_CLRSPC_FAM_RGB) {
        qtImage = QImage(qtWidth, qtHeight, hasAlpha
                                                ? QImage::Format_ARGB32
                                                : QImage::Format_RGB32);
    } else if (jasperColorspaceFamily == JAS_CLRSPC_FAM_GRAY) {
        if (hasAlpha) {
            qtImage = QImage(qtWidth, qtHeight, QImage::Format_ARGB32);
        } else {
            qtImage = QImage(qtWidth, qtHeight, QImage::Format_Grayscale8);
        }
    }

    // Copy data
    if (oddComponentSubsampling) {
        // This is a hack really, copying of data with component subsampling
        // != 1 doesn't fit in with the rest of the scanline copying framework.
        copyJasperQtGeneric();
    } else if (jasperColorspaceFamily == JAS_CLRSPC_FAM_RGB) {
        if (hasAlpha)
            copyJasperQt(&Jpeg2000JasperReader::copyScanlineJasperQtRGBA);
        else
            copyJasperQt(&Jpeg2000JasperReader::copyScanlineJasperQtRGB);
    } else if (jasperColorspaceFamily == JAS_CLRSPC_FAM_GRAY) {
        if (hasAlpha)
            copyJasperQt(&Jpeg2000JasperReader::copyScanlineJasperQtGrayA);
        else
            copyJasperQt(&Jpeg2000JasperReader::copyScanlineJasperQtGray);
    }
    if (decodeOk)
        *pImage = qtImage;

    return decodeOk;
}

/*!
    \internal
*/
void Jpeg2000JasperReader::copyJasperQtGeneric()
{
    // Create scanline data poinetrs
    jas_matrix_t **jasperMatrix;
    jas_seqent_t **jasperRow;
    createJasperMatrix(jasperMatrix);
    jasperRow = (jas_seqent_t**)malloc(jasNumComponents * sizeof(jas_seqent_t *));
    Q_CHECK_PTR(jasperRow);

    int imageY = 0;
    for (int componentY = 0; componentY < computedComponentHeight; ++componentY) {
        for (int c = 0; c < jasNumComponents; ++c) {
            jas_image_readcmpt(jasper_image, colorComponentMapping[c], 0,
                               componentY, computedComponentWidth, 1,
                               jasperMatrix[c]);
            jasperRow[c] = jas_matrix_getref(jasperMatrix[c], 0, 0);
        }
        for (int verticalSubsample = 0;
                verticalSubsample < computedComponentVerticalSubsampling;
                ++verticalSubsample) {
            uchar *scanLineUchar = qtImage.scanLine(imageY);
            QRgb *scanLineQRgb = reinterpret_cast<QRgb *>(scanLineUchar);
            for (int componentX = 0; componentX < computedComponentWidth;
                    ++componentX) {
                for (int horizontalSubsample = 0;
                        horizontalSubsample <
                            computedComponentHorizontalSubsampling;
                        ++horizontalSubsample) {
                    if (jasperColorspaceFamily == JAS_CLRSPC_FAM_RGB) {
                        if (hasAlpha) {
                            *scanLineQRgb++ = (jasperRow[3][componentX] << 24) |
                                              (jasperRow[0][componentX] << 16) |
                                              (jasperRow[1][componentX] << 8) |
                                              jasperRow[2][componentX];
                        } else {
                            *scanLineQRgb++ = (jasperRow[0][componentX] << 16) |
                                              (jasperRow[1][componentX] << 8) |
                                              jasperRow[2][componentX];
                        }
                    } else if (jasperColorspaceFamily == JAS_CLRSPC_FAM_GRAY) {
                        if (hasAlpha) {
                            *scanLineQRgb++ = (jasperRow[1][componentX] << 24) |
                                              (jasperRow[0][componentX] << 16) |
                                              (jasperRow[0][componentX] << 8) |
                                              jasperRow[0][componentX];
                        } else {
                            *scanLineUchar++ = jasperRow[0][componentX];
                        }
                    }
                }
            }
            ++imageY;
        }
    }
}

/*!
    \internal
    Copies data from Jasper to QImage. The scanlineCopier parameter specifies
    which function to use for handling each scan line.
*/
void Jpeg2000JasperReader::copyJasperQt(const ScanlineFunc scanlineCopier)
{
    // Create scanline data poinetrs
    jas_matrix_t **jasperMatrix;
    jas_seqent_t **jasperRow;

    createJasperMatrix(jasperMatrix);
    jasperRow = (jas_seqent_t**)malloc(jasNumComponents * sizeof(jas_seqent_t *));
    Q_CHECK_PTR(jasperRow);

    for (int scanline = 0; scanline < qtHeight; ++scanline) {
        for (int c = 0; c < jasNumComponents; ++c) {
            jas_image_readcmpt(jasper_image, colorComponentMapping[c], 0,
                               scanline, qtWidth, 1, jasperMatrix[c]);
            jasperRow[c] = jas_matrix_getref(jasperMatrix[c], 0, 0);
        }
        (this->*scanlineCopier)(jasperRow, qtImage.scanLine(scanline));
    }

    freeJasperMatrix(jasperMatrix);
    free(jasperRow);
}

/*!
    \internal
    Copies RGB data from Jasper to a 32-bit QImage.
*/
void Jpeg2000JasperReader::copyScanlineJasperQtRGB(
                            jas_seqent_t ** const jasperRow, uchar *qtScanLine)
{
    QRgb *scanLine = reinterpret_cast<QRgb *>(qtScanLine);
    for (int c = 0; c < qtWidth; ++c) {
        *scanLine++ = (0xFF << 24) |
                      (jasperRow[0][c] << 16) |
                      (jasperRow[1][c] << 8) |
                      jasperRow[2][c];
    }
}

/*!
    \internal
    Copies RGBA data from Jasper to a 32-bit QImage.
*/
void Jpeg2000JasperReader::copyScanlineJasperQtRGBA(jas_seqent_t ** const jasperRow,  uchar *qtScanLine)
{
    QRgb *scanLine = reinterpret_cast<QRgb *>(qtScanLine);
    for (int c = 0; c < qtWidth; ++c) {
        *scanLine++ = (jasperRow[3][c] << 24) |
                      (jasperRow[0][c] << 16) |
                      (jasperRow[1][c] << 8) |
                      jasperRow[2][c];
    }
}

/*!
    \internal
    Copies data from a grayscale image to an 8-bit QImage.
*/
void Jpeg2000JasperReader::copyScanlineJasperQtGray(jas_seqent_t ** const jasperRow,  uchar *qtScanLine)
{
    for (int c = 0; c < qtWidth; ++c) {
        // *qtScanLine++ = (jasperRow[0][c] >> (jasComponentPrecicion[0] - 8));
        *qtScanLine++ = jasperRow[0][c];
    }
}

/*!
    \internal
    Copies data from a grayscale image to a 32-bit QImage.
    Note that in this case we use an 32-bit image for grayscale data, since the
    alpha value is per-pixel, not per-color (per-color alpha is supported by
    8-bit QImage).
*/
void Jpeg2000JasperReader::copyScanlineJasperQtGrayA(jas_seqent_t ** const jasperRow,  uchar *qtScanLine)
{
    QRgb *scanLine = reinterpret_cast<QRgb *>(qtScanLine);
    for (int c = 0; c < qtWidth; ++c) {
        *scanLine++ = (jasperRow[1][c] << 24) |
                      (jasperRow[0][c] << 16) |
                      (jasperRow[0][c] << 8) |
                      jasperRow[0][c];
    }
}

/*!
    Opens the file data and attempts to decode it using the Jasper library.
    Returns true on success, false on failure.

    32-bit and color mapped color images are encoded as RGB images,
    color mapped grayscale images are encoded as grayscale images
*/
bool Jpeg2000JasperReader::write(const QImage &image, int quality)
{
    if (!jasperOk)
        return false;

    qtImage = image;

    qtHeight = qtImage.height();
    qtWidth = qtImage.width();
    qtDepth = qtImage.depth();

    if (qtDepth == 32) { // RGB(A)
        jasper_image = newRGBAImage(qtWidth, qtHeight, qtImage.hasAlphaChannel());
        if (!jasper_image)
            return false;

        if (qtImage.hasAlphaChannel())
            copyQtJasper(&Jpeg2000JasperReader::copyScanlineQtJasperRGBA);
        else
            copyQtJasper(&Jpeg2000JasperReader::copyScanlineQtJasperRGB);
    } else if (qtDepth == 8) {
        // Color mapped grayscale
        if (qtImage.allGray()) {
            jasper_image = newGrayscaleImage(qtWidth, qtHeight, qtImage.hasAlphaChannel());
            if (!jasper_image)
                return false;

            if (qtImage.hasAlphaChannel())
                copyQtJasper(&Jpeg2000JasperReader::copyScanlineQtJasperColormapGrayscaleA);
            else
                copyQtJasper(&Jpeg2000JasperReader::copyScanlineQtJasperColormapGrayscale);
        } else {
            // Color mapped color
            jasper_image = newRGBAImage(qtWidth, qtHeight, qtImage.hasAlphaChannel());
            if (!jasper_image)
                return false;

            if (qtImage.hasAlphaChannel())
                copyQtJasper(&Jpeg2000JasperReader::copyScanlineQtJasperColormapRGBA);
            else
                copyQtJasper(&Jpeg2000JasperReader::copyScanlineQtJasperColormapRGB);
        }
    } else {
        qDebug("Unable to handle color depth %d", qtDepth);
        return false;
    }

    int fmtid;
    if (format == Jp2Format)
        fmtid = jas_image_strtofmt(const_cast<char*>("jp2"));
    else /* if (format == J2cFormat) */
        // JasPer refers to the code stream format as jpc
        fmtid = jas_image_strtofmt(const_cast<char*>("jpc"));

    const int minQuality = 0;
    const int maxQuality = 100;

    if (quality == -1)
        quality = 100;
    if (quality <= minQuality)
        quality = minQuality;
    if (quality > maxQuality)
        quality = maxQuality;

    // Qt specifies quality as an integer in the range 0..100. Jasper specifies
    // compression rate as an real in the range 0..1, where 1 corresponds to no
    // compression. Computing the rate from quality is difficult, large images
    // get better image quality than small images at the same rate. If the rate
    // is too low, Jasper will generate a completely black image.
    // minirate is the smallest safe rate value.
    const double minRate = 0.001;

    // maxRate specifies maximum target rate, which give the minimum amount
    // of compression. Tests show that maxRates higer than 0.3 give no
    // additional image quality for most images. Large images could use an even
    // smaller maxRate value.
    const double maxRate = 0.3;

    // Set jasperRate to a value in the range minRate..maxRate. Distribute the
    // quality steps more densely at the lower end if the rate scale.
    const double jasperRate = minRate + pow((double(quality) / double(maxQuality)), 2) * maxRate;

    // The Jasper format string contains two options:
    // rate: rate=x
    // lossy/lossless compression : mode=real/mode=int
    QString jasperFormatString;

    // If quality is not maxQuality, we set lossy encoding.
    // (lossless is default)
    if (quality != maxQuality) {
        jasperFormatString += QLatin1String("mode=real");
        jasperFormatString += QString(QLatin1String(" rate=%1")).arg(jasperRate);
    }

    // Open an empty jasper stream that grows automatically
    jas_stream_t * memory_stream = jas_stream_memopen(0, -1);

    // Jasper wants a non-const string.
    char *str = qstrdup(jasperFormatString.toLatin1().constData());
    jas_image_encode(jasper_image, memory_stream, fmtid, str);
    delete[] str;
    jas_stream_flush(memory_stream);

    // jas_stream_t::obj_ is a void* which points to the stream implementation,
    // e.g a file stream or a memory stream. But in our case we know that it is
    // a memory stream since we created the object, so we just reiterpret_cast
    // here..
    char *buffer = reinterpret_cast<char *>(reinterpret_cast<jas_stream_memobj_t*>(memory_stream->obj_)->buf_);
    qint64 length = jas_stream_length(memory_stream);
    ioDevice->write(buffer, length);

    jas_stream_close(memory_stream);
    jas_image_destroy(jasper_image);

    return true;
}

/*!
    \internal
    Copies data from qtImage to JasPer. The scanlineCopier parameter specifies
    which function to use for handling each scan line.
*/
void Jpeg2000JasperReader::copyQtJasper(const ScanlineFuncWrite scanlinecopier)
{
    // Create jasper matrix for holding one scanline
    jas_matrix_t **jasperMatrix;
    createJasperMatrix(jasperMatrix);

    for (int scanline = 0; scanline < qtHeight; ++scanline) {
        (this->*scanlinecopier)(jasperMatrix, qtImage.scanLine(scanline));

        // Write a scanline of data to jasper_image
        for (int c = 0; c < jasNumComponents; ++c)
            jas_image_writecmpt(jasper_image, c, 0, scanline, qtWidth, 1,
                                jasperMatrix[c]);
    }
    freeJasperMatrix(jasperMatrix);
}

/*!
    \internal
*/
void Jpeg2000JasperReader::copyScanlineQtJasperRGB(jas_matrix_t ** jasperRow,
                                                   uchar *qtScanLine)
{
    QRgb *scanLineBuffer = reinterpret_cast<QRgb *>(qtScanLine);
    for (int col = 0; col < qtWidth; ++col) {
        jas_matrix_set(jasperRow[0], 0, col, (*scanLineBuffer & 0xFF0000) >> 16);
        jas_matrix_set(jasperRow[1], 0, col, (*scanLineBuffer & 0x00FF00) >> 8);
        jas_matrix_set(jasperRow[2], 0, col, *scanLineBuffer & 0x0000FF);
        ++scanLineBuffer;
    }
}

/*!
    \internal
*/
void Jpeg2000JasperReader::copyScanlineQtJasperRGBA(jas_matrix_t ** jasperRow,
                                                    uchar *qtScanLine)
{
    QRgb *scanLineBuffer = reinterpret_cast<QRgb *>(qtScanLine);
    for (int col = 0; col < qtWidth; ++col) {
        jas_matrix_set(jasperRow[3], 0, col, (*scanLineBuffer & 0xFF000000) >> 24);
        jas_matrix_set(jasperRow[0], 0, col, (*scanLineBuffer & 0x00FF0000) >> 16);
        jas_matrix_set(jasperRow[1], 0, col, (*scanLineBuffer & 0x0000FF00) >> 8);
        jas_matrix_set(jasperRow[2], 0, col, *scanLineBuffer & 0x000000FF);
        ++scanLineBuffer;
    }
}

/*!
    \internal
*/
void Jpeg2000JasperReader::copyScanlineQtJasperColormapRGB(jas_matrix_t ** jasperRow,
                                                           uchar *qtScanLine)
{
    for (int col = 0; col < qtWidth; ++col) {
        QRgb color = qtImage.color(*qtScanLine);
        jas_matrix_set(jasperRow[0], 0, col, qRed(color));
        jas_matrix_set(jasperRow[1], 0, col, qGreen(color));
        jas_matrix_set(jasperRow[2], 0, col, qBlue(color));
        ++qtScanLine;
    }
}

/*!
    \internal
*/
void Jpeg2000JasperReader::copyScanlineQtJasperColormapRGBA(jas_matrix_t ** jasperRow,
                                                            uchar *qtScanLine)
{
    for (int col = 0; col < qtWidth; ++col) {
        QRgb color = qtImage.color(*qtScanLine);
        jas_matrix_set(jasperRow[0], 0, col, qRed(color));
        jas_matrix_set(jasperRow[1], 0, col, qGreen(color));
        jas_matrix_set(jasperRow[2], 0, col, qBlue(color));
        jas_matrix_set(jasperRow[3], 0, col, qAlpha(color));
        ++qtScanLine;
    }
}

/*!
    \internal
*/
void Jpeg2000JasperReader::copyScanlineQtJasperColormapGrayscale(jas_matrix_t ** jasperRow,
                                                                 uchar *qtScanLine)
{
    for (int col = 0; col < qtWidth; ++col) {
        QRgb color = qtImage.color(*qtScanLine);
        jas_matrix_set(jasperRow[0], 0, col, qGray(color));
        ++qtScanLine;
    }
}

/*!
    \internal
*/
void Jpeg2000JasperReader::copyScanlineQtJasperColormapGrayscaleA(jas_matrix_t ** jasperRow,
                                                                  uchar *qtScanLine)
{
    for (int col = 0; col < qtWidth; ++col) {
        QRgb color = qtImage.color(*qtScanLine);
        jas_matrix_set(jasperRow[0], 0, col, qGray(color));
        jas_matrix_set(jasperRow[1], 0, col, qAlpha(color));
        ++qtScanLine;
    }
}

/*!
    \internal
    Attempts to change the color space for the image to wantedColorSpace using
    the JasPer library
*/
bool Jpeg2000JasperReader::attemptColorspaceChange(int wantedColorSpace)
{
    //qDebug("Attemting color space change");
    jas_cmprof_t *outprof;
    if (!(outprof = jas_cmprof_createfromclrspc(wantedColorSpace)))
        return false;

    jas_image_t *newimage;
    if (!(newimage = jas_image_chclrspc(jasper_image, outprof,
                                        JAS_CMXFORM_INTENT_PER))) {
        jas_cmprof_destroy(outprof);
        return false;
    }
    jas_image_destroy(jasper_image);
    jas_cmprof_destroy(outprof);
    jasper_image = newimage;
    return true;
}

/*!
    \internal
    Set up a component with parameters suitable for storing a QImage.
*/
jas_image_cmptparm_t Jpeg2000JasperReader::createComponentMetadata(
                                            const int width, const int height)
{
    jas_image_cmptparm_t param;
    param.tlx = 0;
    param.tly = 0;
    param.hstep = 1;
    param.vstep = 1;
    param.width = width;
    param.height = height;
    param.prec = 8;
    param.sgnd = 0;
    return param;
}

/*!
    \internal
    Create a new RGB JasPer image with a possible alpha channel.
*/
jas_image_t* Jpeg2000JasperReader::newRGBAImage(const int width,
                                                const int height, bool alpha)
{
    jasNumComponents = alpha ? 4 : 3;
    jas_image_cmptparm_t *params = new jas_image_cmptparm_t[jasNumComponents];
    jas_image_cmptparm_t param = createComponentMetadata(width, height);
    for (int c=0; c < jasNumComponents; c++)
        params[c] = param;
    jas_image_t *newImage = jas_image_create(jasNumComponents, params,
                                             JAS_CLRSPC_SRGB);

    if (!newImage) {
        delete[] params;
        return 0;
    }

    jas_image_setcmpttype(newImage, 0, JAS_IMAGE_CT_RGB_R);
    jas_image_setcmpttype(newImage, 1, JAS_IMAGE_CT_RGB_G);
    jas_image_setcmpttype(newImage, 2, JAS_IMAGE_CT_RGB_B);

    /*
        It is unclear how one stores opacity(alpha) components with JasPer,
        the following seems to have no effect. The opacity component gets
        type id 3 or 48 depending jp2 or j2c format no matter what one puts
        in here.

        The symbols are defined as follows:
        #define JAS_IMAGE_CT_RGB_R 0
        #define JAS_IMAGE_CT_RGB_G 1
        #define JAS_IMAGE_CT_RGB_B 2
        #define JAS_IMAGE_CT_OPACITY 0x7FFF
    */
    if (alpha)
        jas_image_setcmpttype(newImage, 3, JAS_IMAGE_CT_OPACITY);
    delete[] params;
    return newImage;
}

/*!
    \internal
    Create a new RGB JasPer image with a possible alpha channel.
*/
jas_image_t *Jpeg2000JasperReader::newGrayscaleImage(const int width,
                                                     const int height,
                                                     bool alpha)
{
    jasNumComponents = alpha ? 2 : 1;
    jas_image_cmptparm_t param = createComponentMetadata(width, height);
    jas_image_t *newImage = jas_image_create(1, &param, JAS_CLRSPC_SGRAY);
    if (!newImage)
        return 0;

    jas_image_setcmpttype(newImage, 0, JAS_IMAGE_CT_GRAY_Y);

    // See corresponding comment for newRGBAImage.
    if (alpha)
        jas_image_setcmpttype(newImage, 1, JAS_IMAGE_CT_OPACITY);
    return newImage;
}

/*!
    \internal
    Allocate data structures that hold image data during transfer from the
    JasPer data structures to QImage.
*/
bool Jpeg2000JasperReader::createJasperMatrix(jas_matrix_t **&matrix)
{
    matrix = (jas_matrix_t**)malloc(jasNumComponents * sizeof(jas_matrix_t *));
    for (int c = 0; c < jasNumComponents; ++c)
        matrix[c] = jas_matrix_create(1, qtWidth);
    return true;
}

/*!
    \internal
    Free data structures that hold image data during transfer from the
    JasPer data structures to QImage.
*/
bool Jpeg2000JasperReader::freeJasperMatrix(jas_matrix_t **matrix)
{
    for (int c = 0; c < jasNumComponents; ++c)
        jas_matrix_destroy(matrix[c]);
    free(matrix);
    return false;
}

/*!
    \internal
*/
void Jpeg2000JasperReader::printColorSpaceError()
{
    QString colorspaceFamily, colorspaceSpecific;
    decodeColorSpace(jas_image_clrspc(jasper_image), colorspaceFamily,
                     colorspaceSpecific);
    qDebug("Jpeg2000 decoder is not able to handle color space %s - %s",
           qPrintable(colorspaceFamily), qPrintable(colorspaceSpecific));
}
/*!
    \internal
*/
bool Jpeg2000JasperReader::decodeColorSpace(int clrspc, QString &family,
                                                        QString &specific)
{
    int fam = jas_clrspc_fam(clrspc);
    int mbr = jas_clrspc_mbr(clrspc);

    switch (fam) {
        case 0: family = QLatin1String("JAS_CLRSPC_FAM_UNKNOWN"); break;
        case 1: family = QLatin1String("JAS_CLRSPC_FAM_XYZ"); break;
        case 2: family = QLatin1String("JAS_CLRSPC_FAM_LAB"); break;
        case 3: family = QLatin1String("JAS_CLRSPC_FAM_GRAY"); break;
        case 4: family = QLatin1String("JAS_CLRSPC_FAM_RGB"); break;
        case 5: family = QLatin1String("JAS_CLRSPC_FAM_YCBCR"); break;
        default: family = QLatin1String("Unknown"); return false;
    }

    switch (mbr) {
        case 0:
            switch (fam) {
                case 1: specific = QLatin1String("JAS_CLRSPC_CIEXYZ"); break;
                case 2: specific = QLatin1String("JAS_CLRSPC_CIELAB"); break;
                case 3: specific = QLatin1String("JAS_CLRSPC_SGRAY"); break;
                case 4: specific = QLatin1String("JAS_CLRSPC_SRGB"); break;
                case 5: specific = QLatin1String("JAS_CLRSPC_SYCBCR"); break;
                default: specific = QLatin1String("Unknown"); return false;
            }
            break;
        case 1:
            switch (fam) {
                case 3: specific = QLatin1String("JAS_CLRSPC_GENGRAY"); break;
                case 4: specific = QLatin1String("JAS_CLRSPC_GENRGB"); break;
                case 5: specific = QLatin1String("JAS_CLRSPC_GENYCBCR"); break;
                default: specific = QLatin1String("Unknown"); return false;
            }
            break;
        default:
            return false;
    }
    return true;
}
/*!
    \internal
*/
void Jpeg2000JasperReader::printMetadata(jas_image_t *image)
{
#ifndef QT_NO_DEBUG
    // jas_image_cmptparm_t param
    qDebug("Image width: %ld", long(jas_image_width(image)));
    qDebug("Image height: %ld", long(jas_image_height(image)));
    qDebug("Coordinates on reference grid: (%ld,%ld) (%ld,%ld)",
           long(jas_image_tlx(image)), long(jas_image_tly(image)),
           long(jas_image_brx(image)), long(jas_image_bry(image)));
    qDebug("Number of image components: %d", jas_image_numcmpts(image));

    QString colorspaceFamily;
    QString colorspaceSpecific;
    decodeColorSpace(jas_image_clrspc(image), colorspaceFamily, colorspaceSpecific);
    qDebug("Color model (space): %d, %s - %s", jas_image_clrspc(image),
           qPrintable(colorspaceFamily), qPrintable(colorspaceSpecific));

    qDebug("Component metadata:");

    for (int c = 0; c < jas_image_numcmpts(image); ++c) {
        qDebug("Component %d:", c);
        qDebug("    Component type: %ld", long(jas_image_cmpttype(image, c)));
        qDebug("    Width: %ld", long(jas_image_cmptwidth(image, c)));
        qDebug("    Height: %ld", long(jas_image_cmptheight(image, c)));
        qDebug("    Signedness: %d", jas_image_cmptsgnd(image, c));
        qDebug("    Precision: %d", jas_image_cmptprec(image, c));
        qDebug("    Horizontal subsampling factor: %ld",long(jas_image_cmpthstep(image, c)));
        qDebug("    Vertical subsampling factor: %ld",  long(jas_image_cmptvstep(image, c)));
        qDebug("    Coordinates on reference grid: (%ld,%ld) (%ld,%ld)",
               long(jas_image_cmpttlx(image, c)), long(jas_image_cmpttly(image, c)),
               long(jas_image_cmptbrx(image, c)), long(jas_image_cmptbry(image, c)));
    }
#else
    Q_UNUSED(image);
#endif
}

QT_END_NAMESPACE
