/*****************************************************************************************
* Copyright (c) 2007 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of 
* this software and associated documentation files (the "Software"), to deal in 
* the Software without restriction, including without limitation the rights to use, 
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the 
* Software, and to permit persons to whom the Software is furnished to do so, 
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, 
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
* PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT 
* HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
* CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE 
* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
*****************************************************************************************/

/************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2008-07-10 15:23:21 +0530 (Thu, 10 Jul 2008) $
 * $Revision: 556 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Definitions for the Ink utilities module
 *
 * CONTENTS: 
 *
 * AUTHOR:     Deepu V.
 *
 * DATE:       March 07 2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef __LTKINKUTILS_H
#define __LTKINKUTILS_H

#include <cfloat>

#include "LTKInc.h"

#include "LTKTypes.h"

class LTKTraceGroup;

/**
* @ingroup util
*/

/** @brief This class contains the methods for computing statistics of a channel
* @class LTKInkUtils
*/

class LTKInkUtils  
{
public:
	/**
	 * @name Constructors and Destructor
	 */
	// @{

	/**
	 * Default Constructor
	 */
	LTKInkUtils();

	/**
	 * Destructor 
	 */
	virtual ~LTKInkUtils();

	// @}

	/**
	 * @name Functions that computes the statistics
	 */
	// @{

	/**
	 * Generic function that computes the statistics of channels of an LTKTraceGroup object  
	 * 
	 * @param  traceGroup         The TraceGroup whose statistics need to be computed
     * @param  channelNames       Names of channels in the traceGroup for which statistics have to be comptued
     * @param  properties         The names of the statistics to be computed
     * @param  channelStatistics  output vector containing results
	 */
    static int computeChannelStatistics(const LTKTraceGroup& traceGroup,const vector<string>& channelNames,  
		const vector<ELTKTraceGroupStatistics>& properties, vector<vector<float> >& channelStatistics);


	/**
	 * This function computes the minimum of channels of an LTKTraceGroup object 
	 * 
	 * @param  traceGroup         The TraceGroup whose maximum need to be computed
     * @param  channelNames       Names of channels in the traceGroup for which maximum need to be computed
     * @param  maxValues          output vector containing maxValues for each channel
	 */
	static int computeChannelMaximum(const LTKTraceGroup& traceGroup, const vector<string>& channelNames, 
		vector<float>& maxValues);

	/**
	 * This function computes the minimum of channels of an LTKTraceGroup object 
	 * 
	 * @param  traceGroup         The TraceGroup whose minimum need to be computed
     * @param  channelNames       Names of channels in the traceGroup for which minimum need to be computed
     * @param  minValues          output vector containing minValues for each channel
	 */
	static int computeChannelMinimum(const LTKTraceGroup& traceGroup, const vector<string>& channelNames, 
		vector<float>& minValues);

	/**
	 * This function computes the minimum and maximum of channels of an LTKTraceGroup object 
	 * 
	 * @param  traceGroup         The TraceGroup whose parameters need to be computed
     * @param  channelNames       Names of channels for which parameters need to be computed
     * @param  minValues          output vector containing minValues for each channel
     * @param  maxValues          output vector containing maxValues for each channel
     */
	static int computeChannelMaxMin(const LTKTraceGroup& traceGroup, const vector<string>& channelNames, 
		vector<float>& maxValues, vector<float>& minValues);


	// @}

};

#endif //#ifndef __LTKINKUTILS_H
