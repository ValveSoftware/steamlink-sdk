/*****************************************************************************

        FFTRealPassDirect.h
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



#if ! defined (FFTRealPassDirect_HEADER_INCLUDED)
#define	FFTRealPassDirect_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"def.h"
#include	"FFTRealFixLenParam.h"
#include	"OscSinCos.h"



template <int PASS>
class FFTRealPassDirect
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

   typedef	FFTRealFixLenParam::DataType	DataType;
	typedef	OscSinCos <DataType>	OscType;

	FORCEINLINE static void
						process (long len, DataType dest_ptr [], DataType src_ptr [], const DataType x_ptr [], const DataType cos_ptr [], long cos_len, const long br_ptr [], OscType osc_list []);



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						FFTRealPassDirect ();
						~FFTRealPassDirect ();
						FFTRealPassDirect (const FFTRealPassDirect &other);
	FFTRealPassDirect &
						operator = (const FFTRealPassDirect &other);
	bool				operator == (const FFTRealPassDirect &other);
	bool				operator != (const FFTRealPassDirect &other);

};	// class FFTRealPassDirect



#include	"FFTRealPassDirect.hpp"



#endif	// FFTRealPassDirect_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
