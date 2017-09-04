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
/************************************************************************
 * FILE DESCR: Definition of LTKTraceFormat which holds the information about the type and number of  
 *			   channel data available at each pen point
 *
 * CONTENTS:   
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef __LTKTRACEFORMAT_H
#define __LTKTRACEFORMAT_H

#include "LTKTypes.h"

class LTKChannel;

/**
* @ingroup Common_Classes
*/

/** @brief Contains the format of channels in which ink coordinates are stored.
 * @class LTKTraceFormat
 * This class contains the format of channels in which ink coordinates are stored.
 * Contains a vector of LTKChannel objects.
 */

class LTKTraceFormat
{
private:

	LTKChannelVector m_channelVector;	//group of channels defining the trace format
	
public:

	/**
	 * @name Constructor and Destructors
	 */
	//@{

	/**
	 * Default Constructor.
	 * Default channels are X and Y. 
	 * Therfore contains LTKChannle objects for X and Y channels.
	 */

    LTKTraceFormat();

	/**
	 * Takes a vector of LTKChannel objects. vector of these objects 
	 * have to be first constructed outside and then passed to constructor.
	 */

    LTKTraceFormat(const LTKChannelVector& channelsVec);

	/**
	 * Copy Constructor
	 */

	LTKTraceFormat(const LTKTraceFormat& traceFormat);

	/**
	 * Destructor
	 */

	~LTKTraceFormat();

	//@}

	/**
	 * @name Assignment operator
	 */
	//@{

	/**
	 * Assignment operator
	 * @param traceFormatObj The object to be copied by assignment
	 *
	 * @return LTKTraceFormat object
	 */

	LTKTraceFormat& operator=(const LTKTraceFormat& traceFormat);
	//@}
	
	/**
	 * @name Getter Functions
	 */
	//@{

	/**
	 * This method returns the position at which a channel coordinate is stored
	 * in a point.
	 *
	 * @param channelName
	 *
	 * @param returnIndex
	 *
	 * @return SUCCESS or FAILURE
	 * 
	 */

    int getChannelIndex(const string& channelName, int& outReturnIndex) const ;

	/**
	 * This method returns the number of channels for which positions of 
	 * electronic pen are captured.
	 *
	 * @param void
	 * 
	 * @return number of channels
	 */

    int getNumChannels() const ;

	/**
	 * This method returns the name of a channel at a particular Index.
	 * 
	 * @param index The index at which a channel name is required
	 *
	 * @param channelName A string passed by reference that holds the name of the 
	 * channel at the specified index
	 *
	 * @return SUCCESS or ECHANNEL_INDEX_OUT_OF_BOUND.
	 */

	int getChannelName(int index, string& outChannelName) const ;

	/**
	 * This method is used to know the regular channels that are present in 
	 * trace format.
	 *
	 * @param void
	 *
	 * @return A vector of channel names that are regular
	 */

     stringVector getRegularChannelNames() const ;

	/**
	 * This method is used to know all channels that are present in trace format
	 *
	 * @param void
	 *
	 * @return A vector of all channel names
	 */

     stringVector getAllChannelNames() const ;
	//@}

	/**
	 * @name Setter Functions
	 */
	//@{

	/**
	 * This method sets channel format if a different ordering of channels is required.
	 * @param A vector of LTKChannel objects 
	 *
	 * @return The channels of the previous format used.
	 */

    void setChannelFormat(const LTKChannelVector& channelFormatVec);

	/**
	 * This method is used to add a channel to the trace format
	 * @param LTKChannel object that contains details of the channel to be added
	 *
	 * @return ERROR
	 */
	
    int addChannel(const LTKChannel& channel);

	//@}

};

#endif

//#ifndef __LTKTRACEFORMAT_H
//#define __LTKTRACEFORMAT_H



