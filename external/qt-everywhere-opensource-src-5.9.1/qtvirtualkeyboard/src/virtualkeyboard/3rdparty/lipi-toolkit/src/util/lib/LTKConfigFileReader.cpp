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
 * $LastChangedDate: 2011-02-08 14:54:14 +0530 (Tue, 08 Feb 2011) $
 * $Revision: 833 $
 * $Author: dineshm $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation of the Configuration File Reader Module
 *
 * CONTENTS:   
 *	getMap
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of LTKConfigFileReader
 ************************************************************************/

#include "LTKConfigFileReader.h"

#include "LTKStringUtil.h"

#include "LTKException.h"

#include "LTKErrorsList.h"

#include "LTKMacros.h"

#include "LTKErrors.h"

#include "LTKLoggerUtil.h"

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKConfigFileReader
* DESCRIPTION	: Default Constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of LTKConfigFileReader ctor
* Nidhi Sharma	08-FEB-2007
******************************************************************************/

LTKConfigFileReader::LTKConfigFileReader(const string& configFilePath):
m_configFilePath(configFilePath)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Entering: LTKConfigFileReader::LTKConfigFileReader()" << endl;

	// Read the config file into stringStringMap
	int errorCode = getMap();

	if (errorCode != SUCCESS )
	{
	// logger message
	 LOG( LTKLogger::LTK_LOGLEVEL_ERR) 
        <<"Error: LTKConfigFileReader::LTKConfigFileReader()"<<endl;
	throw LTKException(errorCode);
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Exiting: LTKConfigFileReader::LTKConfigFileReader()" << endl;
}

/***************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getMap
* DESCRIPTION	: reads string-value pair into an asoociative array
* ARGUMENTS		: cfgFile - the name of the configuration file from which the string-value pairs 
*				  are to be read from
* RETURNS		: an associative array indexable on the string to the value
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of getMap
*Nidhi Sharma	08-FEB-2007		Made this function a private member function of the class
*****************************************************************************/

int LTKConfigFileReader::getMap()
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Entering: LTKConfigFileReader::getMap()" << endl;

	string line = "";						//	a line read from the config file

	vector<string> strTokens;				//	string list found in the line read

	//	opening the config file

	ifstream cfgFileHandle(m_configFilePath.c_str());

	//	checking if the file open was successful

	if(!cfgFileHandle)
	{
		// logger message
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Unable to Open Config file :"<< m_configFilePath<<endl;
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EFILE_OPEN_ERROR <<": "<< 
                  getErrorMessage(EFILE_OPEN_ERROR)
                  <<"LTKConfigFileReader::getMap() => Config File Not Found" <<endl;

		LTKReturnError(EFILE_OPEN_ERROR);
	}

	//	reading lines of the config file
	while(getline(cfgFileHandle, line, NEW_LINE_DELIMITER))
	{
		// trim the line
		LTKStringUtil::trimString(line);

		if(line.empty())
		{
		//	skipping over empty line
			continue;
		}
		else if((line[0] == COMMENTCHAR))
		{
		//	skipping over commented lines
			continue;
		}
		else
		{
			LTKStringUtil::tokenizeString(line,  "=",  strTokens);
			
			if(strTokens.size() == 2)
			{
				LTKStringUtil::trimString(strTokens[0]);
				LTKStringUtil::trimString(strTokens[1]);
				
				m_cfgFileMap[strTokens[0]] = strTokens[1];
			}
			else
			{
				// Logger message
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EINVALID_CFG_FILE_ENTRY <<": "<< 
                  getErrorMessage(EINVALID_CFG_FILE_ENTRY)
                  <<"LTKConfigFileReader::getMap()" <<endl;

                //	closing the config file
            	cfgFileHandle.close();
                
				LTKReturnError(EINVALID_CFG_FILE_ENTRY);
			}
		}
		//	populating the name value pair associative array (map)
	}

	//	closing the config file
	cfgFileHandle.close();

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Exiting: LTKConfigFileReader::getMap()" << endl;

	return SUCCESS;
}

/****************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: ~LTKConfigFileReader
* DESCRIPTION	: destructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of ~LTKConfigFileReader
*****************************************************************************/

LTKConfigFileReader::~LTKConfigFileReader(){}

/**************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 08-FEB-2007
* NAME			: getConfigValue
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of getConfigValue
****************************************************************************/
int LTKConfigFileReader::getConfigValue(const string& key, string& outValue)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Entering: LTKConfigFileReader::getConfigValue()" << endl;

	stringStringMap::const_iterator cfgItem = m_cfgFileMap.find(key);
	if (cfgItem != m_cfgFileMap.end() )
	{
		outValue = cfgItem->second.c_str();
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Exiting: LTKConfigFileReader::getConfigValue()" << endl;
		return SUCCESS;
	}
	// logger mesage
	int errorCode = EKEY_NOT_FOUND;
	LTKReturnError(errorCode);
}

/****************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 08-FEB-2007
* NAME			: isConfigMapEmpty
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of getConfigValue
****************************************************************************/
bool LTKConfigFileReader::isConfigMapEmpty()
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Entering: LTKConfigFileReader::isConfigMapEmpty()" << endl;

	bool returnBool = false;

	if ( m_cfgFileMap.empty() )
	{
		returnBool = true;
	}
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Exiting: LTKConfigFileReader::isConfigMapEmpty()" << endl;

	return returnBool;
}

/***************************************************************************
* AUTHOR		: Srinivasa Vithal
* DATE			: 21-NOV-2007
* NAME			: getCfgFileMap
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of getConfigValue
****************************************************************************/
const stringStringMap& LTKConfigFileReader::getCfgFileMap()
{
	return m_cfgFileMap;
}
