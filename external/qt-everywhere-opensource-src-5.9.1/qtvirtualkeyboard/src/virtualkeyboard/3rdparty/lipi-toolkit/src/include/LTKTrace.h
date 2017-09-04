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

/************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2008-07-18 15:00:39 +0530 (Fri, 18 Jul 2008) $
 * $Revision: 561 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Definition of LTKTrace which holds series of points from a pen down event to the next
 *			   immediate pen up event
 *
 * CONTENTS:
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 * Thanigai		09-AUG-2005		Added emptyTrace function to empty the trace
 ************************************************************************/

#ifndef __LTKTRACE_H
#define __LTKTRACE_H

#include "LTKTypes.h"
#include "LTKTraceFormat.h"

class LTKTraceFormat;

/** @defgroup Common_Classes  Common classes
*/

/**
* @ingroup Common_Classes
*/

/** @brief Stores contiguous series of coordinates for one stroke.
 * @class LTKTrace
 * This class contains contiguous series of coordinates for one stroke.
 * All channel values are stored internally as float. Hence the coordinates to be
 * passed to LTKTrace should be first converted to "float" type.
 */

class LTKTrace
{

private:

    vector<floatVector> m_traceChannels;//values of channels which make up the trace

	LTKTraceFormat m_traceFormat;


public:

	/**
	 * @name Constructors and Destructor
	 */
	//@{

	/**
	 * Default Constructor
	 */

	LTKTrace();

	/**
	 * This constructor initialises a vector of channel values with a vector of float.
	 *
	 * @param allChannelValues    A vector of float that contains contiguous channel values
	 * @param channelFormat       An object of LTKTraceFormat that provides information about
	 *                            the channel positions.
	 */

    LTKTrace(const floatVector& allChannelsVec, const LTKTraceFormat& channelformat);

	/**
	 * This constructor initialises a trace according to the specified trace format
	 *
	 * @param channelFormat       An object of LTKTraceFormat that provides information about
	 *                            the channel positions
	 */

    LTKTrace(const LTKTraceFormat& channelformat);

	/**
	 * Copy Constructor
	 */

	LTKTrace(const LTKTrace& trace);

	/**
	 * Destructor
	 */

	virtual ~LTKTrace();
	//@}

	/**
	 * @name Assignment operator
	 */
	//@{

	/**
	 * Assignment operator
	 * @param traceObj The object to be copied by assignment
	 *
	 * @return LTKTrace object
	 */

	LTKTrace& operator=(const LTKTrace& trace);
	//@}

	/**
	 * @name Getter Functions
	 */
	//@{

	/**
	 * This function returns the number of points that a stroke contains.
	 * Number of points is obtained by dividing the total number of contiguous channel values
	 * with the number of channels in trace format.
	 *
	 * @param void
	 *
	 * @return number of points in trace are returned.
	 *
	 */

	int getNumberOfPoints() const;

	/**
	 * This method returns a specific point in a stroke.
	 * @param pointIndex    The point index at which channel values are desired.
	 *
	 * @return A vector of float that contains channel values of a desired point.
	 *
	 */

	int getPointAt(int pointIndex, floatVector& outPointCoordinates) const;

	/**
	 * This method returns vector of float that contains all values
	 * of a specific channel in the stroke.
	 *
	 * @param channelName   The name of the channel, whose coordinates are required.
	 *
	 * @return A vector of float that contains all values of a desired channel.
	 */

	int getChannelValues(const string& channelName, floatVector& outChannelValues) const;

	/**
	 * This method returns vector of float that contains all values
	 * a channel at a specific position.
	 *
	 * @param channelIndex    Index of the channel,at which all coordinates are required.
	 *
	 * @return A vector of float that contains all values of a desired channel
	 */

	int getChannelValues(int channelIndex, floatVector& outChannelValues) const;

	/**
	 * This method returns a channel value at a specific point.
	 *
	 * @param Name of the channel whose value is required.
	 * @param the point number at which a channel's value is required.
	 *
	 * @return channel value at the specified point. This is a float.
	 *
	 */

	int getChannelValueAt(const string& channelName, int pointIndex,
						  float& outValue) const;



     //@}

	/**
	 * @name SetFunctions
	 */
	// @{

	/**
	 * This method reassigns the values of the channel specified. The size of the
	 * vector is expected to be same as that of the current channel size.
	 *
	 * @param channelName     Name of the channel
	 * @param channelValues   Vector that holds the new channel values
	 *
	 *
	 * @return errorCode
	 *
	 */

    int reassignChannelValues(const string& channelName,
						 const floatVector& channelValues);

	/**
	 * This method reassigns the values of all the channels. The number of rows
	 * in the input 2D vector must be equal to the current number of channels with
	 * each row having the same length. And this assumes one-to-one correspondence
	 * with the channel vector.
	 *
	 * @param allChannelValues new values of all the channels
	 *
	 * @return errorCode
	 *
	 */
	int setAllChannelValues(const float2DVector& allChannelValues);


	/**
	 * This method adds a point of coordinates to the existing set of points,
	 * of a trace.
	 *
	 * @param pointVec    vector of coordinates of a point.
	 *
	 * @return void
	 *
	 */
	int addPoint(const floatVector& pointVec);


	/**
	 * This function adds channel values of a new channel.
	 *
	 * @param channelValuesVec The channel values of the new channel.
	 * @param channelName The new channel to be added.
	 *
	 * @return void
	 */

    int addChannel(const floatVector &channelValuesVec,const LTKChannel& channel);

	/**
	 * This function empties the trace
	 *
	 * @param
	 *
	 * @return int
	 */

    void emptyTrace();

	/**
	 * Returns true if the data vector is empty
	 *
	 * @param
	 *
	 * @return bool
	 */

	bool isEmpty() const;


	/**
     * Getter on the current trace format
	 *
	 * @param
	 *
	 * @return const LTKTraceFormat&
	 */

   const LTKTraceFormat& getTraceFormat() const;


    int setChannelValues(const string& channelName,
                         const floatVector &inputChannelValuesVec);
	//@}



};

#endif

//#ifndef __LTKTRACE_H
//#define __LTKTRACE_H


