/*****************************************************************************

        FFTRealUseTrigo.hpp
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



#if defined (FFTRealUseTrigo_CURRENT_CODEHEADER)
	#error Recursive inclusion of FFTRealUseTrigo code header.
#endif
#define	FFTRealUseTrigo_CURRENT_CODEHEADER

#if ! defined (FFTRealUseTrigo_CODEHEADER_INCLUDED)
#define	FFTRealUseTrigo_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"OscSinCos.h"



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <int ALGO>
void	FFTRealUseTrigo <ALGO>::prepare (OscType &osc)
{
	osc.clear_buffers ();
}

template <>
void	FFTRealUseTrigo <0>::prepare (OscType &osc)
{
	// Nothing
}



template <int ALGO>
void	FFTRealUseTrigo <ALGO>::iterate (OscType &osc, DataType &c, DataType &s, const DataType cos_ptr [], long index_c, long index_s)
{
	osc.step ();
	c = osc.get_cos ();
	s = osc.get_sin ();
}

template <>
void	FFTRealUseTrigo <0>::iterate (OscType &osc, DataType &c, DataType &s, const DataType cos_ptr [], long index_c, long index_s)
{
	c = cos_ptr [index_c];
	s = cos_ptr [index_s];
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



#endif	// FFTRealUseTrigo_CODEHEADER_INCLUDED

#undef FFTRealUseTrigo_CURRENT_CODEHEADER



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
