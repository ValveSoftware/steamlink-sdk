
#ifndef MAME_PNG_H
#define MAME_PNG_H


#define PNG_Signature       "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A"

#define PNG_CN_IHDR 0x49484452L     /* Chunk names */
#define PNG_CN_PLTE 0x504C5445L
#define PNG_CN_IDAT 0x49444154L
#define PNG_CN_IEND 0x49454E44L
#define PNG_CN_gAMA 0x67414D41L
#define PNG_CN_sBIT 0x73424954L
#define PNG_CN_cHRM 0x6348524DL
#define PNG_CN_tRNS 0x74524E53L
#define PNG_CN_bKGD 0x624B4744L
#define PNG_CN_hIST 0x68495354L
#define PNG_CN_tEXt 0x74455874L
#define PNG_CN_zTXt 0x7A545874L
#define PNG_CN_pHYs 0x70485973L
#define PNG_CN_oFFs 0x6F464673L
#define PNG_CN_tIME 0x74494D45L
#define PNG_CN_sCAL 0x7343414CL

#define PNG_PF_None     0   /* Prediction filters */
#define PNG_PF_Sub      1
#define PNG_PF_Up       2
#define PNG_PF_Average  3
#define PNG_PF_Paeth    4

/* PNG support */
struct png_info {
	UINT32 width, height;
	UINT32 xoffset, yoffset;
	UINT32 xres, yres;
	double xscale, yscale;
	double source_gamma;
	UINT32 chromaticities[8];
	UINT32 resolution_unit, offset_unit, scale_unit;
	UINT8 bit_depth;
	UINT32 significant_bits[4];
	UINT32 background_color[4];
	UINT8 color_type;
	UINT8 compression_method;
	UINT8 filter_method;
	UINT8 interlace_method;
	UINT32 num_palette;
	UINT8 *palette;
	UINT32 num_trans;
	UINT8 *trans;
	UINT8 *image;

	/* The rest is private and should not be used
	 * by the public functions
	 */
	UINT8 bpp;
	UINT32 rowbytes;
	UINT8 *zimage;
	UINT32 zlength;
	UINT8 *fimage;
};

int png_verify_signature (void *fp);
int png_inflate_image (struct png_info *p);
int png_read_file(void *fp, struct png_info *p);
int png_expand_buffer_8bit (struct png_info *p);
void png_delete_unused_colors (struct png_info *p);
int png_unfilter(struct png_info *p);

int png_write_bitmap(void *fp, struct osd_bitmap *bitmap);

#endif

