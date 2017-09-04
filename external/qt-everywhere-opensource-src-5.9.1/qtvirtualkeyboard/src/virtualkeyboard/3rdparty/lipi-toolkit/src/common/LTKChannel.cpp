/******************************************************************************
* Copyright (c) 2007 Hewlett-Packard Development Company, L.P.
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
 * FILE DESCR: Implementation of LTKChannel which has the description of a 
 *             particular input stream like x-coordinate stream, 
 *             y-coordinate stream, time, pressure etc.
 *
 * CONTENTS:
 *	getChannelType
 *	getChannelName
 *	isRegularChannel
 *	setChannelType
 *	setChannelName
 *	setRegularChannel
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 *****************************************************************************/

#include "LTKErrors.h"
#include "LTKErrorsList.h"
#include "LTKChannel.h"
#include "LTKMacros.h"
#include "LTKLoggerUtil.h"

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKChannel
* DESCRIPTION	: Default Constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

LTKChannel::LTKChannel():
	m_channelName(DEFAULT_CHANNEL_NAME),
	m_channelType(DEFAULT_DATA_TYPE),
	m_isRegularChannel(true)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKChannel::LTKChannel()" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKChannel::LTKChannel()" << endl;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKChannel
* DESCRIPTION	: Parameterized Constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

LTKChannel::LTKChannel(const string& channelName):
	m_channelName(channelName),
	m_channelType(DEFAULT_DATA_TYPE),
	m_isRegularChannel(true)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKChannel::LTKChannel(const string&)" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKChannel::LTKChannel(const string&)" << endl;

}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKChannel
* DESCRIPTION	: Initializes the members of the class
* ARGUMENTS		: channelName - logical name of the input stream
*				  channelType - data type of the values from this input stream
*				  isRegular - type of channel : true for regular, false for 
                              intermittent
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

LTKChannel::LTKChannel(const string& channelName, ELTKDataType channelType, 
					   bool isRegular) :
	m_channelName(channelName), 
	m_channelType(channelType), 
	m_isRegularChannel(isRegular)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKChannel::LTKChannel(const string&,ELTKDataType,bool)" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKChannel::LTKChannel(const string&,ELTKDataType,bool)" << endl;

}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKChannel
* DESCRIPTION	: Copy Constructor
* ARGUMENTS		: channel - LTKChannel to be copied
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

LTKChannel::LTKChannel(const LTKChannel& channel)
{

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKChannel::LTKChannel(const LTKChannel&)" << endl;

	m_channelName = channel.m_channelName;

	m_channelType = channel.m_channelType;

	m_isRegularChannel = channel.m_isRegularChannel;
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKChannel::LTKChannel(const LTKChannel&)" << endl;

}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: operator=
* DESCRIPTION	: Overloaded assignment operator
* ARGUMENTS		: channel - LTKChannel to be assigned to
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

LTKChannel& LTKChannel::operator=(const LTKChannel& channel)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKChannel::operator=()" << endl;

	if ( this != &channel )
	{
		m_channelName = channel.m_channelName;

		m_channelType = channel.m_channelType;

		m_isRegularChannel = channel.m_isRegularChannel;
	}
/*	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKChannel::operator=()" << endl;*/

	return *this;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: ~LTKChannel
* DESCRIPTION	: destructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

LTKChannel::~LTKChannel()
{
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getChannelType
* DESCRIPTION	: returns data type of the values from the channel
* ARGUMENTS		: 
* RETURNS		: data type of the values from the channel
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

ELTKDataType LTKChannel::getChannelType() const
{
	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKChannel::getChannelType()" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKChannel::getChannelType()" << endl;*/

	return m_channelType;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getChannelName
* DESCRIPTION	: returns the logical name of the channel
* ARGUMENTS		: 
* RETURNS		: logical name of the channel
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

string LTKChannel::getChannelName() const
{
/*	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKChannel::getChannelName()" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKChannel::getChannelName()" << endl;*/

	return m_channelName;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: isRegularChannel
* DESCRIPTION	: returns the type of the channel
* ARGUMENTS		: 
* RETURNS		: type ofthe channel : true for regular, false for intermittent 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

bool LTKChannel::isRegularChannel() const 
{
	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKChannel::isRegularChannel()" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKChannel::isRegularChannel()" << endl;*/

	return m_isRegularChannel;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setChannelType
* DESCRIPTION	: sets the data type of the values from the channel
* ARGUMENTS		: channelType - data type of the values from the channel
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* *****************************************************************************/

void LTKChannel::setChannelType(ELTKDataType channelType)
{
/*	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKChannel::setChannelType()" << endl;*/

	m_channelType = channelType;

	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "m_channelType = " << m_channelType << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKChannel::setChannelType()" << endl;*/

}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setChannelName
* DESCRIPTION	: sets the name of the channel
* ARGUMENTS		: channelName - name of the channel
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

int LTKChannel::setChannelName(const string& channelName)
{
	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKChannel::setChannelName()" << endl;*/

	if( channelName.length() == 0)
	{
        LTKReturnError(EEMPTY_STRING);    
    }

    m_channelName = channelName;

	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "m_channelName = " << m_channelName << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKChannel::setChannelName()" << endl;*/

	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setRegularChannel
* DESCRIPTION	: sets the type of the channel
* ARGUMENTS		: isRegular - type of the channel : true for regular, 
							  false for intermittent
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

void LTKChannel::setRegularChannel(bool isRegular)
{
	/*LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKChannel::setRegularChannel()" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKChannel::setRegularChannel()" << endl;*/

	m_isRegularChannel = isRegular;

}
