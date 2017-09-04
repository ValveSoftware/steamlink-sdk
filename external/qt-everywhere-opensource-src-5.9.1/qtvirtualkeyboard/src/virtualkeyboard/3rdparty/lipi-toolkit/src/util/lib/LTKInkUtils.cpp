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
 * $LastChangedDate: 2011-02-08 11:00:11 +0530 (Tue, 08 Feb 2011) $
 * $Revision: 832 $
 * $Author: dineshm $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation of LTKInkUtils that computes the statistics
 *             of a trace group
 *
 * CONTENTS: 
 *   computeTraceGroupStatistics 
 *   computeChannelMaximum
 *   computeChannelMinimum
 *   computeChannelMaxMin
 *
 * AUTHOR:     Deepu V.
 *
 * DATE:       March 9, 2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#include "LTKInkUtils.h"
#include "LTKMacros.h"
#include "LTKErrors.h"
#include "LTKLoggerUtil.h"
#include "LTKErrorsList.h"
#include "LTKTrace.h"
#include "LTKTraceGroup.h"


/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 09-MAR-2005
* NAME			: LTKInkUtils
* DESCRIPTION	: Initialization constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKInkUtils::LTKInkUtils()
{

}


/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 09-MAR-2005
* NAME			: computeChannelStatistics
* DESCRIPTION	: This is a generic function that computes the statistics of channels of
*                 an LTKTraceGroup object passed to it.   
* ARGUMENTS		: traceGroup        - The TraceGroup whose statistics need to be computed                  channelNames      - Names of channels in the traceGroup for which 
*			      channelNames      - channels for which statistics have to be comptued
*		          properties        - The names of the statistics to be computed
*			      channelStatistics - output vector containing results
*			      channelStatistics[i][j] the statistics properties[j] for channelname
*			      channelNames[i]
*
* RETURNS		:  SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKInkUtils::computeChannelStatistics(const LTKTraceGroup& traceGroup,
               const vector<string>& channelNames,  const vector<ELTKTraceGroupStatistics>& properties,
			   vector<vector<float> >& channelStatistics)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKInkUtils::computeChannelStatistics()" << endl;
	
	vector<float> tempVec; //temporary vector

	int numChannels = channelNames.size(); //num of channels for which statistics need to be computed

	int numFeatures = properties.size();   //number of properties to be calculated

	int numTraces = traceGroup.getNumTraces(); //number of traces in each tracegroup

	int numPoints;              //number of points in a stroke


	int totalNumPoints=0;  //each channel is of equal length

	float currVal;              //value of current point in the channel

	int traceIndex, channelIndex, pointIndex, featureIndex;

	// Clear the output vector
	channelStatistics.clear();

	//Make an initial vector
	tempVec.clear();
	for (featureIndex= 0 ; featureIndex <numFeatures; ++featureIndex)
	{
		switch(properties[featureIndex])
		{
		//initializing max
		case TG_MAX:tempVec.push_back(-FLT_MAX);
			break;
		//initializing min
		case TG_MIN:tempVec.push_back(FLT_MAX);
			break;
		//initializing avg
		case TG_AVG:tempVec.push_back(0);
			break;

		default: LOG(LTKLogger::LTK_LOGLEVEL_ERR)
				       <<"Error: LTKInkUtils::computeChannelStatistics()"<<endl;

			LTKReturnError(EUNSUPPORTED_STATISTICS);
		}
	}
	
	//Initialization Every channel has the same value
	for(channelIndex =0; channelIndex<numChannels; ++channelIndex)
	{
		channelStatistics.push_back(tempVec);

		//initialize total number of points for each channel to zero
	}


	//Iterating through all the strokes
	for (traceIndex = 0; traceIndex <numTraces; ++traceIndex)
	{
		LTKTrace trace;
		traceGroup.getTraceAt(traceIndex, trace);

		//Iterating through all the channels in a stroke
		for (channelIndex =0; channelIndex<numChannels; ++channelIndex)
		{
			//get the current channel values
			floatVector currChannel;
			trace.getChannelValues(channelNames[channelIndex], currChannel);

			//get the current output vector to be updated
			floatVector& currStats = channelStatistics.at(channelIndex);

			//number of points in this channel
			numPoints = currChannel.size();

			if(channelIndex==0)
			{
				totalNumPoints += numPoints;
			}

			//iterate through all points in the channel
			for(pointIndex = 0; pointIndex <numPoints; ++pointIndex)
			{
				currVal = currChannel[pointIndex];

				//updating all features as we iterate through each point;
				for (featureIndex =0; featureIndex<numFeatures; featureIndex++)
				{
					switch(properties[featureIndex])
					{

					//updating the maximum
					case TG_MAX:
						if(currVal > currStats[featureIndex])
							currStats[featureIndex] = currVal;
						break;

					//updating the minimum
					case TG_MIN:
						if(currVal < currStats[featureIndex])
							currStats[featureIndex] = currVal;
						break;

					//accumulating the sum
					case TG_AVG:
						currStats[featureIndex] += currVal;
						break;

					default: LOG(LTKLogger::LTK_LOGLEVEL_ERR)
							        <<"Error: LTKInkUtils::computeChannelStatistics()"<<endl;

						LTKReturnError(EUNSUPPORTED_STATISTICS);

					}

				}

			}
			
		}

	}

	//Finalization Step
	for (channelIndex= 0 ; channelIndex<numChannels; ++channelIndex)
	{

		floatVector& currStats = channelStatistics.at(channelIndex);

		//total number of points in this channel
		numPoints = totalNumPoints; 

		for(featureIndex = 0; featureIndex<numFeatures; ++featureIndex)
		{
			switch(properties[featureIndex])
			{
			//finding the average
			case TG_AVG:
					currStats[featureIndex] /= numPoints;
				break;
			}
		}
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKInkUtils::computeChannelStatistics()" << endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 09-MAR-2005
* NAME			: computeChannelMaximum
* DESCRIPTION	: Function that computes the minimums of channels of an
*                 LTKTraceGroup object passed to it.   
* ARGUMENTS		: traceGroup        - The TraceGroup whose minimums need to be computed               
*			      channelNames      - channels for which minimum vals have to be comptued
*			      minValues         - output vector containing results
*			      minValues[i] the minimum for channelname[i]
*
* RETURNS		:  SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKInkUtils::computeChannelMaximum(const LTKTraceGroup& traceGroup, const vector<string>& channelNames, 
		vector<float>& maxValues)
{
	int errorCode;
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKInkUtils::computeChannelMaximum()" << endl;

	vector<vector<float> >results;	//results from computeChannelStatistics;

	vector<ELTKTraceGroupStatistics> properties (1,TG_MAX); //Initializing the properties to MIN


	maxValues.clear(); //clear the minValues

	//call generic function
	if(errorCode = computeChannelStatistics(traceGroup,channelNames,properties,results))
	{
		return errorCode;
	}

	//populate the results
	for (int resultIndex=0; resultIndex<results.size(); ++resultIndex)
	{
		maxValues.push_back( (results.at(resultIndex)).at(0) );
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKInkUtils::computeChannelMaximum()" << endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 09-MAR-2005
* NAME			: computeChannelMinimum
* DESCRIPTION	: Function that computes the minimums of channels of an
*                 LTKTraceGroup object passed to it.   
* ARGUMENTS		: traceGroup        - The TraceGroup whose minimums need to be computed              
*			      channelNames      - channels for which minimum vals have to be comptued
*			      minValues         - output vector containing results
*			      minValues[i] the minimum for channelname[i]
*
* RETURNS		:  SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKInkUtils::computeChannelMinimum(const LTKTraceGroup& traceGroup,const vector<string>& channelNames, 
		vector<float>& minValues)
{

	int errorCode;
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKInkUtils::computeChannelMinimum()" << endl;


	vector<vector<float> >results;	//results from computeChannelStatistics;

	vector<ELTKTraceGroupStatistics> properties (1,TG_MIN); //Initializing the properties to MIN


	minValues.clear(); //clear the minValues

	//call generic function
	if(errorCode = computeChannelStatistics(traceGroup,channelNames,properties,results))
	{
		return errorCode;
	}

	//populate the results
	for (int resultIndex =0; resultIndex<results.size(); ++resultIndex)
	{
		minValues.push_back( (results.at(resultIndex)).at(0) );
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKInkUtils::computeChannelMinimum()" << endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 09-MAR-2005
* NAME			: computeChannelMaxMin
* DESCRIPTION	: Function that computes the minimum and maximum of channels of an
*                 LTKTraceGroup object passed to it.   
* ARGUMENTS		: traceGroup        - The TraceGroup whose parameters need to be computed           
*			      channelNames      - channels for which parameters have to be comptued
*			      minValues         - output vector containing min  values
*			      maxValues         - output vector containing max  values
*
* RETURNS		:  SUCCESS/FAILURE
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKInkUtils::computeChannelMaxMin(const LTKTraceGroup& traceGroup, const vector<string>& channelNames, 
		vector<float>& maxValues, vector<float>& minValues)
{
	int errorCode;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKInkUtils::computeChannelMaxMin()" << endl;

	vector<vector<float> >results;	//results from computeChannelStatistics;

	vector<ELTKTraceGroupStatistics> properties (2); //Initializing the properties to MIN

	//pushing the operations to be performed
	properties[0] = TG_MIN;
	properties[1] = TG_MAX;

	minValues.clear(); //clear the minValues
	maxValues.clear(); //clear the maxvalues

	//call generic function
	if(errorCode = computeChannelStatistics(traceGroup,channelNames,properties,results))
	{
		return errorCode;
	}

	//populate the results
	for (int resultIndex =0; resultIndex<results.size(); ++resultIndex)
	{
		minValues.push_back( (results.at(resultIndex)).at(0) );
		maxValues.push_back( (results.at(resultIndex)).at(1) );
	}
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKInkUtils::computeChannelMaxMin()" << endl;

	return SUCCESS;
}


/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 09-MAR-2005
* NAME			: ~LTKInkUtils
* DESCRIPTION	: destructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
LTKInkUtils::~LTKInkUtils()
{

}

