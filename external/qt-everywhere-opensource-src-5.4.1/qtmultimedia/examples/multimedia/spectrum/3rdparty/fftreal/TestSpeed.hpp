/*****************************************************************************

        TestSpeed.hpp
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



#if defined (TestSpeed_CURRENT_CODEHEADER)
	#error Recursive inclusion of TestSpeed code header.
#endif
#define	TestSpeed_CURRENT_CODEHEADER

#if ! defined (TestSpeed_CODEHEADER_INCLUDED)
#define	TestSpeed_CODEHEADER_INCLUDED



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"test_fnc.h"
#include	"stopwatch/StopWatch.h"
#include	"TestWhiteNoiseGen.h"

#include	<typeinfo>

#include	<cstdio>



/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



template <class FO>
int	TestSpeed <FO>::perform_test_single_object (FO &fft)
{
	assert (&fft != 0);

   int            ret_val = 0;

	const std::type_info &	ti = typeid (fft);
	const char *	class_name_0 = ti.name ();

   if (ret_val == 0)
   {
	   perform_test_d (fft, class_name_0);
   }
   if (ret_val == 0)
   {
	   perform_test_i (fft, class_name_0);
   }
	if (ret_val == 0)
   {
      perform_test_di (fft, class_name_0);
   }

   if (ret_val == 0)
   {
      printf ("\n");
   }

   return (ret_val);
}



template <class FO>
int	TestSpeed <FO>::perform_test_d (FO &fft, const char *class_name_0)
{
	assert (&fft != 0);
   assert (class_name_0 != 0);

	const long		len = fft.get_length ();
   const long     nbr_tests = limit (
      static_cast <long> (NBR_SPD_TESTS / len / len),
      1L,
      static_cast <long> (MAX_NBR_TESTS)
   );

	TestWhiteNoiseGen <DataType>	noise;
	std::vector <DataType>	x (len, 0);
	std::vector <DataType>	s (len);
	noise.generate (&x [0], len);

   printf (
		"%s::do_fft () speed test [%ld samples]... ",
		class_name_0,
		len
	);
	fflush (stdout);

	stopwatch::StopWatch	chrono;
	chrono.start ();
	for (long test = 0; test < nbr_tests; ++ test)
	{
		fft.do_fft (&s [0], &x [0]);
		chrono.stop_lap ();
	}

	printf ("%.1f clocks/sample\n", chrono.get_time_best_lap (len));

	return (0);
}



template <class FO>
int	TestSpeed <FO>::perform_test_i (FO &fft, const char *class_name_0)
{
	assert (&fft != 0);
   assert (class_name_0 != 0);

	const long		len = fft.get_length ();
   const long     nbr_tests = limit (
      static_cast <long> (NBR_SPD_TESTS / len / len),
      1L,
      static_cast <long> (MAX_NBR_TESTS)
   );

	TestWhiteNoiseGen <DataType>	noise;
	std::vector <DataType>	x (len);
	std::vector <DataType>	s (len, 0);
	noise.generate (&s [0], len);

   printf (
		"%s::do_ifft () speed test [%ld samples]... ",
		class_name_0,
		len
	);
	fflush (stdout);

	stopwatch::StopWatch	chrono;
	chrono.start ();
	for (long test = 0; test < nbr_tests; ++ test)
	{
		fft.do_ifft (&s [0], &x [0]);
		chrono.stop_lap ();
	}

	printf ("%.1f clocks/sample\n", chrono.get_time_best_lap (len));

	return (0);
}



template <class FO>
int	TestSpeed <FO>::perform_test_di (FO &fft, const char *class_name_0)
{
	assert (&fft != 0);
   assert (class_name_0 != 0);

	const long		len = fft.get_length ();
   const long     nbr_tests = limit (
      static_cast <long> (NBR_SPD_TESTS / len / len),
      1L,
      static_cast <long> (MAX_NBR_TESTS)
   );

	TestWhiteNoiseGen <DataType>	noise;
	std::vector <DataType>	x (len, 0);
	std::vector <DataType>	s (len);
	std::vector <DataType>	y (len);
	noise.generate (&x [0], len);

   printf (
		"%s::do_fft () / do_ifft () / rescale () speed test [%ld samples]... ",
		class_name_0,
		len
	);
	fflush (stdout);

	stopwatch::StopWatch	chrono;

	chrono.start ();
	for (long test = 0; test < nbr_tests; ++ test)
	{
		fft.do_fft (&s [0], &x [0]);
		fft.do_ifft (&s [0], &y [0]);
		fft.rescale (&y [0]);
		chrono.stop_lap ();
	}

	printf ("%.1f clocks/sample\n", chrono.get_time_best_lap (len));

	return (0);
}



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



#endif	// TestSpeed_CODEHEADER_INCLUDED

#undef TestSpeed_CURRENT_CODEHEADER



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
