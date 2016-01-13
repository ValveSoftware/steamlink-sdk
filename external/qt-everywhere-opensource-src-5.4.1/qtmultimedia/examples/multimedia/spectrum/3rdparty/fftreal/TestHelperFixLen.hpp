/*****************************************************************************

        TestHelperFixLen.hpp
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



#if defined (TestHelperFixLen_CURRENT_CODEHEADER)
	#error Recursive inclusion of TestHelperFixLen code header.
#endif
#define	TestHelperFixLen_CURRENT_CODEHEADER

#if ! defined (TestHelperFixLen_CODEHEADER_INCLUDED)
#define	TestHelperFixLen_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"test_settings.h"

#include	"TestAccuracy.h"
#if defined (test_settings_SPEED_TEST_ENABLED)
	#include	"TestSpeed.h"
#endif



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <int L>
void	TestHelperFixLen <L>::perform_test_accuracy (int &ret_val)
{
   if (ret_val == 0)
   {
		FftType			fft;
      ret_val = TestAccuracy <FftType>::perform_test_single_object (fft);
   }
}



template <int L>
void	TestHelperFixLen <L>::perform_test_speed (int &ret_val)
{
#if defined (test_settings_SPEED_TEST_ENABLED)

   if (ret_val == 0)
   {
		FftType			fft;
      ret_val = TestSpeed <FftType>::perform_test_single_object (fft);
   }

#endif
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



#endif	// TestHelperFixLen_CODEHEADER_INCLUDED

#undef TestHelperFixLen_CURRENT_CODEHEADER



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
