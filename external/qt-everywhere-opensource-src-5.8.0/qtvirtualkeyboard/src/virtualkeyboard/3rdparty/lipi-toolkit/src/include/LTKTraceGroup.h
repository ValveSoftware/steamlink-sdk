/******************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE
********************************************************************************/

/******************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2008-07-18 15:00:39 +0530 (Fri, 18 Jul 2008) $
 * $Revision: 561 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/******************************************************************************
 * FILE DESCR: Definition of LTKTraceGroup which holds a sequence of LTKTrace
 *             type objects
 *
 * CONTENTS:
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date		   Description of change
 * Deepu V.     March 7,2005   Added new assignment operator
 *                             LTKTraceGroup (const LTKTrace& trace)
 *                             and copy constructor
 *                             LTKTraceGroup& operator = (const LTKTrace& trace)
 * Thanigai		09-AUG-2005	   Added a function to empty the trace group
 *****************************************************************************/

#ifndef __LTKTRACEGROUP_H
#define __LTKTRACEGROUP_H

#include "LTKTypes.h"

class LTKTrace;

/**
* @ingroup Common_Classes
*/

/** @brief Contains set of traces that have similar characteristic.
 * @class LTKTraceGroup
 * This class contains set of traces that have similar characteristic.
 * Trace objects should be constructed and a vector that contains trace objects
 * should be passed to the constructor of the class.
 *
 */

class LTKTraceGroup
{

private:

	float m_xScaleFactor; //scale factor of the x channel

	float m_yScaleFactor; //scale factor of the y channel

	LTKTraceVector m_traceVector; //traces forming the trace group

public:

	/**
	 * @name Constructors and Destructor
	 */
	// @{

	/**
	 * Default constructor.
	 */


    LTKTraceGroup ();

	/**
	 * This constructor takes a vector of LTKTrace objects.
	 */

	LTKTraceGroup(const LTKTraceVector& inTraceVector,
		          float xScaleFactor=1.0, float yScaleFactor=1.0);

	/**
	 * Copy Constructor.
	 */

	LTKTraceGroup (const LTKTraceGroup& traceGroup);

	/**
	 * Constructor makes a Trace group with a single trace
	 */

	LTKTraceGroup (const LTKTrace& trace, float xScaleFactor=1.0,
		           float yScaleFactor=1.0);

	/**
	 * Destructor
	 */

	~LTKTraceGroup ();
	//@}

	/**
	 * @name Assignment operartor.
	 */
	//@{

	/**
	 * Assignment operator
	 * @param traceGroupObj The object to be copied by assignment
	 *
	 * @return LTKTraceGroup object
	 */

	LTKTraceGroup& operator = (const LTKTraceGroup& traceGroup);

		/**
	 * Assignment operator
	 * @param trace The trace object to be copied by assignment
	 *
	 * @return LTKTraceGroup object
	 */

	LTKTraceGroup& operator = (const LTKTrace& trace);

	//@}

	/**
	 * @name Getter Functions
	 */
	//@{

	/**
	 * This function returns all traces that are present in the trace group.
	 *
	 * @param void
	 *
	 * @return Returns reference to vector of LTKTrace objects, that form a trace group.
	 *
	 */

	const LTKTraceVector& getAllTraces () const;


	/**
	 * This function gives a trace at a specified index in a trace group.
	 *
	 * @param traceIndex index of a trace in trace group
	 *
	 * @return Reference to LTKTrace object
	 */

	int getTraceAt (int traceIndex, LTKTrace& outTrace) const;

	/**
	 * This fucntion returns the number of traces present in trace group.
	 *
	 * @param void
	 *
	 * @return Number of traces present in trace group.
	 *
	 */

	int getNumTraces () const;

	float getXScaleFactor() const;
	float getYScaleFactor() const;

	//@}

	/**
	 * Setter Fucntions
	 */
	//@{

	/**
	 * This function is used to add traces to a trace group object.
	 *
	 * @param traceObj trace object that has to be added to trace group.
	 *
	 * @return void
	 *
	 */

	int addTrace (const LTKTrace& trace);

	/**
	 * This function replaces vector of all traces in a trace group with the
	 * new traces vector.
	 *
	 * @param tracesVector The new traces which to have accessed.
	 *
	 * @return void
	 */

    int setAllTraces (const LTKTraceVector& tracesVec, float xScaleFactor=1.0,
		              float yScaleFactor=1.0);

	/**
	 * This function empties the tracevector
	 *
	 * @param void
	 *
	 * @return int
	 */

    void emptyAllTraces();
    /**
	 * This function calculates the maximum and minimum of coordinates in a trace group.
	 *
	 * @param xMin reference to minimum value of x channel
	 *
	 * @param yMin reference to minimum value of y channel
	 *
	 * @param xMax reference to maximum value of x channel
	 *
	 * @param yMax reference to maximum value of y channel
	 *
	 * @return int error code
	 */

	int getBoundingBox(float& outXMin,float& outYMin,float& outXMax,float& outYMax) const;



	/**
	 * Scales the tracegroup according to the x and y scale factors taking into account the current scale factors.
	 * After scaling, the tracegroup is translated in order to preserve the "cornerToPreserve".
	 *
	 * @param xScaleFactor factor by which x dimension has to be scaled
	 * @param yScaleFactor factor by which y dimension has to be scaled
     * @param cornerToPreserve  corner to be retained after scaling
	 * @return int error code
	 */

	int scale(float xScaleFactor,float yScaleFactor,TGCORNER cornerToPreserve);



	/**
	 * Translates the tracegroup so that the "referenceCorner" is moved to (x,y)
     *
	 *
	 * @param x x value of the point to which referenceCorner has to be moved
	 * @param y y value of the point to which referenceCorner has to be moved
	 * @param referenceCorner - the reference corner in the tracegroup that has to be moved to (x,y)
	 * @return int error code
	 */

	int translateTo(float x,float y,TGCORNER referenceCorner);


	/**
	 * Scales the tracegroup according to the x and y scale factors.
	 * After scaling, the "referenceCorner" of the tracegroup is translated to
	 *(translateToX,translateToY)
	 *
	 * @param xScaleFactor factor by which x dimension has to be scaled
	 * @param yScaleFactor factor by which y dimension has to be scaled
	 * @param referenceCorner corner to be retained after scaling and moved to (translateToX,translateToY)
     * @return int error code
	 */

	int affineTransform(float xScaleFactor,float yScaleFactor,float translateToX,
		                float translateToY,TGCORNER referenceCorner);

        bool containsAnyEmptyTrace() const;


	//@}

};

#endif

//#ifndef __LTKTRACEGROUP_H
//#define __LTKTRACEGROUP_H


