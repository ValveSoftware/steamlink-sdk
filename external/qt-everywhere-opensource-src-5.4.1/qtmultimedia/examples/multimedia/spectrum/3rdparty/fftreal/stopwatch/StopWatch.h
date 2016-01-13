/*****************************************************************************

        StopWatch.h
        Copyright (c) 2005 Laurent de Soras

Utility class based on ClockCycleCounter to measure the unit time of a
repeated operation.

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



#if ! defined (stopwatch_StopWatch_HEADER_INCLUDED)
#define	stopwatch_StopWatch_HEADER_INCLUDED

#if defined (_MSC_VER)
	#pragma once
	#pragma warning (4 : 4250) // "Inherits via dominance."
#endif



/*\\\ INCLUDE FILES \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

#include	"ClockCycleCounter.h"



namespace stopwatch
{



class StopWatch
{

/*\\\ PUBLIC \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

public:

						StopWatch ();

	stopwatch_FORCEINLINE void
						start ();
	stopwatch_FORCEINLINE void
						stop_lap ();

	double			get_time_total (Int64 nbr_op) const;
	double			get_time_best_lap (Int64 nbr_op) const;



/*\\\ PROTECTED \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

protected:



/*\\\ PRIVATE \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

	ClockCycleCounter
						_ccc;
	Int64				_nbr_laps;



/*\\\ FORBIDDEN MEMBER FUNCTIONS \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/

private:

						StopWatch (const StopWatch &other);
	StopWatch &		operator = (const StopWatch &other);
	bool				operator == (const StopWatch &other);
	bool				operator != (const StopWatch &other);

};	// class StopWatch



}	// namespace stopwatch



#include	"StopWatch.hpp"



#endif	// stopwatch_StopWatch_HEADER_INCLUDED



/*\\\ EOF \\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\\*/
