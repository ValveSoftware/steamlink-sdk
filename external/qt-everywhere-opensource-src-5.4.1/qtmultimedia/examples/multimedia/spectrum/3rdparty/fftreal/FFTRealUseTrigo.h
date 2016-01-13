/*****************************************************************************

        FFTRealUseTrigo.h
        Copyright (c) 2005 Laurent de Soras

Template parameters:
	- ALGO: algorithm choice. 0 = table, other = oscillator

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



#if ! defined (FFTRealUseTrigo_HEADER_INCLUDED)
#define	FFTRealUseTrigo_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"def.h"
#include	"FFTRealFixLenParam.h"
#include	"OscSinCos.h"



template <int ALGO>
class FFTRealUseTrigo
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

   typedef	FFTRealFixLenParam::DataType	DataType;
	typedef	OscSinCos <DataType>	OscType;

	FORCEINLINE static void
						prepare (OscType &osc);
	FORCEINLINE	static void
						iterate (OscType &osc, DataType &c, DataType &s, const DataType cos_ptr [], long index_c, long index_s);



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						FFTRealUseTrigo ();
						~FFTRealUseTrigo ();
						FFTRealUseTrigo (const FFTRealUseTrigo &other);
	FFTRealUseTrigo &
						operator = (const FFTRealUseTrigo &other);
	bool				operator == (const FFTRealUseTrigo &other);
	bool				operator != (const FFTRealUseTrigo &other);

};	// class FFTRealUseTrigo



#include	"FFTRealUseTrigo.hpp"



#endif	// FFTRealUseTrigo_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
