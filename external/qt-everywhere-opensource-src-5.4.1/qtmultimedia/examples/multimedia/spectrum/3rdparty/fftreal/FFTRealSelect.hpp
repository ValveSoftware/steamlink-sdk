/*****************************************************************************

        FFTRealSelect.hpp
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



#if defined (FFTRealSelect_CURRENT_CODEHEADER)
	#error Recursive inclusion of FFTRealSelect code header.
#endif
#define	FFTRealSelect_CURRENT_CODEHEADER

#if ! defined (FFTRealSelect_CODEHEADER_INCLUDED)
#define	FFTRealSelect_CODEHEADER_INCLUDED



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <int P>
float *	FFTRealSelect <P>::sel_bin (float *e_ptr, float *o_ptr)
{
	return (o_ptr);
}



template <>
float *	FFTRealSelect <0>::sel_bin (float *e_ptr, float *o_ptr)
{
	return (e_ptr);
}



#endif	// FFTRealSelect_CODEHEADER_INCLUDED

#undef FFTRealSelect_CURRENT_CODEHEADER



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
