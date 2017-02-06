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
 * $LastChangedDate: 2011-01-11 13:48:17 +0530 (Tue, 11 Jan 2011) $
 * $Revision: 827 $
 * $Author: mnab $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: implementation of LTKTrace which holds series of points 
 *			   from a pen down event to the next immediate pen up event
 *
 * CONTENTS: 
 *	getNumberOfPoints
 *	getPointAt
 *	getChannelValues
 *	getChannelValues (overloaded)
 *	getChannelValueAt
 *	setChannelValues
 *	addPoint
  *	getChannelNames
 *	addChannel
 *	getChannelIndex
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#include "LTKTrace.h"
#include "LTKMacros.h"
#include "LTKTraceFormat.h"
#include "LTKErrors.h"
#include "LTKErrorsList.h"
#include "LTKChannel.h"
#include "LTKException.h"
#include "LTKLoggerUtil.h"


/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKTrace
* DESCRIPTION	: Default Constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKTrace::LTKTrace()
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTrace::LTKTrace()"<<endl;
	
	floatVector emptyChannel;
	
	m_traceChannels.assign(2,emptyChannel);
    
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTrace::LTKTrace()"<<endl;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKTrace
* DESCRIPTION	: Initialization constructor
* ARGUMENTS		: inputStream - incoming data 
*				  traceFormat - format of the incoming data
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKTrace::LTKTrace(const floatVector& inputStream, 
                   const LTKTraceFormat& traceFormat)
{

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTrace::LTKTrace(const floatVector&,const LTKTraceFormat& )"<<endl;

	int inputStreamSize     = inputStream.size();
	int numChannels         = traceFormat.getNumChannels();

	floatVector tempChannel;

	if(numChannels == 0)
	{
			LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
			"Error: "<<EZERO_CHANNELS <<": "<<
			getErrorMessage(EZERO_CHANNELS) <<
			"LTKTrace::LTKTrace(const floatVector&,const LTKTraceFormat&() "<<endl;

		throw LTKException(EZERO_CHANNELS);
	}

	if(inputStreamSize==0 || ((inputStreamSize % numChannels) != 0))
	{
		
			LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
			"Error: "<<EINVALID_INPUT_STREAM <<": "<<
			getErrorMessage(EINVALID_INPUT_STREAM) <<
			"LTKTrace::LTKTrace(const floatVector&,const LTKTraceFormat&() "<<endl;

		throw LTKException(EINVALID_INPUT_STREAM);
	}

	m_traceFormat = traceFormat;

	/*
	 * values of individual channels are separated from contiguous channel values. 
	 * vector<float> of these individual channels is computed, and such vectors  
	 * for all channels are collected into another vector.
	 */

	for(int channelIndex = 0; channelIndex < numChannels; ++channelIndex)
	{
        for(int inputIndex = channelIndex; inputIndex < inputStreamSize; 
			inputIndex += numChannels)
		{
			tempChannel.push_back(inputStream[inputIndex]);
		}

		m_traceChannels.push_back(tempChannel); 

		tempChannel.clear();
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTrace::LTKTrace(const floatVector&,const LTKTraceFormat& )"<<endl;

}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKTrace
* DESCRIPTION	: Trace format initialization constructor
* ARGUMENTS		: traceFormat - format of the incoming data
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKTrace::LTKTrace(const LTKTraceFormat& traceFormat)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTrace::LTKTrace(const LTKTraceFormat&)"<<endl;

	floatVector tempChannel;

	int numChannels = traceFormat.getNumChannels();

	m_traceFormat = traceFormat;

	m_traceChannels.assign(numChannels, tempChannel);

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTrace::LTKTrace(const LTKTraceFormat&)"<<endl;

}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKTrace
* DESCRIPTION	: copy constructor
* ARGUMENTS		: trace - trace to be copied
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKTrace::LTKTrace(const LTKTrace& trace)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTrace::LTKTrace(const LTKTrace&)"<<endl;

	m_traceChannels = trace.m_traceChannels;
	m_traceFormat   = trace.m_traceFormat;
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTrace::LTKTrace(const LTKTrace&)"<<endl;

}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: operator=
* DESCRIPTION	: assignment operator
* ARGUMENTS		: trace - trace to be assigned 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKTrace& LTKTrace::operator=(const LTKTrace& trace)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTrace::operator=()"<<endl;

	if ( this != &trace )
	{
		m_traceChannels = trace.m_traceChannels;
		m_traceFormat   = trace.m_traceFormat;
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTrace::operator=()"<<endl;

	return *this;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getNumberOfPoints
* DESCRIPTION	: get number of points in the trace
* ARGUMENTS		: 
* RETURNS		: number of points in the trace
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

int LTKTrace::getNumberOfPoints() const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTrace::getNumberOfPoints()"<<endl;
    
    
  	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTrace::getNumberOfPoints()"<<endl;
   
    return m_traceChannels[0].size();	
	
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getPointAt
* DESCRIPTION	: returns the point at a specified index
* ARGUMENTS		: pointIndex - index of the point whose values are required
* RETURNS		: point at the specified index
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

int LTKTrace::getPointAt(int pointIndex, floatVector& outPointCoordinates) const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTrace::getPointAt()"<<endl;
	
	if ( pointIndex < 0 || pointIndex >= m_traceChannels[0].size() )
	{
			LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
			"Error: "<<EPOINT_INDEX_OUT_OF_BOUND <<": "<<
			getErrorMessage(EPOINT_INDEX_OUT_OF_BOUND) <<
			"LTKTrace::getPointAt() "<<endl;

		LTKReturnError(EPOINT_INDEX_OUT_OF_BOUND);
	}

	vector<floatVector>::const_iterator channelIterator = m_traceChannels.begin();

	vector<floatVector>::const_iterator traceChannelsEnd = m_traceChannels.end();

	for(; channelIterator != traceChannelsEnd; ++channelIterator)
	{
		outPointCoordinates.push_back((*channelIterator)[pointIndex]);
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTrace::getPointAt()"<<endl;
	
	return SUCCESS;
}
	
/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getChannelValues
* DESCRIPTION	: get the values of the channel given its name
* ARGUMENTS		: channelName - name of the channel whose values are required
* RETURNS		: values of the channel with the specified name
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/
int LTKTrace::getChannelValues(const string& channelName, 
                                   floatVector& outChannelValues) const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTrace::getChannelValues(const string&, floatVector&)"<<endl;
	
	int channelIndex = -1;
	
	int errorCode = 0;

    if ((errorCode=m_traceFormat.getChannelIndex(channelName, channelIndex)) != SUCCESS )
    {
        	LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
			"Error: LTKTrace::getChannelValues(string&, floatVector&) "<<endl;

		LTKReturnError(errorCode);
    }
    
    outChannelValues = m_traceChannels[channelIndex];

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTrace::getChannelValues(string&, floatVector&)"<<endl;
	
    return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getChannelValues
* DESCRIPTION	: get the values of the channel given its index
* ARGUMENTS		: channelIndex - index of the channel whose values are required
* RETURNS		: values of the channel at the specified index
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/
int LTKTrace::getChannelValues(int channelIndex,
                               floatVector& outChannelValues) const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTrace::getChannelValues(int, floatVector&)"<<endl;

	if(channelIndex < 0 || channelIndex >= m_traceFormat.getNumChannels())
	{
			LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
			"Error: "<<ECHANNEL_INDEX_OUT_OF_BOUND <<": "<<
			getErrorMessage(ECHANNEL_INDEX_OUT_OF_BOUND) <<
			"LTKTrace::getChannelValues() "<<endl;

		LTKReturnError(ECHANNEL_INDEX_OUT_OF_BOUND);
	}

    outChannelValues = m_traceChannels[channelIndex];

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTrace::getChannelValues(int, floatVector&)"<<endl;
	
    return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getChannelValueAt
* DESCRIPTION	: gets the value of a particular channel at a particular point
* ARGUMENTS		: channelName - name of the channel to get the value from
*				  pointIndex - index of the point at which the channel value 
*                 is required
* RETURNS		: value of the specified channel at the specified point
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/
int LTKTrace::getChannelValueAt(const string& channelName, int pointIndex, 
                                    float& outValue) const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTrace::getChannelValueAt(const string&, floatVector&)"<<endl;

	if (pointIndex < 0 || pointIndex >= m_traceChannels[0].size() )
	{
			LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
			"Error: "<<ECHANNEL_INDEX_OUT_OF_BOUND <<": "<<
			getErrorMessage(ECHANNEL_INDEX_OUT_OF_BOUND) <<
			"LTKTrace::getChannelValueAt() "<<endl;

		LTKReturnError(EPOINT_INDEX_OUT_OF_BOUND);
	}

    int channelIndex = -1;

    if (m_traceFormat.getChannelIndex(channelName, channelIndex) != SUCCESS )
    {
			LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
			"Error: "<<ECHANNEL_NOT_FOUND <<": "<<
			getErrorMessage(ECHANNEL_NOT_FOUND) <<
			"LTKTrace::getChannelValueAt() "<<endl;

        LTKReturnError(ECHANNEL_NOT_FOUND);
    }

    outValue = m_traceChannels[channelIndex][pointIndex];
    
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTrace::getChannelValueAt()"<<endl;
	
	return SUCCESS;
	
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setChannelValues
* DESCRIPTION	: resets the data of the given channel 
* ARGUMENTS		: channelName - name of the channel whose data is to be reset
*				  channelData - new data for the channel reset
* RETURNS		: SUCCESS on successful reset
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/
int LTKTrace::reassignChannelValues(const string &channelName, 
							   const floatVector &channelData)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTrace::reassignChannelValues(const string&, floatVector&)"<<endl;

	if(channelData.size() != m_traceChannels[0].size())
	{
			LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
			"Error: "<<ECHANNEL_SIZE_MISMATCH <<": "<<
			getErrorMessage(ECHANNEL_SIZE_MISMATCH) <<
			"LTKTrace::reassignChannelValues() "<<endl;

		LTKReturnError(ECHANNEL_SIZE_MISMATCH);
	}

    int channelIndex = -1;

    if (m_traceFormat.getChannelIndex(channelName, channelIndex) != SUCCESS )
    {
			LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
			"Error: "<<ECHANNEL_NOT_FOUND <<": "<<
			getErrorMessage(ECHANNEL_NOT_FOUND) <<
			"LTKTrace::reassignChannelValues() "<<endl;

        LTKReturnError(ECHANNEL_NOT_FOUND);
    }
    
	// updating the changed values of a channel in  m_traceChannels vector.
	m_traceChannels[channelIndex] = channelData; 

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTrace::reassignChannelValues()"<<endl;
	
	return SUCCESS;
}


/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setAllChannelValues
* DESCRIPTION	: This method reassigns the values of all the channels. The number 
*                 of rows in the input 2D vector must be equal to the current number 
*                 of channels with each row having the same length. And this assumes 
*                 one-to-one correspondence with the channel vector.
* ARGUMENTS		: allChannelValues - new values of all the channels  
*				  channelData - new data for the channel reset
* RETURNS		: SUCCESS on successful reset
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

int LTKTrace::setAllChannelValues(const float2DVector& allChannelValues)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTrace::setAllChannelValues()"<<endl;

    if(allChannelValues.size() != m_traceFormat.getNumChannels())
    {

			LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
			"Error: "<<ENUM_CHANNELS_MISMATCH <<": "<<
			getErrorMessage(ENUM_CHANNELS_MISMATCH) <<
			"LTKTrace::setAllChannelValues() "<<endl;

			 LTKReturnError(ENUM_CHANNELS_MISMATCH);
    }
    
    if(allChannelValues[0].size()==0)
    {
        	LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
			"Error: "<<EEMPTY_VECTOR <<": "<<
			getErrorMessage(EEMPTY_VECTOR) <<
			"LTKTrace::setAllChannelValues() "<<endl;

       LTKReturnError(EEMPTY_VECTOR);                               
    }
    
    int prevRowSize=allChannelValues[0].size();
    int currRowSize=0;
    
    for(int i=1; i<allChannelValues.size(); ++i)
    {
         currRowSize = allChannelValues[i].size();
         
         if(currRowSize != prevRowSize)
         {
           	LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
				"Error: "<<EUNEQUAL_LENGTH_VECTORS <<": "<<
				getErrorMessage(EUNEQUAL_LENGTH_VECTORS) <<
				"LTKTrace::setAllChannelValues() "<<endl;

           LTKReturnError(EUNEQUAL_LENGTH_VECTORS);              
         }
         
         prevRowSize = currRowSize;
    }
    
    m_traceChannels = allChannelValues;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTrace::setAllChannelValues()"<<endl;
	
    
    return SUCCESS;
}



/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: addPoint
* DESCRIPTION	: adds a point to the trace
* ARGUMENTS		: pointVec - point to be added to the trace
* RETURNS		: SUCCESS on successful addition
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/
int LTKTrace::addPoint(const floatVector& pointVec)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTrace::setAllChannelValues()"<<endl;

	int numChannels = m_traceFormat.getNumChannels();

	if ( pointVec.size() != numChannels )
	{
			LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
				"Error: "<<EUNEQUAL_LENGTH_VECTORS <<": "<<
				getErrorMessage(EUNEQUAL_LENGTH_VECTORS) <<
				"LTKTrace::setAllChannelValues() "<<endl;

		LTKReturnError(ENUM_CHANNELS_MISMATCH);
	}

	// Adding the new point in m_traceChannels vector.
	for(int index=0; index < numChannels; ++index)
	{
		(m_traceChannels[index]).push_back(pointVec[index]);
	}
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTrace::setAllChannelValues()"<<endl;
	
	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: addChannel
* DESCRIPTION	: adds a new channel to the trace
* ARGUMENTS		: channelValuesVec - values of the new channel
*				  channelName - new channel name
* RETURNS		: SUCCESS on successful addition
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*******************************************************************************/

int LTKTrace::addChannel(const floatVector &channelValuesVec, 
						 const LTKChannel& channel)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTrace::addChannel()"<<endl;

	if ( m_traceChannels[0].size() !=0 && 
        channelValuesVec.size()  != m_traceChannels[0].size() )
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
			"Error: "<<ECHANNEL_SIZE_MISMATCH <<": "<<
			m_traceChannels[0].size() <<
			"LTKTrace::addChannel() "<<endl;

		LTKReturnError(ECHANNEL_SIZE_MISMATCH);
	}

    int errorCode = m_traceFormat.addChannel(channel);

    if (errorCode != SUCCESS)
    {
        LTKReturnError(errorCode);
    }

	m_traceChannels.push_back(channelValuesVec);

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTrace::addChannel()"<<endl;
	
	return SUCCESS;
}


/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: ~LTKTrace
* DESCRIPTION	: destructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKTrace::~LTKTrace()
{
}

/******************************************************************************
* AUTHOR		: Bharath A
* DATE			: 17-DEC-2007
* NAME			: emptyTrace
* DESCRIPTION	: To empty the channel values but retain the trace format
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*******************************************************************************/
void LTKTrace::emptyTrace()
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTrace::emptyTrace()"<<endl;

    for(int i=0;i<m_traceChannels.size();++i)
    {
       m_traceChannels[i].clear();
    }
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTrace::emptyTrace()"<<endl;
	
}


/******************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 17-Dec-2007
* NAME			: isEmpty
* DESCRIPTION	: Checks if the data vector is empty
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/
bool LTKTrace::isEmpty() const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTrace::isEmpty()"<<endl;

 
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTrace::isEmpty()"<<endl;
	
    return (m_traceChannels[0].size()==0);
}


/******************************************************************************
* AUTHOR		: Dinesh M
* DATE			: 17-Dec-2007
* NAME			: getTraceFormat
* DESCRIPTION	: Getter on the current trace format
* ARGUMENTS		: 
* RETURNS		: const LTKTraceFormat&
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

const LTKTraceFormat& LTKTrace::getTraceFormat() const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTrace::getTraceFormat()"<<endl;

 
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTrace::getTraceFormat()"<<endl;
	
  return m_traceFormat;    
}

