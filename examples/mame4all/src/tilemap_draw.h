/***********************************************************************************/
/*
	The following procedure body is #included several times by
	tilemap.c to implement a suite of tilemap_draw subroutines.

	The constants TILE_WIDTH and TILE_HEIGHT are different in
	each instance of this code, allowing arithmetic shifts to
	be used by the compiler instead of multiplies/divides.

	This routine should be fairly optimal, for C code, though of
	course there is room for improvement.

	It renders pixels one row at a time, skipping over runs of totally
	transparent tiles, and calling custom blitters to handle runs of
	masked/totally opaque tiles.
*/

DECLARE( draw, (int xpos, int ypos),
{
	int tilemap_priority_code = blit.tilemap_priority_code;
	int x1 = xpos;
	int y1 = ypos;
	int x2 = xpos+blit.source_width;
	int y2 = ypos+blit.source_height;

	/* clip source coordinates */
	if( x1<blit.clip_left ) x1 = blit.clip_left;
	if( x2>blit.clip_right ) x2 = blit.clip_right;
	if( y1<blit.clip_top ) y1 = blit.clip_top;
	if( y2>blit.clip_bottom ) y2 = blit.clip_bottom;

	if( x1<x2 && y1<y2 ){ /* do nothing if totally clipped */
		DATA_TYPE *dest_baseaddr = xpos + (DATA_TYPE *)blit.screen->line[y1];
		DATA_TYPE *dest_next;

		//int priority_bitmap_row_offset = priority_bitmap_line_offset*TILE_HEIGHT;
		int priority_bitmap_row_offset = priority_bitmap_line_offset<<(TILE_HEIGHT/8+2);
		UINT8 *priority_bitmap_baseaddr = xpos + (UINT8 *)priority_bitmap->line[y1];
		UINT8 *priority_bitmap_next;

		int priority = blit.tile_priority;
		const DATA_TYPE *source_baseaddr;
		const DATA_TYPE *source_next;
		const UINT8 *mask_baseaddr;
		const UINT8 *mask_next;

		int c1;
		int c2; /* leftmost and rightmost visible columns in source tilemap */
		int y; /* current screen line to render */
		int y_next;

		/* convert screen coordinates to source tilemap coordinates */
		x1 -= xpos;
		y1 -= ypos;
		x2 -= xpos;
		y2 -= ypos;

		source_baseaddr = (DATA_TYPE *)blit.pixmap->line[y1];
		mask_baseaddr = blit.bitmask->line[y1];

		//c1 = x1/TILE_WIDTH;
		c1 = x1>>(TILE_WIDTH/8+2); /* round down */
		//c2 = (x2+TILE_WIDTH-1)/TILE_WIDTH;
		c2 = (x2+TILE_WIDTH-1)>>(TILE_WIDTH/8+2); /* round up */

		y = y1;
		//y_next = TILE_HEIGHT*(y1/TILE_HEIGHT) + TILE_HEIGHT;
		y_next = ((y1>>(TILE_HEIGHT/8+2))<<(TILE_HEIGHT/8+2)) + TILE_HEIGHT;
		if( y_next>y2 ) y_next = y2;

		{
			int dy = y_next-y;
			dest_next = dest_baseaddr + dy*blit.dest_line_offset;
			priority_bitmap_next = priority_bitmap_baseaddr + dy*priority_bitmap_line_offset;
			source_next = source_baseaddr + dy*blit.source_line_offset;
			mask_next = mask_baseaddr + dy*blit.mask_line_offset;
		}

		for(;;){
			//int row = y/TILE_HEIGHT;
			int row = y>>(TILE_HEIGHT/8+2);
			UINT8 *mask_data = blit.mask_data_row[row];
			UINT8 *priority_data = blit.priority_data_row[row];

			int tile_type;
			int prev_tile_type = TILE_TRANSPARENT;

			int x_start = x1;
			int x_end;

			int column;
			for( column=c1; column<=c2; column++ ){
				if( column==c2 || priority_data[column]!=priority )
					tile_type = TILE_TRANSPARENT;
				else
					tile_type = mask_data[column];

				if( tile_type!=prev_tile_type ){
					//x_end = column*TILE_WIDTH;
					x_end = column<<(TILE_WIDTH/8+2);
					if( x_end<x1 ) x_end = x1;
					if( x_end>x2 ) x_end = x2;

					if( prev_tile_type != TILE_TRANSPARENT ){
						if( prev_tile_type == TILE_MASKED ){
							//int count = (x_end+7)/8 - x_start/8;
							int count = ((x_end+7)>>3) - (x_start>>3);
							//const UINT8 *mask0 = mask_baseaddr + x_start/8;
							const UINT8 *mask0 = mask_baseaddr + (x_start>>3);
							const DATA_TYPE *source0 = source_baseaddr + (x_start&0xfff8);
							DATA_TYPE *dest0 = dest_baseaddr + (x_start&0xfff8);
							UINT8 *pmap0 = priority_bitmap_baseaddr + (x_start&0xfff8);
							int i = y;
							for(;;){
								memcpybitmask( dest0, (UINT32 *)source0, mask0, count );
								memsetbitmask8( pmap0, tilemap_priority_code, mask0, count );
								if( ++i == y_next ) break;

								dest0 += blit.dest_line_offset;
								source0 += blit.source_line_offset;
								mask0 += blit.mask_line_offset;
								pmap0 += priority_bitmap_line_offset;
							}
						}
						else { /* TILE_OPAQUE */
							int num_pixels = x_end - x_start;
							DATA_TYPE *dest0 = dest_baseaddr+x_start;
							const DATA_TYPE *source0 = source_baseaddr+x_start;
							UINT8 *pmap0 = priority_bitmap_baseaddr + x_start;
							int i = y;
							for(;;){
								//memcpy( dest0, source0, num_pixels*sizeof(DATA_TYPE));
								memcpy( dest0, source0, num_pixels<<(sizeof(DATA_TYPE)-1) );
								memset( pmap0, tilemap_priority_code, num_pixels );
								if( ++i == y_next ) break;

								dest0 += blit.dest_line_offset;
								source0 += blit.source_line_offset;
								pmap0 += priority_bitmap_line_offset;
							}
						}
					}
					x_start = x_end;
				}

				prev_tile_type = tile_type;
			}

			if( y_next==y2 ) break; /* we are done! */

			priority_bitmap_baseaddr = priority_bitmap_next;
			dest_baseaddr = dest_next;
			source_baseaddr = source_next;
			mask_baseaddr = mask_next;

			y = y_next;
			y_next += TILE_HEIGHT;

			if( y_next>=y2 ){
				y_next = y2;
			}
			else {
				dest_next += blit.dest_row_offset;
				priority_bitmap_next += priority_bitmap_row_offset;
				source_next += blit.source_row_offset;
				mask_next += blit.mask_row_offset;
			}
		} /* process next row */
	} /* not totally clipped */
})

DECLARE( draw_opaque, (int xpos, int ypos),
{
	int tilemap_priority_code = blit.tilemap_priority_code;
	int x1 = xpos;
	int y1 = ypos;
	int x2 = xpos+blit.source_width;
	int y2 = ypos+blit.source_height;
	/* clip source coordinates */
	if( x1<blit.clip_left ) x1 = blit.clip_left;
	if( x2>blit.clip_right ) x2 = blit.clip_right;
	if( y1<blit.clip_top ) y1 = blit.clip_top;
	if( y2>blit.clip_bottom ) y2 = blit.clip_bottom;

	if( x1<x2 && y1<y2 ){ /* do nothing if totally clipped */
		UINT8 *priority_bitmap_baseaddr = xpos + (UINT8 *)priority_bitmap->line[y1];
		//int priority_bitmap_row_offset = priority_bitmap_line_offset*TILE_HEIGHT;
		int priority_bitmap_row_offset = priority_bitmap_line_offset<<(TILE_HEIGHT/8+2);

		int priority = blit.tile_priority;
		DATA_TYPE *dest_baseaddr = xpos + (DATA_TYPE *)blit.screen->line[y1];
		DATA_TYPE *dest_next;
		const DATA_TYPE *source_baseaddr;
		const DATA_TYPE *source_next;

		int c1;
		int c2; /* leftmost and rightmost visible columns in source tilemap */
		int y; /* current screen line to render */
		int y_next;

		/* convert screen coordinates to source tilemap coordinates */
		x1 -= xpos;
		y1 -= ypos;
		x2 -= xpos;
		y2 -= ypos;

		source_baseaddr = (DATA_TYPE *)blit.pixmap->line[y1];

		//c1 = x1/TILE_WIDTH;
		c1 = x1>>(TILE_WIDTH/8+2); /* round down */
		//c2 = (x2+TILE_WIDTH-1)/TILE_WIDTH;
		c2 = (x2+TILE_WIDTH-1)>>(TILE_WIDTH/8+2); /* round up */

		y = y1;
		//y_next = TILE_HEIGHT*(y1/TILE_HEIGHT) + TILE_HEIGHT;
		y_next = ((y1>>(TILE_HEIGHT/8+2))<<(TILE_HEIGHT/8+2)) + TILE_HEIGHT;
		if( y_next>y2 ) y_next = y2;

		{
			int dy = y_next-y;
			dest_next = dest_baseaddr + dy*blit.dest_line_offset;
			source_next = source_baseaddr + dy*blit.source_line_offset;
		}

		for(;;){
			//int row = y/TILE_HEIGHT;
			int row = y>>(TILE_HEIGHT/8+2);
			UINT8 *priority_data = blit.priority_data_row[row];

			int tile_type;
			int prev_tile_type = TILE_TRANSPARENT;

			int x_start = x1;
			int x_end;

			int column;
			for( column=c1; column<=c2; column++ ){
				if( column==c2 || priority_data[column]!=priority )
					tile_type = TILE_TRANSPARENT;
				else
					tile_type = TILE_OPAQUE;

				if( tile_type!=prev_tile_type ){
					//x_end = column*TILE_WIDTH;
					x_end = column<<(TILE_WIDTH/8+2);
					if( x_end<x1 ) x_end = x1;
					if( x_end>x2 ) x_end = x2;

					if( prev_tile_type != TILE_TRANSPARENT ){
						/* TILE_OPAQUE */
						int num_pixels = x_end - x_start;
						DATA_TYPE *dest0 = dest_baseaddr+x_start;
						UINT8 *pmap0 = priority_bitmap_baseaddr+x_start;
						const DATA_TYPE *source0 = source_baseaddr+x_start;
						int i = y;
						for(;;){
							//memcpy( dest0, source0, num_pixels*sizeof(DATA_TYPE) );
							memcpy( dest0, source0, num_pixels<<(sizeof(DATA_TYPE)-1) );
							memset( pmap0, tilemap_priority_code, num_pixels );
							if( ++i == y_next ) break;

							dest0 += blit.dest_line_offset;
							pmap0 += priority_bitmap_line_offset;
							source0 += blit.source_line_offset;
						}
					}
					x_start = x_end;
				}

				prev_tile_type = tile_type;
			}

			if( y_next==y2 ) break; /* we are done! */

			priority_bitmap_baseaddr += priority_bitmap_row_offset;
			dest_baseaddr = dest_next;
			source_baseaddr = source_next;

			y = y_next;
			y_next += TILE_HEIGHT;

			if( y_next>=y2 ){
				y_next = y2;
			}
			else {
				dest_next += blit.dest_row_offset;
				source_next += blit.source_row_offset;
			}
		} /* process next row */
	} /* not totally clipped */
})

#undef TILE_WIDTH
#undef TILE_HEIGHT
#undef DATA_TYPE
#undef memcpybitmask
#undef DECLARE
