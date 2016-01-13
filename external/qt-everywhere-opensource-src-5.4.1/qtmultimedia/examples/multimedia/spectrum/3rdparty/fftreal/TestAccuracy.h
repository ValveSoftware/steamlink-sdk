/*****************************************************************************

        TestAccuracy.h
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



#if ! defined (TestAccuracy_HEADER_INCLUDED)
#define	TestAccuracy_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class FO>
class TestAccuracy
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

	typedef	typename FO::DataType	DataType;
	typedef	long double	BigFloat;	// To get maximum accuracy during intermediate calculations

   static int		perform_test_single_object (FO &fft);
   static int		perform_test_d (FO &fft, const char *class_name_0);
   static int		perform_test_i (FO &fft, const char *class_name_0);
   static int		perform_test_di (FO &fft, const char *class_name_0);



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	enum {			NBR_ACC_TESTS	= 10 * 1000 * 1000	};
   enum {         MAX_NBR_TESTS  = 10000  };

   static void		compute_tf (DataType s [], const DataType x [], long length);
	static void		compute_itf (DataType x [], const DataType s [], long length);
	static int		compare_vect_display (const DataType x_ptr [], const DataType y_ptr [], long len, BigFloat &max_err_rel);
	static BigFloat
						compute_power (const DataType x_ptr [], long len);
	static BigFloat
						compute_power (const DataType x_ptr [], const DataType y_ptr [], long len);
	static void		compare_vect (const DataType x_ptr [], const DataType y_ptr [], BigFloat &power, long &max_err_pos, long len);



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						TestAccuracy ();
						~TestAccuracy ();
						TestAccuracy (const TestAccuracy &other);
	TestAccuracy &	operator = (const TestAccuracy &other);
	bool				operator == (const TestAccuracy &other);
	bool				operator != (const TestAccuracy &other);

};	// class TestAccuracy



#include	"TestAccuracy.hpp"



#endif	// TestAccuracy_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
