/*****************************************************************************

        ClockCycleCounter.h
        Copyright (c) 2003 Laurent de Soras

Instrumentation class, for accurate time interval measurement. You may have
to modify the implementation to adapt it to your system and/or compiler.

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



#if ! defined (stopwatch_ClockCycleCounter_HEADER_INCLUDED)
#define	stopwatch_ClockCycleCounter_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"def.h"
#include	"Int64.h"



namespace stopwatch
{



class ClockCycleCounter
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

						ClockCycleCounter ();

	stopwatch_FORCEINLINE void
						start ();
	stopwatch_FORCEINLINE void
						stop_lap ();
	Int64				get_time_total () const;
	Int64				get_time_best_lap () const;



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	void				compute_clk_mul ();
	void				compute_measure_time_total ();
	void				compute_measure_time_lap ();

	static void		spend_time ();
	static stopwatch_FORCEINLINE Int64
						read_clock_counter ();

	Int64				_start_time;
	Int64				_state;
	Int64				_best_score;

	static Int64	_measure_time_total;
	static Int64	_measure_time_lap;
	static int		_clk_mul;
	static bool		_init_flag;



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						ClockCycleCounter (const ClockCycleCounter &other);
	ClockCycleCounter &
						operator = (const ClockCycleCounter &other);
	bool				operator == (const ClockCycleCounter &other);
	bool				operator != (const ClockCycleCounter &other);

};	// class ClockCycleCounter



}	// namespace stopwatch



#include	"ClockCycleCounter.hpp"



#endif	// stopwatch_ClockCycleCounter_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
