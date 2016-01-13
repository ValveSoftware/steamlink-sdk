/*****************************************************************************

        TestWhiteNoiseGen.hpp
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



#if defined (TestWhiteNoiseGen_CURRENT_CODEHEADER)
	#error Recursive inclusion of TestWhiteNoiseGen code header.
#endif
#define	TestWhiteNoiseGen_CURRENT_CODEHEADER

#if ! defined (TestWhiteNoiseGen_CODEHEADER_INCLUDED)
#define	TestWhiteNoiseGen_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class DT>
TestWhiteNoiseGen <DT>::TestWhiteNoiseGen ()
:	_rand_state (0)
{
	_rand_state = reinterpret_cast <StateType> (this);
}



template <class DT>
void	TestWhiteNoiseGen <DT>::generate (DataType data_ptr [], long len)
{
	assert (data_ptr != 0);
	assert (len > 0);

	const DataType	one = static_cast <DataType> (1);
	const DataType	mul = one / static_cast <DataType> (0x80000000UL);

	long				pos = 0;
	do
	{
		const DataType	x = static_cast <DataType> (_rand_state & 0xFFFFFFFFUL);
		data_ptr [pos] = x * mul - one;

		_rand_state = _rand_state * 1234567UL + 890123UL;

		++ pos;
	}
	while (pos < len);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



#endif	// TestWhiteNoiseGen_CODEHEADER_INCLUDED

#undef TestWhiteNoiseGen_CURRENT_CODEHEADER



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
