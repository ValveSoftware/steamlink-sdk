/*****************************************************************************

        test.cpp
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



#if defined (_MSC_VER)
	#pragma warning (4 : 4786) // "identifier was truncated to '255' characters in the debug information"
	#pragma warning (4 : 4800) // "forcing value to bool 'true' or 'false' (performance warning)"
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"test_settings.h"
#include	"TestHelperFixLen.h"
#include	"TestHelperNormal.h"

#if defined (_MSC_VER)
#include	<crtdbg.h>
#include	<new.h>
#endif	// _MSC_VER

#include	<new>

#include	<cassert>
#include	<cstdio>



#define	TEST_


/*\\\ FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/



static int	TEST_perform_test_accuracy_all ();
static int	TEST_perform_test_speed_all ();

static void	TEST_prog_init ();
static void	TEST_prog_end ();



int main (int argc, char *argv [])
{
	using namespace std;

	int				ret_val = 0;

	TEST_prog_init ();

	try
	{
		if (ret_val == 0)
		{
			ret_val = TEST_perform_test_accuracy_all ();
		}

		if (ret_val == 0)
		{
			ret_val = TEST_perform_test_speed_all ();
		}
	}

	catch (std::exception &e)
	{
		printf ("\n*** main(): Exception (std::exception) : %s\n", e.what ());
		ret_val = -1;
	}

	catch (...)
	{
		printf ("\n*** main(): Undefined exception\n");
		ret_val = -1;
	}

	TEST_prog_end ();

	return (ret_val);
}



int	TEST_perform_test_accuracy_all ()
{
   int            ret_val = 0;

	TestHelperNormal <float >::perform_test_accuracy (ret_val);
	TestHelperNormal <double>::perform_test_accuracy (ret_val);

   TestHelperFixLen < 1>::perform_test_accuracy (ret_val);
   TestHelperFixLen < 2>::perform_test_accuracy (ret_val);
   TestHelperFixLen < 3>::perform_test_accuracy (ret_val);
   TestHelperFixLen < 4>::perform_test_accuracy (ret_val);
   TestHelperFixLen < 7>::perform_test_accuracy (ret_val);
   TestHelperFixLen < 8>::perform_test_accuracy (ret_val);
   TestHelperFixLen <10>::perform_test_accuracy (ret_val);
   TestHelperFixLen <12>::perform_test_accuracy (ret_val);
   TestHelperFixLen <13>::perform_test_accuracy (ret_val);

	return (ret_val);
}



int	TEST_perform_test_speed_all ()
{
   int            ret_val = 0;

#if defined (test_settings_SPEED_TEST_ENABLED)

	TestHelperNormal <float >::perform_test_speed (ret_val);
	TestHelperNormal <double>::perform_test_speed (ret_val);

   TestHelperFixLen < 1>::perform_test_speed (ret_val);
   TestHelperFixLen < 2>::perform_test_speed (ret_val);
   TestHelperFixLen < 3>::perform_test_speed (ret_val);
   TestHelperFixLen < 4>::perform_test_speed (ret_val);
   TestHelperFixLen < 7>::perform_test_speed (ret_val);
   TestHelperFixLen < 8>::perform_test_speed (ret_val);
   TestHelperFixLen <10>::perform_test_speed (ret_val);
   TestHelperFixLen <12>::perform_test_speed (ret_val);
   TestHelperFixLen <14>::perform_test_speed (ret_val);
   TestHelperFixLen <16>::perform_test_speed (ret_val);
   TestHelperFixLen <20>::perform_test_speed (ret_val);

#endif

   return (ret_val);
}



#if defined (_MSC_VER)
static int __cdecl	TEST_new_handler_cb (size_t dummy)
{
	throw std::bad_alloc ();
	return (0);
}
#endif	// _MSC_VER



#if defined (_MSC_VER) && ! defined (NDEBUG)
static int	__cdecl	TEST_debug_alloc_hook_cb (int alloc_type, void *user_data_ptr, size_t size, int block_type, long request_nbr, const unsigned char *filename_0, int line_nbr)
{
	if (block_type != _CRT_BLOCK)	// Ignore CRT blocks to prevent infinite recursion
	{
		switch (alloc_type)
		{
		case	_HOOK_ALLOC:
		case	_HOOK_REALLOC:
		case	_HOOK_FREE:

			// Put some debug code here

			break;

		default:
			assert (false);	// Undefined allocation type
			break;
		}
	}

	return (1);
}
#endif



#if defined (_MSC_VER) && ! defined (NDEBUG)
static int	__cdecl	TEST_debug_report_hook_cb (int report_type, char *user_msg_0, int *ret_val_ptr)
{
	*ret_val_ptr = 0;	// 1 to override the CRT default reporting mode

	switch (report_type)
	{
	case	_CRT_WARN:
	case	_CRT_ERROR:
	case	_CRT_ASSERT:

// Put some debug code here

		break;
	}

	return (*ret_val_ptr);
}
#endif



static void	TEST_prog_init ()
{
#if defined (_MSC_VER)
	::_set_new_handler (::TEST_new_handler_cb);
#endif	// _MSC_VER

#if defined (_MSC_VER) && ! defined (NDEBUG)
	{
		const int	mode =   (1 * _CRTDBG_MODE_DEBUG)
						       | (1 * _CRTDBG_MODE_WNDW);
		::_CrtSetReportMode (_CRT_WARN, mode);
		::_CrtSetReportMode (_CRT_ERROR, mode);
		::_CrtSetReportMode (_CRT_ASSERT, mode);

		const int	old_flags = ::_CrtSetDbgFlag (_CRTDBG_REPORT_FLAG);
		::_CrtSetDbgFlag (  old_flags
		                  | (1 * _CRTDBG_LEAK_CHECK_DF)
		                  | (1 * _CRTDBG_CHECK_ALWAYS_DF));
		::_CrtSetBreakAlloc (-1);	// Specify here a memory bloc number
		::_CrtSetAllocHook (TEST_debug_alloc_hook_cb);
		::_CrtSetReportHook (TEST_debug_report_hook_cb);

		// Speed up I/O but breaks C stdio compatibility
//		std::cout.sync_with_stdio (false);
//		std::cin.sync_with_stdio (false);
//		std::cerr.sync_with_stdio (false);
//		std::clog.sync_with_stdio (false);
	}
#endif	// _MSC_VER, NDEBUG
}



static void	TEST_prog_end ()
{
#if defined (_MSC_VER) && ! defined (NDEBUG)
	{
		const int	mode =   (1 * _CRTDBG_MODE_DEBUG)
						       | (0 * _CRTDBG_MODE_WNDW);
		::_CrtSetReportMode (_CRT_WARN, mode);
		::_CrtSetReportMode (_CRT_ERROR, mode);
		::_CrtSetReportMode (_CRT_ASSERT, mode);

		::_CrtMemState	mem_state;
		::_CrtMemCheckpoint (&mem_state);
		::_CrtMemDumpStatistics (&mem_state);
	}
#endif	// _MSC_VER, NDEBUG
}



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
