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
 * $LastChangedDate: 2008-07-18 15:33:34 +0530 (Fri, 18 Jul 2008) $
 * $Revision: 564 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Defines the function to check the version compatibility
 *
 * CONTENTS:   
 *
 * AUTHOR:     Nidhi Sharma
 *
 * DATE:       May 25, 2007
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#include "LTKVersionCompatibilityCheck.h"
#include "LTKStringUtil.h"
#include "LTKTypes.h"
#include "LTKLoggerUtil.h"

/**********************************************************************************
* AUTHOR		: Nidhi sharma
* DATE			: 25-May-2007
* NAME			: LTKVersionCompatibilityCheck
* DESCRIPTION	: Default Constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
LTKVersionCompatibilityCheck::LTKVersionCompatibilityCheck():m_delimiter(".")
{
}

/**********************************************************************************
* AUTHOR		: Nidhi sharma
* DATE			: 25-May-2007
* NAME			: ~LTKVersionCompatibilityCheck
* DESCRIPTION	: Default Destructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
LTKVersionCompatibilityCheck::~LTKVersionCompatibilityCheck()
{
}

/**********************************************************************************
* AUTHOR		: Nidhi sharma
* DATE			: 25-May-2007
* NAME			: checkCompatibility
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
bool LTKVersionCompatibilityCheck::checkCompatibility(const string& supportedMinVersion,
														const string& currentVersion, 
														const string& versionRead)
														{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKVersionCompatibilityCheck::checkCompatibility()" << endl;
														
    bool returnStatus = false;


    if (versionRead == currentVersion )
    {
        return true;
    }
    
    if (isFirstVersionHigher(versionRead, supportedMinVersion) == true &&
        isFirstVersionHigher(currentVersion, versionRead) == true)
    {
        returnStatus = true;
    }
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKVersionCompatibilityCheck::checkCompatibility()" << endl;

    return returnStatus;
}

bool LTKVersionCompatibilityCheck::isFirstVersionHigher(const string& firstVersion,
                                                        const string& secondVersion)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKVersionCompatibilityCheck::isFirstVersionHigher()" << endl;

    if ( firstVersion == secondVersion )
    {
	 return true;
    }
    
    bool returnVal = false;
    bool equalityObtained = false;
    
    stringVector firstVerTok;
    stringVector secondVerTok;

    LTKStringUtil::tokenizeString(firstVersion,  m_delimiter,  firstVerTok);
    LTKStringUtil::tokenizeString(secondVersion,  m_delimiter,  secondVerTok);

    int versionTokSize = firstVerTok.size();

    if ( versionTokSize != secondVerTok.size())
    {
        return false;
    }

    // compare first tokens
    int cmpResults = compareTokens(atoi((firstVerTok[0]).c_str()), atoi((secondVerTok[0]).c_str()));

    switch (cmpResults)
    {
        case GREATER :
            returnVal = true;
            break;

        case LESSER :
            returnVal = false;
            break;

        case EQUAL :
            equalityObtained = true;
            break;
    }

    if (equalityObtained == false)
    {
        return returnVal;
    }
    else
    {
        equalityObtained = false;
        returnVal = false;
        
        // Compare the second tokens
        int cmpResults2 = compareTokens(atoi((firstVerTok[1]).c_str()), atoi((secondVerTok[1]).c_str()));

        switch (cmpResults2)
        {
            case GREATER :
                returnVal = true;
                break;

            case LESSER :
                returnVal = false;
                break;

            case EQUAL :
                equalityObtained = true;
                break;
        }
    }

    if (equalityObtained == false)
    {
        return returnVal;
    }
    else
    {
        equalityObtained = false;
        returnVal = false;
        
        // Compare the second tokens
        int cmpResults3 = compareTokens(atoi((firstVerTok[2]).c_str()), atoi((secondVerTok[2]).c_str()));

        switch (cmpResults3)
        {
            case GREATER :
                returnVal = true;
                break;

            case LESSER :
                returnVal = false;
                break;

            case EQUAL :
                returnVal = true;
                break;
        }
    }
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKVersionCompatibilityCheck::isFirstVersionHigher()" << endl;
    return returnVal;
    
}

ELTKVersionComparison LTKVersionCompatibilityCheck::compareTokens(int firstTokenIntegerValue, 
																		int secondTokenIntegerValue)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKVersionCompatibilityCheck::compareTokens()" << endl;

    ELTKVersionComparison returnVal;
    
    //int firstTokenIntegerValue =atoi((firstToken).c_str());
    //int secondTokenIntegerValue =atoi((secondToken).c_str());

    if (firstTokenIntegerValue > secondTokenIntegerValue )
    {
        returnVal = GREATER;
    }
    else if (firstTokenIntegerValue < secondTokenIntegerValue )
    {
        returnVal = LESSER;
    }
    else
    {
        returnVal = EQUAL;
    }	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKVersionCompatibilityCheck::compareTokens()" << endl;

    return returnVal;
}