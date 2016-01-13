/*****************************************************************************

        DynArray.hpp
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



#if defined (DynArray_CURRENT_CODEHEADER)
	#error Recursive inclusion of DynArray code header.
#endif
#define	DynArray_CURRENT_CODEHEADER

#if ! defined (DynArray_CODEHEADER_INCLUDED)
#define	DynArray_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	<cassert>



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class T>
DynArray <T>::DynArray ()
:	_data_ptr (0)
,	_len (0)
{
	// Nothing
}



template <class T>
DynArray <T>::DynArray (long size)
:	_data_ptr (0)
,	_len (0)
{
	assert (size >= 0);
	if (size > 0)
	{
		_data_ptr = new DataType [size];
		_len = size;
	}
}



template <class T>
DynArray <T>::~DynArray ()
{
	delete [] _data_ptr;
	_data_ptr = 0;
	_len = 0;
}



template <class T>
long	DynArray <T>::size () const
{
	return (_len);
}



template <class T>
void	DynArray <T>::resize (long size)
{
	assert (size >= 0);
	if (size > 0)
	{
		DataType *		old_data_ptr = _data_ptr;
		DataType *		tmp_data_ptr = new DataType [size];

		_data_ptr = tmp_data_ptr;
		_len = size;

		delete [] old_data_ptr;
	}
}



template <class T>
const typename DynArray <T>::DataType &	DynArray <T>::operator [] (long pos) const
{
	assert (pos >= 0);
	assert (pos < _len);

	return (_data_ptr [pos]);
}



template <class T>
typename DynArray <T>::DataType &	DynArray <T>::operator [] (long pos)
{
	assert (pos >= 0);
	assert (pos < _len);

	return (_data_ptr [pos]);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



#endif	// DynArray_CODEHEADER_INCLUDED

#undef DynArray_CURRENT_CODEHEADER



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
