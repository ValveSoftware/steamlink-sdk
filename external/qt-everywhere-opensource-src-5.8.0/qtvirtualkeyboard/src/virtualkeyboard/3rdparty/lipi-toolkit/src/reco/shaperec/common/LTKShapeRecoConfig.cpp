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
 * $LastChangedDate: 2011-02-08 11:00:11 +0530 (Tue, 08 Feb 2011) $
 * $Revision: 832 $
 * $Author: dineshm $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation of LTKShapeRecoConfig which holds the configuration information read 
 *			   from the configuration files
 *
 * CONTENTS:   
 *	readConfigInfo
 *	getLipiRoot
 *	getShapeSet
 *	getProfile
 *	getShapeRecognizerName
 *	getNumberOfShapes
 *	setLipiRoot
 *	setShapeSet
 *	setProfile
 *	setShapeRecognizerName
 *	setNumShapes
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#include "LTKShapeRecoConfig.h"
#include "LTKErrorsList.h"
#include "LTKLoggerUtil.h"
#include "LTKErrors.h"
#include "LTKShapeRecoConfig.h"
#include "LTKException.h"
#include "LTKConfigFileReader.h"
#include "LTKMacros.h"

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKShapeRecoConfig
* DESCRIPTION	: Default Constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKShapeRecoConfig::LTKShapeRecoConfig()
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Entered default constructor of LTKShapeRecoConfig"  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exiting default constructor of LTKShapeRecoConfig"  << endl;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKShapeRecoConfig
* DESCRIPTION	: Constructor initializing the member variables
* ARGUMENTS		: lipiRoot - root directory of the Lipi Tool Kit
*				  shapeSet - shape set to be used for training/recognition
*				  profile - profile to be used for training/recognition
*				  shapeRecognizerName - name of the shape recognizer to be used for training/recognition
*				  numShapes - number of shapes in the recognition problem
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ************************************************************************************/

LTKShapeRecoConfig::LTKShapeRecoConfig(const  string& lipiRoot, const string& shapeSet,
					 const  string& profile, const string& shapeRecognizerName, int numShapes) :
					m_lipiRoot(lipiRoot), m_shapeSet(shapeSet), m_profile(profile),
					m_shapeRecognizerName(shapeRecognizerName), m_numShapes(numShapes)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Entered initialization constructor of LTKShapeRecoConfig"  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "m_lipiRoot = " << m_lipiRoot  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "m_shapeSet = " << m_shapeSet  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "m_profile = " << m_profile  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "m_shapeRecognizerName = " << m_shapeRecognizerName  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "m_numShapes = " << m_numShapes  << endl;

	int errorCode;
	if(m_numShapes <= 0)
	{
		errorCode = EINVALID_NUM_OF_SHAPES;

		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
            "Invalid value for number of shapes :" << m_numShapes << endl;
		
		throw LTKException(errorCode);
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exiting initialization constructor of LTKShapeRecoConfig"  << endl;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: readConfig
* DESCRIPTION	: Reads the configuration files
* ARGUMENTS		: configFile - name of the main configuration file
* RETURNS		: SUCCESS on successful reading of the configuration file
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ************************************************************************************/

int LTKShapeRecoConfig::readConfigInfo(const string& configFile)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Entered LTKShapeRecoConfig::readConfig"  << endl;

	string shapeSetCfg;						//	shape set configuration file

	string profileCfg;						//	profile configuration file

	LTKConfigFileReader* mainProperties;			//	main config file name value pairs
	
	LTKConfigFileReader* shapesetProperties;		//	shapeset config name value pairs

	LTKConfigFileReader* profileProperties;		//	profile config name value pairs

	//	reading the main configuration file

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
	    "Main configuration file is " << configFile  << endl;

	mainProperties = new LTKConfigFileReader(configFile);

	LOG( LTKLogger::LTK_LOGLEVEL_INFO) << 
        "Main configuration read"  << endl;

	//	setting config information
	
	mainProperties->getConfigValue("shapeset", m_shapeSet);

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "shapeSet = " << m_shapeSet  << endl;

	mainProperties->getConfigValue("profile", m_profile);

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "profile = " << m_profile  << endl;

	//	composing the shape set configuration file name

	shapeSetCfg = m_lipiRoot + SEPARATOR + m_shapeSet + SEPARATOR + SHAPESETFILE;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Shapeset configuration file is " << shapeSetCfg  << endl;

	//	reading the shape set configuration file

	shapesetProperties = new LTKConfigFileReader(shapeSetCfg);

	LOG( LTKLogger::LTK_LOGLEVEL_INFO) << 
        "Shapeset configuration read"  << endl;

	//	setting config information
	string numShapesStr;
	shapesetProperties->getConfigValue(PROJECT_CFG_ATTR_NUMSHAPES_STR, numShapesStr);
	m_numShapes = atoi(numShapesStr.c_str());

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "m_numShapes = " << m_numShapes  << endl;

	if(m_numShapes <= 0)
	{
		delete mainProperties;
		delete shapesetProperties;

		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
            "Invalid value for number of shapes : " << m_numShapes << endl;

		LTKReturnError(EINVALID_NUM_OF_SHAPES); // Error while reading project.cfg 

		//throw LTKException("numShapes cannot be less than or equal to zero");
	}

	//	composing the proile configuration file name
	
	profileCfg = m_lipiRoot + SEPARATOR + m_shapeSet + SEPARATOR + m_profile + SEPARATOR + PROFILEFILE;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Profile configuration file is " << profileCfg  << endl;

	//	reading the profile configuration file

	profileProperties = new LTKConfigFileReader(profileCfg);

	LOG( LTKLogger::LTK_LOGLEVEL_INFO) << "Profile configuration read"  << endl;

	//	setting config information

	profileProperties->getConfigValue(SHAPE_RECOGNIZER_STRING, m_shapeRecognizerName);

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "m_shapeRecognizerName = " << m_shapeRecognizerName  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  "Exiting LTKShapeRecoConfig::readConfig"  << endl;

	delete mainProperties;
	delete shapesetProperties;
	delete profileProperties;
	return SUCCESS;
	
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getLipiRoot
* DESCRIPTION	: returns the root directory of the lipi tool kit
* ARGUMENTS		: 
* RETURNS		: root directory of the lipi tool kit
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ************************************************************************************/

const string& LTKShapeRecoConfig::getLipiRoot()
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Entered LTKShapeRecoConfig::getLipiRoot"  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exiting LTKShapeRecoConfig::getLipiRoot with return value " <<
        m_lipiRoot  << endl;

	return m_lipiRoot;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getShapeSet
* DESCRIPTION	: returns the shape set to be used for training/recognition
* ARGUMENTS		: 
* RETURNS		: shape set to be used for training/recognition
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ************************************************************************************/

const string& LTKShapeRecoConfig::getShapeSet()
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Entered LTKShapeRecoConfig::getShapeSet"  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exiting LTKShapeRecoConfig::getShapeSet with return value " <<
        m_shapeSet  << endl;

	return m_shapeSet;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getProfile
* DESCRIPTION	: returns the profile to be used for training/recognition
* ARGUMENTS		: 
* RETURNS		: profile to be used for training/recognition
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

const string& LTKShapeRecoConfig::getProfile()
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Entered LTKShapeRecoConfig::getProfile"  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exiting LTKShapeRecoConfig::getProfile with return value " <<
        m_profile  << endl;

	return m_profile;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getShapeRecognizerName
* DESCRIPTION	: returns the name of the shape recognizer being used
* ARGUMENTS		: 
* RETURNS		: name of the shape recognizer being used
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ************************************************************************************/

const string& LTKShapeRecoConfig::getShapeRecognizerName()
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Entered LTKShapeRecoConfig::getShapeRecognizerName"  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exiting LTKShapeRecoConfig::getShapeRecognizerName with return value " <<
        m_shapeRecognizerName  << endl;

	return m_shapeRecognizerName;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getNumShapes
* DESCRIPTION	: returns the number of shapes in the recognition problem
* ARGUMENTS		: 
* RETURNS		: number of shapes in the recognition problem
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ************************************************************************************/

int LTKShapeRecoConfig::getNumShapes()
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Entered LTKShapeRecoConfig::getNumShapes"  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exiting LTKShapeRecoConfig::getNumShapes with return value " << m_numShapes << endl;

	return m_numShapes;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setLipiRoot
* DESCRIPTION	: sets the root directory of the lipi tool kit
* ARGUMENTS		: lipiRootStr - root directory of the lipi tool kit
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ************************************************************************************/

int LTKShapeRecoConfig::setLipiRoot(const string& lipiRootStr)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Entered LTKShapeRecoConfig::setLipiRoot"  << endl;

	this->m_lipiRoot = lipiRootStr;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "m_lipiRoot = " << m_lipiRoot  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exiting LTKShapeRecoConfig::setLipiRoot"  << endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setShapeSet
* DESCRIPTION	: sets the shape set to be used for training/recognition
* ARGUMENTS		: shapeSetStr - shape set to be used for training/recognition
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKShapeRecoConfig::setShapeSet(const string& shapeSetStr)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Entered LTKShapeRecoConfig::setShapeSet"  << endl;

	this->m_shapeSet = shapeSetStr;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "m_shapeSet = " << m_shapeSet  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exiting LTKShapeRecoConfig::setShapeSet"  << endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setProfile
* DESCRIPTION	: sets the profile to be used for training/recognition
* ARGUMENTS		: profileStr - profile to be used for training/recognition
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKShapeRecoConfig::setProfile(const string& profileStr)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Entered LTKShapeRecoConfig::setProfile"  << endl;

	this->m_profile = profileStr;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "m_profile = " << m_profile  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exiting LTKShapeRecoConfig::setProfile"  << endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setShapeRecognizerName
* DESCRIPTION	: sets the name of the shape recognizer to be used for training/recognition
* ARGUMENTS		: shapeRecognizerName - name of the shape recognizer to be used for training/recognition
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ************************************************************************************/

int LTKShapeRecoConfig::setShapeRecognizerName(const string& shapeRecognizerName)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Entered LTKShapeRecoConfig::setShapeRecognizerName"  << endl;

	this->m_shapeRecognizerName = shapeRecognizerName;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "m_shapeRecognizerName = " << m_shapeRecognizerName  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exiting LTKShapeRecoConfig::setShapeRecognizerName"  << endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setNumShapes
* DESCRIPTION	: sets the number of shapes in the recongition problem
* ARGUMENTS		: numShapes - number of shapes in the recongition problem
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ************************************************************************************/

int LTKShapeRecoConfig::setNumShapes(int numShapes)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Entered LTKShapeRecoConfig::setNumShapes"  << endl;

	this->m_numShapes = numShapes;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "m_numShapes = " << m_numShapes  << endl;

	if(m_numShapes <= 0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) <<
            "Invalid value for number of shapes : " << m_numShapes << endl;

		LTKReturnError(EINVALID_NUM_OF_SHAPES)
		//return ECONFIG_FILE_OPEN_ERR; // Error while reading project.cfg 

		//throw LTKException("m_numShapes cannot be less than or equal to zero");
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exiting LTKShapeRecoConfig::setNumShapes"  << endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: ~LTKShapeRecoConfig
* DESCRIPTION	: destructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ************************************************************************************/

LTKShapeRecoConfig::~LTKShapeRecoConfig()
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Entered destructor of LTKShapeRecoConfig"  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exiting destructor of LTKShapeRecoConfig"  << endl;
}
