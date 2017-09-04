/*****************************************************************************

        fnc.hpp
        Copyright (c) 2003 Laurent de Soras

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



#if defined (stopwatch_fnc_CURRENT_CODEHEADER)
	#error Recursive inclusion of fnc code header.
#endif
#define	stopwatch_fnc_CURRENT_CODEHEADER

#if ! defined (stopwatch_fnc_CODEHEADER_INCLUDED)
#define	stopwatch_fnc_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	<cassert>
#include	<cmath>

namespace std {}



namespace stopwatch
{



template <typename T>
inline T	min (T a, T b)
{
	return ((a < b) ? a : b);
}



template <typename T>
inline T	max (T a, T b)
{
	return ((b < a) ? a : b);
}



int	round_int (double x)
{
	using namespace std;

	return (static_cast <int> (floor (x + 0.5)));
}



}	// namespace stopwatch



#endif	// stopwatch_fnc_CODEHEADER_INCLUDED

#undef stopwatch_fnc_CURRENT_CODEHEADER



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
