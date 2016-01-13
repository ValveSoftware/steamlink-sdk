/*******************************************************************************
* Copyright (C) Marvell International Ltd. and its affiliates
*
* Marvell GPL License Option
*
* If you received this File from Marvell, you may opt to use, redistribute and/or
* modify this File in accordance with the terms and conditions of the General
* Public License Version 2, June 1991 (the "GPL License"), a copy of which is
* available along with the File in the license.txt file or by writing to the Free
* Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
* on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
*
* THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
* WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
* DISCLAIMED.  The GPL License provides additional details about this warranty
* disclaimer.
********************************************************************************/



#ifndef __VPP_API_TYPES_H__
#define __VPP_API_TYPES_H__

/*-----------------------------------------------------------------------------
 * Macros and Constants
 *-----------------------------------------------------------------------------
 */

/*-----------------------------------------------------------------------------
 * HDMI Notification Events
 *-----------------------------------------------------------------------------
 */
#define MV_THINVPP_EVENT_HDMI_SINK_CONNECTED        0
#define MV_THINVPP_EVENT_HDMI_SINK_DISCONNECTED     1
#define MV_THINVPP_EVENT_HDMI_VIDEO_CFG_ERR         2
#define MV_THINVPP_EVENT_HDMI_AUDIO_CFG_ERR         3
#define MV_THINVPP_EVENT_HDMI_HDCP_ERR              4

/*-----------------------------------------------------------------------------
 * HDMI Aux packet enums, structures
 *-----------------------------------------------------------------------------
 */
typedef enum tagVPP_HDMI_PKT_ID
{
    VPP_HDMI_PKT_ID_ACP                 = 0x04,
    VPP_HDMI_PKT_ID_ISRC1               = 0x05,
    VPP_HDMI_PKT_ID_ISRC2               = 0x06,
    VPP_HDMI_PKT_ID_GAMUT_METADATA      = 0x0A,
    VPP_HDMI_PKT_ID_VENDOR_INFOFRAME    = 0x81,
    VPP_HDMI_PKT_ID_AVI_INFOFRAME       = 0x82,
    VPP_HDMI_PKT_ID_SPD_INFOFRAME       = 0x83,
    VPP_HDMI_PKT_ID_AUDIO_INFOFRAME     = 0x84,
    VPP_HDMI_PKT_ID_MPEG_SRC_INFOFRAME  = 0x85,
}VPP_HDMI_PKT_ID;

typedef struct tagVPP_HDMI_PKT_GMD
{
    int                        nextField;
    int                        noCurGBD;
    int                         gdbProfile; // VPP_HDMI_GDB_PROFILE
    int                         seqInfo; // VPP_HDMI_PKT_GMD_SEQ_INFO
    unsigned char                       affectedGamutSeqNum;
    unsigned char                       curGamutSeqNum;
    unsigned char                       gbdData[28];
}VPP_HDMI_PKT_GMD;

// ACP packet
typedef enum tagVPP_HDMI_PKT_ACP_TYPE
{
    VPP_HDMI_PKT_ACP_GENERIC_AUDIO = 0x00,
    VPP_HDMI_PKT_ACP_IEC60958,
    VPP_HDMI_PKT_ACP_DVD_AUDIO,
    VPP_HDMI_PKT_ACP_SACD,
    VPP_HDMI_PKT_ACP_MAX,
}VPP_HDMI_PKT_ACP_TYPE;

typedef struct tagVPP_HDMI_PKT_ACP
{
    int                     type; // VPP_HDMI_PKT_ACP_TYPE
    // Valid only for DVD Audio and SACD
    unsigned char                   dataLen;
    unsigned char                   dataBuf[16];
}VPP_HDMI_PKT_ACP;

// ISRC Packet
typedef enum tagVPP_HDMI_PKT_ISRC_STS
{
    VPP_HDMI_PKT_ISRC_STS_START_POS = 0x01,
    VPP_HDMI_PKT_ISRC_STS_INTER_POS = 0x02,
    VPP_HDMI_PKT_ISRC_STS_END_POS   = 0x04,
}VPP_HDMI_PKT_ISRC_STS;

typedef struct tagVPP_HDMI_PKT_ISRC1
{
    int                    cont;
    int                     sts; // VPP_HDMI_PKT_ISRC_STS
    int                    valid;
    unsigned char                   upc_ean_fld[16];
}VPP_HDMI_PKT_ISRC1;

typedef struct tagVPP_HDMI_PKT_ISRC2
{
    unsigned char                   upc_ean_fld[16];
}VPP_HDMI_PKT_ISRC2;

// SPD InfoFrame
typedef enum tagVPP_HDMI_CEA_SRC_DEV_TYPE
{
    VPP_HDMI_CEA_SRC_DEV_UNKNOWN = 0x00,
    VPP_HDMI_CEA_SRC_DEV_DIG_STB,
    VPP_HDMI_CEA_SRC_DEV_DVD_PLAYER,
    VPP_HDMI_CEA_SRC_DEV_DVHS,
    VPP_HDMI_CEA_SRC_DEV_HDD_REC,
    VPP_HDMI_CEA_SRC_DEV_DVC,
    VPP_HDMI_CEA_SRC_DEV_DSC,
    VPP_HDMI_CEA_SRC_DEV_VIDEO_CD,
    VPP_HDMI_CEA_SRC_DEV_GAME,
    VPP_HDMI_CEA_SRC_DEV_PC,
    VPP_HDMI_CEA_SRC_DEV_BD_PLAYER,
    VPP_HDMI_CEA_SRC_DEV_SACD,
    VPP_HDMI_CEA_SRC_DEV_MAX
}VPP_HDMI_CEA_SRC_DEV_TYPE;

typedef struct tagVPP_HDMI_PKT_SPD_INFOFRM {
    unsigned char                       vendorName[8];
    unsigned char                       prodDescChar[16];
    int   srcDev; // VPP_HDMI_CEA_SRC_DEV_TYPE
}VPP_HDMI_PKT_SPD_INFOFRM;

// MPEG source InfoFrame
typedef enum tagVPP_HDMI_CEA_MPG_FRM_TYPE
{
    VPP_HDMI_MPEG_FRM_UNKNOWN = 0x00,
    VPP_HDMI_MPEG_FRM_I_PICTURE,
    VPP_HDMI_MPEG_FRM_B_PICTURE,
    VPP_HDMI_MPEG_FRM_P_PICTURE,
    VPP_HDMI_MPEG_FRM_MAX,
}VPP_HDMI_CEA_MPG_FRM_TYPE;

typedef struct tagVPP_HDMI_PKT_MPEG_SRC_INFOFRM
{
    unsigned int                      bitRate;
    int                         mpegFrameType; // VPP_HDMI_CEA_MPG_FRM_TYPE
    int                        repeatedField;
}VPP_HDMI_PKT_MPEG_SRC_INFOFRM;

// Vendor Specific InfoFrame
typedef enum tagVPP_HDMI_VIDEO_FORMAT
{
	VPP_HDMI_VIDEO_FMT_NO_INFO = 0x00,
	VPP_HDMI_VIDEO_FMT_EXTENDED_RES_FMT,
	VPP_HDMI_VIDEO_FMT_3D_FMT,
	VPP_HDMI_VIDEO_FMT_RESERVED,
}VPP_HDMI_VIDEO_FORMAT;

typedef enum tagVPP_HDMI_VIC
{
	VPP_HDMI_VIC_RESERVED1 = 0x00,
	VPP_HDMI_VIC_4K_2K_30,
	VPP_HDMI_VIC_4K_2K_25,
	VPP_HDMI_VIC_4K_2K_24,
	VPP_HDMI_VIC_4K_2K_24_SMPTE,
	VPP_HDMI_VIC_RESERVED2,
}VPP_HDMI_VIC;

typedef enum tagVPP_HDMI_3D_STRUCTURE
{
	VPP_HDMI_3D_STRUCT_FRAME_PACKING=0x00,
	VPP_HDMI_3D_STRUCT_FIELD_ALTERNATIVE=0x01,
	VPP_HDMI_3D_STRUCT_LINE_ALTERNATIVE=0x02,
	VPP_HDMI_3D_STRUCT_SIDE_BY_SIDE_FULL=0x03,
	VPP_HDMI_3D_STRUCT_L_DEPTH=0x04,
	VPP_HDMI_3D_STRUCT_L_DEPTH_GFX_GDEPTH=0x05,
    VPP_HDMI_3D_STRUCT_TOP_AND_BOTTOM=0x06,
    VPP_HDMI_3D_STRUCT_RESERVED2=0x07,
	VPP_HDMI_3D_STRUCT_SIDE_BY_SIDE_HALF=0x08,
    VPP_HDMI_3D_STRUCT_RESERVED3=0x09,
}VPP_HDMI_3D_STRUCTURE;

typedef enum tagVPP_HDMI_3D_METADATA_TYPE
{
	VPP_HDMI_3D_METADATA_TYPE_PARALLAX_INFO = 0x00,
	VPP_HDMI_3D_METADATA_TYPE_RESERVED = 0x01,
}VPP_HDMI_3D_METADATA_TYPE;

typedef enum tagVPP_HDMI_3D_EXT_DATA
{
	VPP_HDMI_HORZ_SUBSAMP_3D_EXT_DATA_OL_OR=0x00,
	VPP_HDMI_HORZ_SUBSAMP_3D_EXT_DATA_OL_ER,
	VPP_HDMI_HORZ_SUBSAMP_3D_EXT_DATA_EL_OR,
	VPP_HDMI_HORZ_SUBSAMP_3D_EXT_DATA_EL_ER,

	VPP_HDMI_QUIN_3D_EXT_DATA_OL_OR=0x04,
	VPP_HDMI_QUIN_3D_EXT_DATA_OL_ER,
	VPP_HDMI_QUIN_3D_EXT_DATA_EL_OR,
	VPP_HDMI_QUIN_3D_EXT_DATA_EL_ER,

	VPP_HDMI_3D_EXT_DATA_RESERVED = 0x08,

}VPP_HDMI_3D_EXT_DATA;

typedef struct tagVPP_HDMI_PKT_VNDRSPEC_INFOFRM
{
	unsigned int	ieeeRegID;
	unsigned char 	HdmiVideoFmt;//defines the structure of the extended video formats - //VPP_HDMI_EXTENDED_VIDEO_FORMAT
	unsigned char 	HDMI_VIC;//Video Identification Code - VPP_HDMI_VIC
	unsigned char 	Hdmi_3D_Structure;//Transmission format of 3D Video Data - VPP_HDMI_3D_STRUCTURE
	int 	Hdmi_3D_Meta_Present;//additional bytes of 3D Metadata are present in the Infoframe
	unsigned char 	Hdmi_3D_Ext_Data;//
	unsigned char 	Hdmi_3D_Metadata_Type;//Type of info of metadata whcich is used in the correct rendering of the stereoscopic video
	unsigned char 	Hdmi_3D_Metadata_Length;
	unsigned char 	Hdmi_3D_Metadata[16];//!WARN : Need to check
}VPP_HDMI_PKT_VNDRSPEC_INFOFRM;

// Aux packet structure
typedef union tagVPP_HDMI_PKT
{
    VPP_HDMI_PKT_ACP                acpPkt;
    VPP_HDMI_PKT_GMD                gmdPkt;
    VPP_HDMI_PKT_ISRC1              isrc1Pkt;
    VPP_HDMI_PKT_ISRC2              isrc2Pkt;
    VPP_HDMI_PKT_SPD_INFOFRM        spdInfoFrm;
    VPP_HDMI_PKT_MPEG_SRC_INFOFRM   mpegSrcInfoFrm;
    VPP_HDMI_PKT_VNDRSPEC_INFOFRM   vndrSpecInfoFrm;
}VPP_HDMI_PKT;

/*-----------------------------------------------------------------------------
 * HDMI Audio configuration structure
 *-----------------------------------------------------------------------------
 */
typedef struct VPP_HDMI_AUDIO_CFG_T {
    int     numChannels;
    int     portNum;
    int     sampFreq;
    int     sampSize;
    int     mClkFactor;
    int     audioFmt; /* VPP_HDMI_AUDIO_FMT_T */
    int     hbrAudio; /* True/False for controlling HBR audio */
}VPP_HDMI_AUDIO_CFG;

typedef enum tagVPP_HDMI_AUDIO_VUC_CFG_TYPE
{
    VPP_HDMI_VUC_CFG_SET_DEFAULT = 0x00,
    VPP_HDMI_VUC_CFG_UPDATE_WITH_GIVEN_CFG,
    VPP_HDMI_VUC_CFG_NO_UPDATE_TO_CURRENT_CFG,
    VPP_HDMI_VUC_CFG_MAX
}VPP_HDMI_AUDIO_VUC_CFG_TYPE;

typedef struct VPP_HDMI_AUDIO_VUC_CFG_T {
    // VPP_HDMI_AUDIO_VUC_CFG_TYPE
    unsigned char vBitCfg;
    unsigned char vBit;

    // VPP_HDMI_AUDIO_VUC_CFG_TYPE
    unsigned char uBitsCfg;
    unsigned char uBits[14];

    // VPP_HDMI_AUDIO_VUC_CFG_TYPE
    unsigned char cBitsCfg;
    unsigned char cBits[5];
}VPP_HDMI_AUDIO_VUC_CFG;

/*-----------------------------------------------------------------------------
 * HDMI Sink Capabilities structures
 *-----------------------------------------------------------------------------
 */
typedef enum VPP_HDMI_AUDIO_FMT_T
{
    VPP_HDMI_AUDIO_FMT_UNDEF = 0x00,
    VPP_HDMI_AUDIO_FMT_PCM   = 0x01,
    VPP_HDMI_AUDIO_FMT_AC3,
    VPP_HDMI_AUDIO_FMT_MPEG1,
    VPP_HDMI_AUDIO_FMT_MP3,
    VPP_HDMI_AUDIO_FMT_MPEG2,
    VPP_HDMI_AUDIO_FMT_AAC,
    VPP_HDMI_AUDIO_FMT_DTS,
    VPP_HDMI_AUDIO_FMT_ATRAC,
    VPP_HDMI_AUDIO_FMT_ONE_BIT_AUDIO,
    VPP_HDMI_AUDIO_FMT_DOLBY_DIGITAL_PLUS,
    VPP_HDMI_AUDIO_FMT_DTS_HD,
    VPP_HDMI_AUDIO_FMT_MAT,
    VPP_HDMI_AUDIO_FMT_DST,
    VPP_HDMI_AUDIO_FMT_WMA_PRO,
}VPP_HDMI_AUDIO_FMT;

typedef struct VPP_HDMI_RES_INFO_T {
    int     hActive;
    int     vActive;
    // Refresh rate in Hz, -1 if refresh rate is
    // undefined in the descriptor
    int     refreshRate;
    // 0 = progressive, 1 = interlaced,  2 = undefined
    int     interlaced;
} VPP_HDMI_RES_INFO;

typedef struct VPP_HDMI_AUDIO_FREQ_SPRT_T {
    unsigned char   Res         : 1;
    unsigned char   Fs32KHz     : 1;
    unsigned char   Fs44_1KHz   : 1;
    unsigned char   Fs48KHz     : 1;
    unsigned char   Fs88_2KHz   : 1;
    unsigned char   Fs96KHz     : 1;
    unsigned char   Fs176_4KHz  : 1;
    unsigned char   Fs192KHz    : 1;
} VPP_HDMI_AUDIO_FREQ_SPRT;

typedef struct VPP_HDMI_AUDIO_WDLEN_SPRT_T {
    unsigned char   Res1    : 1;
    unsigned char   WdLen16 : 1;
    unsigned char   WdLen20 : 1;
    unsigned char   WdLen24 : 1;
    unsigned char   Res2    : 4;
} VPP_HDMI_AUDIO_WDLEN_SPRT;

typedef struct VPP_HDMI_AUDIO_INFO_T {
    int                         audioFmt; // VPP_HDMI_AUDIO_FMT
    VPP_HDMI_AUDIO_FREQ_SPRT    freqSprt;
    // Field is valid only for compressed audio formats
    unsigned int                      maxBitRate; // in KHz
    // Field is valid only for LPCM
    VPP_HDMI_AUDIO_WDLEN_SPRT   wdLenSprt;
    unsigned char               maxNumChnls;
} VPP_HDMI_AUDIO_INFO;

typedef struct VPP_HDMI_SPKR_ALLOC_T {
    unsigned char   FlFr  : 1; // FrontLeft/Front Rear
    unsigned char   Lfe   : 1; // Low Frequency Effect
    unsigned char   Fc    : 1; // Front Center
    unsigned char   RlRr  : 1; // Rear Left/Rear Right
    unsigned char   Rc    : 1; // Rear Center
    unsigned char   FlcFrc: 1; // Front Left Center/Front Right Center
    unsigned char   RlcRrc: 1; // Rear Left Center /Rear Right Center
    unsigned char   Res   : 1;
} VPP_HDMI_SPKR_ALLOC;

// Calorimetry support
typedef struct tagVPP_HDMI_CALORIMETRY_INFO
{
    unsigned char   xvYCC601    : 1;
    unsigned char   xvYCC709    : 1;
    unsigned char   MD0         : 1;
    unsigned char   MD1         : 1;
    unsigned char   MD2         : 1;
    unsigned char   res         : 3;
}VPP_HDMI_CALORIMETRY_INFO;

// Pixel repetition info
typedef struct tagVPP_HDMI_PIXEL_REPT_INFO
{
    unsigned int    resMask     : 26;
    unsigned int    prSupport   : 6;
}VPP_HDMI_PIXEL_REPT_INFO;

// Frame buffer info
typedef struct vbuf_info_t
{
    void *   m_pbuf_start;          // base address of the frame buffer;
    unsigned m_buf_stride;           // line stride (in bytes) of the frame buffer
    unsigned m_active_left;          // x-coordination (in pixels) of active window top left in reference window
    unsigned m_active_top;           // y-coordination (in pixels) of active window top left in reference window
    unsigned m_active_width;         // with of active in pixels.
    unsigned m_active_height;        // height of active data in pixels.
    unsigned m_disp_offset;          //Offset (in bytes) of active data to be displayed

    unsigned bgcolor;    // background color of a vpp window
    unsigned alpha;      // global alpha of a vpp window
}VBUF_INFO;


#endif // __VPP_API_TYPES_H__
