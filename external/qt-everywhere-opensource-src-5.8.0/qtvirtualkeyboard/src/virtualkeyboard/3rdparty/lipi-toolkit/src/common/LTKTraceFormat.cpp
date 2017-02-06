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
 * $LastChangedDate: 2008-07-18 15:33:34 +0530 (Fri, 18 Jul 2008) $
 * $Revision: 564 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation of LTKTraceFormat which holds the information 
 *			   about type and number of channel data available at each pen point
 *
 * CONTENTS:
 *	getChannelIndex
 *	getChannelName
 *	getNumChannels
 *	getRegularChannelNames
 *	getAllChannelNames
 *	setChannelFormat
 *	addChannel
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#include "LTKTraceFormat.h"
#include "LTKChannel.h"
#include "LTKException.h"
#include "LTKErrorsList.h"
#include "LTKErrors.h"
#include "LTKMacros.h"
#include "LTKLoggerUtil.h"

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKTraceFormat
* DESCRIPTION	: Default Constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKTraceFormat::LTKTraceFormat()
{	
	LTKChannel xChannel(X_CHANNEL_NAME);
	LTKChannel yChannel(Y_CHANNEL_NAME);
	m_channelVector.push_back(xChannel);
	m_channelVector.push_back(yChannel);
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKTraceFormat
* DESCRIPTION	: Initialization Constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKTraceFormat::LTKTraceFormat(const LTKChannelVector& channelsVec)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Entering: LTKTraceFormat::LTKTraceFormat()"<<endl;
		if(channelsVec.size() == 0)
		{
            LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EZERO_CHANNELS <<":"<< getErrorMessage(EZERO_CHANNELS)
                  <<" LTKTraceFormat::LTKTraceFormat(LTKChannelVector&)" <<endl;

              throw LTKException(EZERO_CHANNELS);                
        }
        else
        {
		    m_channelVector = channelsVec;
        }
    
	  LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Entering: LTKTraceFormat::LTKTraceFormat()"<<endl;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKTraceGroup
* DESCRIPTION	: Copy Constructor
* ARGUMENTS		: traceFormat - object to be copied
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKTraceFormat::LTKTraceFormat(const LTKTraceFormat& traceFormat)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTraceFormat::LTKTraceFormat(const LTKTraceFormat&)"<<endl;

	m_channelVector = traceFormat.m_channelVector;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTraceFormat::LTKTraceFormat(const LTKTraceFormat&)"<<endl;
	
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: operator=
* DESCRIPTION	: Assignment operator
* ARGUMENTS		: traceFormat - object to be assigned
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKTraceFormat& LTKTraceFormat::operator=(const LTKTraceFormat& traceFormat)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTraceFormat::operator=()"<<endl;

	if ( this != &traceFormat )
	{
		m_channelVector = traceFormat.m_channelVector;
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTraceFormat::operator=()"<<endl;

	return *this;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getChannelIndex
* DESCRIPTION	: returns position of the channel given its name
* ARGUMENTS		: channelName - name of the channel whose position is required
* RETURNS		: position of the channel with the given name
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

int LTKTraceFormat::getChannelIndex(const string& channelName,
                                    int& outReturnIndex) const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTraceFormat::getChannelIndex()"<<endl;

	int numChannels = m_channelVector.size();
		
	for(int channelIndex = 0 ; channelIndex < numChannels; ++channelIndex)
	{
		if((m_channelVector[channelIndex]).getChannelName() == channelName)
		{
			outReturnIndex = channelIndex;

			LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
			"Exiting: LTKTraceFormat::getChannelIndex()"<<endl;
            return SUCCESS;
		}
	}
	
	LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
	"Error: "<<ECHANNEL_NOT_FOUND <<": "<<
	getErrorMessage(ECHANNEL_NOT_FOUND) <<
	"LTKCaptureDevice::getChannelIndex()"<<endl;

	return ECHANNEL_NOT_FOUND;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getChannelName
* DESCRIPTION	: 
* ARGUMENTS		: channelIndex - given channel index
* RETURNS		: corresponding channel name
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

int LTKTraceFormat::getChannelName(int channelIndex,
                                   string& outChannelName) const 
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTraceFormat::getChannelName()"<<endl;

	if ( channelIndex < 0 || channelIndex >= m_channelVector.size() )
	{
		
			LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
			"Error: "<<ECHANNEL_INDEX_OUT_OF_BOUND <<": "<<
			getErrorMessage(ECHANNEL_INDEX_OUT_OF_BOUND) <<
			"LTKCaptureDevice::getChannelName()"<<endl;

		LTKReturnError(ECHANNEL_INDEX_OUT_OF_BOUND);
	}

    outChannelName = m_channelVector[channelIndex].getChannelName();
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTraceFormat::getChannelName()"<<endl;
	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getNumChannels
* DESCRIPTION	: returns the number of channels in the trace format
* ARGUMENTS		: 
* RETURNS		: number of channels in the trace format
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

int LTKTraceFormat::getNumChannels() const 
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTraceFormat::getNumChannels()"<<endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTraceFormat::getNumChannels()"<<endl;
	
	return m_channelVector.size();
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getRegularChannelNames
* DESCRIPTION	: returns list of regular channel names
* ARGUMENTS		: 
* RETURNS		: list of regular channel names
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

stringVector LTKTraceFormat::getRegularChannelNames() const 
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTraceFormat::getRegularChannelNames()"<<endl;	

	stringVector regularChannelNamesVector;

	vector<LTKChannel>::const_iterator channelIterator = m_channelVector.begin();
	
	vector<LTKChannel>::const_iterator channelVectorEnd = m_channelVector.end();

	for(; channelIterator != channelVectorEnd; ++channelIterator)
	{

		if( (*channelIterator).isRegularChannel() )
		{
			regularChannelNamesVector.push_back((*channelIterator).getChannelName());
		}
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTraceFormat::getRegularChannelNames()"<<endl;	
	return regularChannelNamesVector;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getAllChannelNames
* DESCRIPTION	: returns list of all channel names
* ARGUMENTS		: 
* RETURNS		: list of all channel names
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

stringVector LTKTraceFormat::getAllChannelNames() const 
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTraceFormat::getAllChannelNames()"<<endl;

	stringVector allChannelNamesVector;

	vector<LTKChannel>::const_iterator channelIterator = m_channelVector.begin();
	
	vector<LTKChannel>::const_iterator channelVectorEnd = m_channelVector.end();

	for(; channelIterator != channelVectorEnd; ++channelIterator)
	{
		allChannelNamesVector.push_back((*channelIterator).getChannelName());
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTraceFormat::getAllChannelNames()"<<endl;	

	return allChannelNamesVector;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setChannelFormat
* DESCRIPTION	: resets the channel format
* ARGUMENTS		: channelFormatVector - reference to the vector to be reset to
* RETURNS		: SUCCESS on successful reset operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

void LTKTraceFormat::setChannelFormat(const LTKChannelVector& channelFormatVector)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTraceFormat::setChannelFormat()"<<endl;
	
	m_channelVector = channelFormatVector;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTraceFormat::setChannelFormat()"<<endl;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: addChannel
* DESCRIPTION	: adds a new channel to the trace format
* ARGUMENTS		: channel - new channel to be added
* RETURNS		: SUCCESS on successful addition
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

int LTKTraceFormat::addChannel(const LTKChannel& channel)
{
   	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Entering: LTKTraceFormat::addChannel()"<<endl;

	string inputChannelName = channel.getChannelName();
    
    // Check if the channel with the name already Exitings
	vector<LTKChannel>::const_iterator channelIterator = m_channelVector.begin();
	
	vector<LTKChannel>::const_iterator channelVectorEnd = m_channelVector.end();

    for(; channelIterator != channelVectorEnd; ++channelIterator)
	{
		if( (*channelIterator).getChannelName() == inputChannelName )
        {
            	LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
				"Error: "<<EDUPLICATE_CHANNEL <<": "<<
				getErrorMessage(EDUPLICATE_CHANNEL) <<
				"LTKCaptureDevice::addChannel()"<<endl;

			LTKReturnError(EDUPLICATE_CHANNEL);
        }
	}

	m_channelVector.push_back(channel);
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	"Exiting: LTKTraceFormat::addChannel()"<<endl;

	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: ~LTKTraceFormat
* DESCRIPTION	: destructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/

LTKTraceFormat::~LTKTraceFormat()
{
}




