/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the config.tests in the Qt ImageFormats module.
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

#include <webp/decode.h>
#include <webp/encode.h>
#include <webp/demux.h>

#if WEBP_ABI_IS_INCOMPATIBLE(WEBP_DECODER_ABI_VERSION, 0x0203) || WEBP_ABI_IS_INCOMPATIBLE(WEBP_ENCODER_ABI_VERSION, 0x0202)
#error "Incompatible libwebp version"
#endif

int main(int, char **)
{
    WebPDecoderConfig config;
    WebPDecBuffer *output_buffer = &config.output;
    WebPBitstreamFeatures *bitstream = &config.input;
    WebPPicture picture;
    picture.use_argb = 0;
    WebPConfig config2;
    config2.lossless = 0;
    WebPData data = {};
    WebPDemuxer *demuxer = WebPDemux(&data);
    WebPIterator iter;
    iter.frame_num = 0;

    return 0;
}
