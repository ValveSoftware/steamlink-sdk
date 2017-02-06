/*****************************************************************************

        TestHelperNormal.hpp
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



#if defined (TestHelperNormal_CURRENT_CODEHEADER)
	#error Recursive inclusion of TestHelperNormal code header.
#endif
#define	TestHelperNormal_CURRENT_CODEHEADER

#if ! defined (TestHelperNormal_CODEHEADER_INCLUDED)
#define	TestHelperNormal_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"test_settings.h"

#include	"TestAccuracy.h"
#if defined (test_settings_SPEED_TEST_ENABLED)
	#include	"TestSpeed.h"
#endif



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class DT>
void	TestHelperNormal <DT>::perform_test_accuracy (int &ret_val)
{
	const int		len_arr [] = { 1, 2, 3, 4, 7, 8, 10, 12 };
	const int		nbr_len = sizeof (len_arr) / sizeof (len_arr [0]);
	for (int k = 0; k < nbr_len && ret_val == 0; ++k)
	{
		const long		len = 1L << (len_arr [k]);
		FftType			fft (len);
		ret_val = TestAccuracy <FftType>::perform_test_single_object (fft);
	}
}



template <class DT>
void	TestHelperNormal <DT>::perform_test_speed (int &ret_val)
{
#if defined (test_settings_SPEED_TEST_ENABLED)

	const int		len_arr [] = { 1, 2, 3, 4, 7, 8, 10, 12, 14, 16, 18, 20, 22 };
	const int		nbr_len = sizeof (len_arr) / sizeof (len_arr [0]);
	for (int k = 0; k < nbr_len && ret_val == 0; ++k)
	{
		const long		len = 1L << (len_arr [k]);
		FftType			fft (len);
		ret_val = TestSpeed <FftType>::perform_test_single_object (fft);
	}

#endif
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



#endif	// TestHelperNormal_CODEHEADER_INCLUDED

#undef TestHelperNormal_CURRENT_CODEHEADER



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
