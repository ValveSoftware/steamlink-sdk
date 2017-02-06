/*****************************************************************************

        FFTRealSelect.h
        Copyright (c) 2005 Laurent de Soras

--- Legal stuff ---

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*Tab=3***********************************************************************/



#if ! defined (FFTRealSelect_HEADER_INCLUDED)
#define	FFTRealSelect_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"def.h"



template <int P>
class FFTRealSelect
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	FORCEINLINE static float *
						sel_bin (float *e_ptr, float *o_ptr);



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						FFTRealSelect ();
						~FFTRealSelect ();
						FFTRealSelect (const FFTRealSelect &other);
	FFTRealSelect&	operator = (const FFTRealSelect &other);
	bool				operator == (const FFTRealSelect &other);
	bool				operator != (const FFTRealSelect &other);

};	// class FFTRealSelect



#include	"FFTRealSelect.hpp"



#endif	// FFTRealSelect_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
