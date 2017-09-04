/*****************************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
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
 * $LastChangedDate$
 * $Revision$
 * $Author$
 *
 ************************************************************************/

/************************************************************************
 * FILE DESCR: Definition of LTKWordRecoConfig holds the config data for
 *			   the recognizer at the time of loading.
 *
 * CONTENTS: 
 *	getClassifierName
 *	getDictionaryPath
 *	getLipiRoot
 *	getNumClasses
 *	getProfile
 *	getScript
 *	getShapeSet
 *	readConfigFile
 *  setLipiRoot
 *
 * AUTHOR:     Mudit Agrawal.
 *
 * DATE:       Mar 2, 2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 * Deepu        24-MAR-2005     Added getGrammarPath
 ************************************************************************/


#include "LTKWordRecoConfig.h"

#include "LTKMacros.h"

#include "LTKErrors.h"

#include "LTKErrorsList.h"

#include "LTKException.h"

#include "LTKLoggerUtil.h"

#include "LTKConfigFileReader.h"


/**********************************************************************************
* AUTHOR		: Mudit Agrawal
* DATE			: 01-MAR-2005
* NAME			: LTKWordRecoConfig
* DESCRIPTION	: DEFAULT CONSTRUCTOR
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/


LTKWordRecoConfig::LTKWordRecoConfig()
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKWordRecoConfig::LTKWordRecoConfig()" << endl;
    m_lipiRoot = "";
    m_problemName = "";
	m_profile = "";
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKWordRecoConfig::LTKWordRecoConfig()" << endl;

}

/**********************************************************************************
* AUTHOR		: Mudit Agrawal
* DATE			: 03-MAR-2005
* NAME			: LTKWordRecoConfig
* DESCRIPTION	: Initialization Constructor
* ARGUMENTS		: lipiRoot
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/


LTKWordRecoConfig::LTKWordRecoConfig(const string& lipiRoot)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKWordRecoConfig::LTKWordRecoConfig(const string&)" << endl;

	if(lipiRoot=="")
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
          <<"Error : "<< EEMPTY_STRING <<":"<< getErrorMessage(EEMPTY_STRING)
          <<" LTKWordRecoConfig::LTKWordRecoConfig(const string&)" <<endl;

		throw LTKException(EEMPTY_STRING);
	}

	m_lipiRoot = lipiRoot;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<  "m_lipiRoot = " << m_lipiRoot <<endl;
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKWordRecoConfig::LTKWordRecoConfig(const string&)" << endl;
}

/**********************************************************************************
* AUTHOR		: Mudit Agrawal
* DATE			: 03-MAR-2005
* NAME			: getClassifierName
* DESCRIPTION	: This function returns the classifier name
* ARGUMENTS		: none
* RETURNS		: returns the classifier name
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/


string LTKWordRecoConfig::getClassifierName() const
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<  "Entering: LTKWordRecoConfig::getClassifierName()" <<endl;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<  "Exiting: LTKWordRecoConfig::getClassifierName()" <<endl;

	return m_classifierName;
}

/**********************************************************************************
* AUTHOR		: Mudit Agrawal
* DATE			: 03-MAR-2005
* NAME			: getDictionaryPath
* DESCRIPTION	: This function returns the Dictionary Path
* ARGUMENTS		: none
* RETURNS		: returns the Dictionary Path
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/


string LTKWordRecoConfig::getDictionaryPath() const
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entered: LTKWordRecoConfig::getDictionaryPath()" <<endl;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting: LTKWordRecoConfig::getDictionaryPath()" <<endl;

	return m_dictionaryPath;
}

/**********************************************************************************
* AUTHOR		: Mudit Agrawal
* DATE			: 03-MAR-2005
* NAME			: getDictionaryPath
* DESCRIPTION	: This function returns the Dictionary Path
* ARGUMENTS		: none
* RETURNS		: returns the Dictionary Path
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/


string LTKWordRecoConfig::getGrammarPath() const
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entered: LTKWordRecoConfig::getGrammarPath()" <<endl;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting: LTKWordRecoConfig::getGrammarPath()" <<endl;

	return m_grammarPath;
}


/**********************************************************************************
* AUTHOR		: Mudit Agrawal
* DATE			: 03-MAR-2005
* NAME			: getLipiRoot
* DESCRIPTION	: This function returns the lipi root
* ARGUMENTS		: none
* RETURNS		: returns the lipi root
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

string LTKWordRecoConfig::getLipiRoot() const
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<  "Entering: LTKWordRecoConfig::getLipiRoot()" <<endl;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<  "Exiting: LTKWordRecoConfig::getLipiRoot()" <<endl;

	return m_lipiRoot;
}



/**********************************************************************************
* AUTHOR		: Mudit Agrawal
* DATE			: 03-MAR-2005
* NAME			: getProfile
* DESCRIPTION	: This function returns the profile
* ARGUMENTS		: none
* RETURNS		: returns the profile
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

string LTKWordRecoConfig::getProfile() const
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<  "Entering: LTKWordRecoConfig::getProfile()" <<endl;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<  "Exiting: LTKWordRecoConfig::getProfile()" <<endl;

	return m_profile;
}


/**********************************************************************************
* AUTHOR		: Mudit Agrawal
* DATE			: 03-MAR-2005
* NAME			: getScript
* DESCRIPTION	: This function returns the script
* ARGUMENTS		: none
* RETURNS		: returns the script
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

string LTKWordRecoConfig::getScript() const
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<  "Entering: LTKWordRecoConfig::getScript()" <<endl;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<  "Exiting: LTKWordRecoConfig::getScript()" <<endl;

	return m_script;
}


/**********************************************************************************
* AUTHOR		: Mudit Agrawal
* DATE			: 23-MAR-2005
* NAME			: getProblemName
* DESCRIPTION	: returns the problem name
* ARGUMENTS		: none
* RETURNS		: returns the problem name
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/


string LTKWordRecoConfig::getProblemName() const
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<  "Entering: LTKWordRecoConfig::getProblemName()" <<endl;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<  "Exiting: LTKWordRecoConfig::getProblemName()" <<endl;

	return m_problemName;
}


/**********************************************************************************
* AUTHOR		: Mudit Agrawal
* DATE			: 03-MAR-2005
* NAME			: setLipiRoot
* DESCRIPTION	: sets the lipiRoot
* ARGUMENTS		: none
* RETURNS		: SUCCEESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKWordRecoConfig::setLipiRoot(const string& lipiRoot)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<  "Entering: LTKWordRecoConfig::setLipiRoot()" <<endl;

	if(lipiRoot=="")
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
          <<"Error : "<< EEMPTY_STRING <<":"<< getErrorMessage(EEMPTY_STRING)
          <<" LTKWordRecoConfig::LTKWordRecoConfig(const string&)" <<endl;

		LTKReturnError(EEMPTY_STRING);
	}

	this->m_lipiRoot = lipiRoot;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<  "m_lipiRoot = " << m_lipiRoot <<endl;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<  "Exiting: LTKWordRecoConfig::setLipiRoot()" <<endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Mudit Agrawal
* DATE			: 03-MAR-2005
* NAME			: readConfigFile
* DESCRIPTION	: reads the main config file and inturn other config files also (defined in main.cfg)
* ARGUMENTS		: none
* RETURNS		: SUCCESS on successful reads
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKWordRecoConfig::readConfigFile(const string& configFileName)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering: LTKWordRecoConfig::readConfigFile()" <<endl;
 
	LTKConfigFileReader* cfgFileMap;

	try
	{
		cfgFileMap= new LTKConfigFileReader(configFileName);

	}
	catch(LTKException e)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
    		<<"Error: LTKWordRecoConfig::readConfigFile()"<<endl;

	    LTKReturnError(e.getErrorCode());

	}

    int errorCode = 0;

	if((errorCode=cfgFileMap->getConfigValue("script", m_script))!=SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
    		<<"Error: LTKWordRecoConfig::readConfigFile()"<<endl;

		LTKReturnError(errorCode);
		
	}

	if((errorCode=cfgFileMap->getConfigValue("problem", m_problemName))!=SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
    		<<"Error: LTKWordRecoConfig::readConfigFile()"<<endl;

		LTKReturnError(errorCode);
		
	}

	if(m_problemName=="")
	{

		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"problem string is empty"<<endl;
		
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
          <<"Error : "<< EEMPTY_STRING <<":"<< getErrorMessage(EEMPTY_STRING)
          <<" LTKWordRecoConfig::readConfigFile()" <<endl;

		LTKReturnError(EEMPTY_STRING);
	}

    if((errorCode=cfgFileMap->getConfigValue("profile", m_profile))!=SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
    		<<"Error: LTKWordRecoConfig::readConfigFile()"<<endl;

		LTKReturnError(errorCode);
		
	}


	if(m_profile=="")
	{

		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"profile string is empty"<<endl;
		
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
          <<"Error : "<< EEMPTY_STRING <<":"<< getErrorMessage(EEMPTY_STRING)
          <<" LTKWordRecoConfig::readConfigFile()" <<endl;

		LTKReturnError(EEMPTY_STRING);
	}

    if((errorCode=cfgFileMap->getConfigValue("dictmap", m_dictionaryPath))!=SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
    		<<"Error: LTKWordRecoConfig::readConfigFile()"<<endl;

		LTKReturnError(errorCode);
		
	}

    if((errorCode=cfgFileMap->getConfigValue("grammarmap", m_grammarPath))!=SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
    		<<"Error: LTKWordRecoConfig::readConfigFile()"<<endl;

		LTKReturnError(errorCode);
		
	}
		
    if(m_lipiRoot=="")
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Lipiroot is empty"<<endl;
		
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
          <<"Error : "<< EEMPTY_STRING <<":"<< getErrorMessage(EEMPTY_STRING)
          <<" LTKWordRecoConfig::LTKWordRecoConfig(const string&)" <<endl;

		LTKReturnError(EEMPTY_STRING);
	}

	cfgFileMap = new LTKConfigFileReader(m_lipiRoot + SEPARATOR + m_problemName
										 + SEPARATOR + WORDFILE);


	cfgFileMap = new LTKConfigFileReader(m_lipiRoot + SEPARATOR + m_problemName 
										 + SEPARATOR + m_profile + SEPARATOR
										 + WORDPROFILEFILE);

	if((errorCode=cfgFileMap->getConfigValue("classifier", 
	  	m_classifierName))!=SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
    		<<"Error: LTKWordRecoConfig::readConfigFile()"<<endl;

		LTKReturnError(errorCode);	
	}

	delete cfgFileMap;

    LOG(LTKLogger::LTK_LOGLEVEL_DEBUG) <<  "Exiting: LTKWordRecoConfig::readConfigFile()" <<endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Mudit Agrawal
* DATE			: 03-MAR-2005
* NAME			: LTKWordRecoConfig
* DESCRIPTION	: DEFAULT DESTRUCTOR
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/


LTKWordRecoConfig::~LTKWordRecoConfig()
{
}

