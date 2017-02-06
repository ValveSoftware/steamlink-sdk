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
 * $LastChangedDate: 2011-01-11 13:48:17 +0530 (Tue, 11 Jan 2011) $
 * $Revision: 827 $
 * $Author: mnab $
 *
 ************************************************************************/
 
/************************************************************************
 * FILE DESCR: Implementation of the String Splitter Module
 *
 * CONTENTS: 
 *	tokenizeString
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#include "LTKStringUtil.h"
#include "LTKMacros.h"
#include "LTKLoggerUtil.h"
#include <sstream>

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKStringUtil
* DESCRIPTION	: Default Constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKStringUtil::LTKStringUtil(){}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: tokenizeString
* DESCRIPTION	: splits the given string at the specified delimiters
* ARGUMENTS		: str - string to be split
*				  outTokens - the vector containing the split parts of the string
*				  delimiters - the symbols at which the string is to be split
* RETURNS		: SUCCESS on successfully splitting of the string
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* Saravanan. R		29-3-07				dynamic allocation for strString
*************************************************************************************/

int LTKStringUtil::tokenizeString(const string& inputString, 
										  const string& delimiters, 
										  vector<string>& outTokens)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
	  " Entering: LTKStringUtil::tokenizeString()" << endl;

    //	clearing the token list
	int inputStrLength = inputString.length();
	int delimLength = delimiters.length();


	char *strToken;
	char *strString = new char[inputStrLength + 1];

	outTokens.clear();

	strcpy(strString, inputString.c_str());

	strToken = strtok(strString, delimiters.c_str());
	
	while( strToken != NULL )
	{
		outTokens.push_back(strToken);
		
		// Get next token: 
		strToken = strtok(NULL, delimiters.c_str());
	}
	delete[] strString;
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
	  " Exiting: LTKStringUtil::tokenizeString()" << endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: ~LTKStringUtil
* DESCRIPTION	: destructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKStringUtil::~LTKStringUtil(){}


/*****************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: ~LTKStringUtil
* DESCRIPTION	: destructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*****************************************************************************/
void LTKStringUtil::trimString(string& str)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
	  " Entering: LTKStringUtil::trimString()" << endl;

	string::size_type pos = str.find_last_not_of(' ');

	if(pos != string::npos)
	{
		// we found a non space char
		str.erase(pos + 1);
		pos = str.find_first_not_of(' ');

		if(pos != string::npos)
		{
			str.erase(0, pos);
		}
	}
	else
	{
		// string has nothing else but spaces
	  	str.erase(str.begin(), str.end());
	}
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
	  " Exiting: LTKStringUtil::trimString()" << endl;
}


/***************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 29-Jan-2008
* NAME			: convertIntegerToString
* DESCRIPTION	: Converts an integer variable to string
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
***************************************************************************/
void LTKStringUtil::convertIntegerToString(int intVariable, string& outStr)
{
    ostringstream tempString;

    tempString << intVariable;

    outStr = tempString.str();
}

/***************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 29-Jan-2008
* NAME			: convertIntegerToString
* DESCRIPTION	: Converts an integer variable to string
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
***************************************************************************/
void LTKStringUtil::convertFloatToString(float floatVariable, string& outStr)
{
    ostringstream tempString;

    tempString << floatVariable;

    outStr = tempString.str();
}

/***************************************************************************
* AUTHOR		:
* DATE			: 18-Mar-2015
* NAME			: convertStringToFloat
* DESCRIPTION	: Converts string to float
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
***************************************************************************/
float LTKStringUtil::convertStringToFloat(const string& str)
{
    float rval;
    stringstream ss(str);
    ss.imbue(locale("C"));
    ss >> rval;
    return rval;
}

/***************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 29-Jan-2008
* NAME			: isNumeric
* DESCRIPTION	: Retruns true if the input string is numeric
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*******************************************************************************/
bool LTKStringUtil::isFloat(const string& inputStr)
{
	string remainingStr = "";

	// check if prefixed with a sign
	if (inputStr.find_first_of('-') == 0 || inputStr.find_first_of('+') == 0)
	{
		remainingStr = inputStr.substr(1);
	}
	else
	{
		remainingStr = inputStr;
	}

	// check if two decimals are there like 9.8.8
	string::size_type pos = remainingStr.find_first_of('.');
	
	if (pos != string::npos)
	{
		string tempString = remainingStr.substr(pos+1);

		string::size_type pos2 = tempString.find_first_of('.');

		if (pos2 != string::npos)
		{
			return false;
		}
	}
	

	const char* ptr = remainingStr.c_str();

	for ( ; *ptr; ptr++)
	{
		if (*ptr < '0' || *ptr > '9' )
		{
			if ( *ptr != '.' )
			{
				return false;
			}
		}
	}

  return true;
}

/***************************************************************************
* AUTHOR		: Srinivasa Vithal
* DATE			: 18-Feb-2008
* NAME			: isFloat
* DESCRIPTION	: Retruns true if the input string is integer
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*******************************************************************************/
bool LTKStringUtil::isInteger(const string& inputStr)
{
	string remainingStr = "";
	
	// check if prefixed with a sign
	if (inputStr.find_first_of('-') == 0 || inputStr.find_first_of('+') == 0)
	{
		remainingStr = inputStr.substr(1);
	}
	else
	{
		remainingStr = inputStr;
	}
	
	// check if a decimal is present
	string::size_type pos = remainingStr.find_first_of('.');
	
	if (pos != string::npos)
	{
			return false;
	}

	const char* ptr = remainingStr.c_str();

	for ( ; *ptr; ptr++)
	{
		if (*ptr < '0' || *ptr > '9' )
		{
				return false;
		}
	}
	
  	return true;
}
