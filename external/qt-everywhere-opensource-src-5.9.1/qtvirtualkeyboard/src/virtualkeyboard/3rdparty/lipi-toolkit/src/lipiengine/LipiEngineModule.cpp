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
*******************************************************************************/

/******************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2011-02-08 11:00:11 +0530 (Tue, 08 Feb 2011) $
 * $Revision: 832 $
 * $Author: dineshm $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation for PCA Shape Recognition module
 *
 * CONTENTS:
 *  initializeLipiEngine
 *  createShapeRecognizer
 *  createWordRecognizer
 *  deleteShapeRecognizer
 *  deleteWordRecognizer
 *  createRecognitionContext
 *  deleteRecognitionContext
 *  getcurrentversion
 *  getLipiRootPath
 *  mapShapeAlgoModuleFunctions
 *  mapWordAlgoModuleFunctions
 *  resolveLogicalNameToProjectProfile
 *
 * AUTHOR:     Thanigai Murugan K
 *
 * DATE:       July 29, 2005
 * CHANGE HISTORY:
 * Author       Date            Description of change
 ************************************************************************/

#include "LipiEngineModule.h"
#include "LTKConfigFileReader.h"
#include "LTKStringUtil.h"
#include "lipiengine.h"
#include "LTKException.h"
#include "LTKOSUtilFactory.h"
#include "LTKOSUtil.h"
#include "LTKLoggerUtil.h"

extern int unloadAllModules();
extern int deleteModule(void* RecoHandle);
extern void addModule(void* RecoHandle, void* handle);

LTKLipiEngineModule* LTKLipiEngineModule::lipiEngineModuleInstance= NULL;

/******************************************************************************
* AUTHOR        : Nidhi 
* DATE          : 06-Dec-2006
* NAME          : LTKLipiEngineModule
* DESCRIPTION   : constructor
* ARGUMENTS     : None
* RETURNS       : None
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
LTKLipiEngineModule::LTKLipiEngineModule():
m_LipiEngineConfigEntries(NULL),
m_logFileName(DEFAULT_LOG_FILE),
m_logLevel(DEFAULT_LOG_LEVEL),
m_OSUtilPtr(LTKOSUtilFactory::getInstance())
{
}

/******************************************************************************
* AUTHOR        : Thanigai 
* DATE          : 29-JUL-2005
* NAME          : ~LTKLipiEngineModule
* DESCRIPTION   : Destructor
* ARGUMENTS     : None
* RETURNS       : None
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
LTKLipiEngineModule::~LTKLipiEngineModule()
{
	delete m_LipiEngineConfigEntries;
    delete m_OSUtilPtr;

    LTKLoggerUtil::destroyLogger();
}

/******************************************************************************
* AUTHOR        : Thanigai 
* DATE          : 29-JUL-2005
* NAME          : initializeLipiEngine
* DESCRIPTION   : Reads the lipiengine.cfg and store the AVP map into 
*				  the map variable "m_LipiEngineConfigEntries"
* ARGUMENTS     : 
* RETURNS       : 0 on success other values error
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
int LTKLipiEngineModule::initializeLipiEngine()
{
	//Log messages after call to configureLogger()	
	
	string temp;
	int errorCode;
	
	if(m_strLipiRootPath == "")
	{
		LTKReturnError(ELIPI_ROOT_PATH_NOT_SET);	// PATH not set
	}
	if(m_strLipiLibPath == "")
	{
		m_strLipiLibPath = m_strLipiRootPath + SEPARATOR + "lib";
	}

	temp = m_strLipiRootPath + SEPARATOR + "projects" + SEPARATOR +
		   LIPIENGINE_CFG_STRING;

	//Read the logical name mapping file from lipiengine.cfg file;
	try
	{
	    m_LipiEngineConfigEntries = new LTKConfigFileReader(temp);	
		
	}
	catch(LTKException e)
	{
		// display warning to the user and continue with default values
		cout << " Could not open file : " << temp << endl <<
		        "proceeding with defaults" << endl;
	}

    errorCode = configureLogger();	// Configure the logger
	
	if(errorCode !=SUCCESS)
	{
		LTKReturnError(errorCode);
	}
	else
	{
		LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
		"LTKLipiEngineModule::initializeLipiEngine()"<<endl;
	}

	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Nidhi sharma
* DATE			: 11-Jan-2007
* NAME			: LTKLipiEngineModule::getInstance
* DESCRIPTION	: Returns a pointer to the LTKLipiEngineModule instance
* ARGUMENTS		: 
* RETURNS		: Address of the LTKLipiEngineModule instance.
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* 
******************************************************************************/
LTKLipiEngineModule* LTKLipiEngineModule::getInstance()
{
	if(lipiEngineModuleInstance == NULL)
	{
		lipiEngineModuleInstance = new LTKLipiEngineModule();
	}

	return lipiEngineModuleInstance;
}

/******************************************************************************
* AUTHOR		: Nidhi sharma
* DATE			: 02-Jul-2008
* NAME			: destroyLipiEngineInstance
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* 
******************************************************************************/
void LTKLipiEngineModule::destroyLipiEngineInstance()
{
    if(lipiEngineModuleInstance != NULL)
	{
		delete lipiEngineModuleInstance;
		lipiEngineModuleInstance = NULL;
	}
}

/******************************************************************************
* AUTHOR        : Thanigai 
* DATE          : 29-JUL-2005
* NAME          : createShapeRecognizer
* DESCRIPTION   : create an instance of shape recognizer object and call 
*				  initialize function. Also loads the model data. 
* ARGUMENTS     : strProjectName - project name; strProfileName - profile name
*                 outShapeRecognizerPtr - return shape recognizer object 
* RETURNS       : if success returns 0 or if fails returns error code 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
int LTKLipiEngineModule::createShapeRecognizer(const string& strProjName, 
                                        const string& strProfName, 
					LTKShapeRecognizer** outShapeRecoObj)
{
	LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
		"Entering: LTKLipiEngineModule::createShapeRecognizer()"<<endl;
	
	int errorCode;
    int iResult = 0;
	void *dllHandler = NULL;
	string recognizerName = "";
	string strProjectName = strProjName;
    string strProfileName = strProfName;
    
	//Validating Project names and profile names
	errorCode = validateProjectAndProfileNames(strProjectName, strProfileName,
											   "SHAPEREC", recognizerName); 
	if (errorCode != SUCCESS)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
    			"Error: " << getErrorMessage(errorCode) << 
    			"LTKLipiEngineModule::createShapeRecognizer()"<<endl;

		LTKReturnError(errorCode);
	}

    // Load the dlaal of the shape recognizer
	errorCode = loadRecognizerDLL(recognizerName, &dllHandler);
    
	if( errorCode!= SUCCESS)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
    			"Error: " << getErrorMessage(errorCode) << 
    			"LTKLipiEngineModule::createShapeRecognizer()"<<endl;

		LTKReturnError(errorCode);
	}

	// Map Algo DLL functions...
	errorCode = mapShapeAlgoModuleFunctions(dllHandler);
	
	if( errorCode!= SUCCESS)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
    			"Error: " << getErrorMessage(errorCode) << 
    			"LTKLipiEngineModule::createShapeRecognizer()"<<endl;

		LTKReturnError(errorCode);
	}

    // Create control Info object
    char currentVersion[VERSION_STR_LEN];
    int iMajor, iMinor, iBugfix;
    
    getToolkitVersion(iMajor, iMinor, iBugfix);
	sprintf(currentVersion, "%d.%d.%d", iMajor, iMinor, iBugfix);
    
    LTKControlInfo controlInfo;
    controlInfo.lipiRoot = m_strLipiRootPath;
    controlInfo.lipiLib = m_strLipiLibPath;
    controlInfo.projectName = strProjectName;
    controlInfo.profileName = strProfileName;
    controlInfo.toolkitVersion = currentVersion;
    
	// Call recognition module's createShapeRecognizer(); 
	errorCode = module_createShapeRecognizer(controlInfo,outShapeRecoObj);
    
    if(errorCode !=SUCCESS)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
        "Error: "<<	getErrorMessage(ECREATE_SHAPEREC) << " "<< recognizerName <<
		"LTKLipiEngineModule::createShapeRecognizer()"<<endl;

		m_OSUtilPtr->unloadSharedLib(dllHandler);
        
		LTKReturnError(ECREATE_SHAPEREC);	
	}

	addModule(*outShapeRecoObj, dllHandler);

	LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
		"Exiting: LTKLipiEngineModule::createShapeRecognizer()"<<endl;

	return SUCCESS;
	
}
/******************************************************************************
* AUTHOR        : Balaji MNA 
* DATE          : 14-JUN-2009
* NAME          : createShapeRecognizer
* DESCRIPTION   : resolves logical project name and call createWordRecognizer
*				  function.  
* ARGUMENTS     : strLogicalProjectName - logical project name; 
*                 outShapeRecognizerPtr - return shape recognizer object 
* RETURNS       : if success returns 0 or if fails returns error code 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
int LTKLipiEngineModule::createShapeRecognizer(string &strLogicalProjectName, LTKShapeRecognizer** outShapeRecognizerPtr) 
{
	int errorCode;
	LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
		"Entering: LTKLipiEngineModule::createShapeRecognizer()"<<endl;

	if(strLogicalProjectName.empty())
	{		
		return EINVALID_PROJECT_NAME;
	}

	string strProjectName = "";
    string strProfileName = "";

	// Resolve the logical name into project name and profile name
	errorCode = resolveLogicalNameToProjectProfile(strLogicalProjectName, 
		                                           strProjectName, 
		                                           strProfileName);

	if(errorCode !=SUCCESS)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
			"Error: " << getErrorMessage(errorCode) << 
			"LTKLipiEngineModule::createShapeRecognizer()"<<endl;

		LTKReturnError(errorCode);
	}

	errorCode = createShapeRecognizer(strProjectName,strProfileName,outShapeRecognizerPtr);

	if(errorCode !=SUCCESS)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
			"Error: " << getErrorMessage(errorCode) << 
			"LTKLipiEngineModule::createShapeRecognizer()"<<endl;

		LTKReturnError(errorCode);
	}

	LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
		"Exiting: LTKLipiEngineModule::createShapeRecognizer()"<<endl;

	return SUCCESS;

}

/******************************************************************************
* AUTHOR        : Thanigai 
* DATE          : 29-JUL-2005
* NAME          : createWordRecognizer
* DESCRIPTION   : create an instance of word recognizer object and call initialize
*				  function. Also loads the model data. 
* ARGUMENTS     : strProjectName - project name; strProfileName - profile name
*                 outShapeRecognizerPtr - return shape recognizer object 
* RETURNS       : if success returns 0 or if fails returns error code 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
int LTKLipiEngineModule::createWordRecognizer(const string& strProjName,
                                              const string& strProfName,
                                              LTKWordRecognizer** outWordRecoObj)
{
	
	LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
		"Entering: LTKLipiEngineModule::createWordRecognizer()"<<endl;

	string recognizerName = "";
	int iResult = 0;
	void *dllHandler = NULL;
	string strProjectName = strProjName;
    string strProfileName = strProfName;
    
	int errorCode;

	errorCode = validateProjectAndProfileNames(strProjectName,strProfileName, 
											   "WORDREC", recognizerName); 
	if (errorCode != SUCCESS)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
    			"Error: " << getErrorMessage(errorCode) << 
    			"LTKLipiEngineModule::createWordRecognizer()"<<endl;


		LTKReturnError(errorCode);
	}

    // Load the dll of the word recognizer

	errorCode = loadRecognizerDLL(recognizerName, &dllHandler);

	if( errorCode!= SUCCESS)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
    			"Error: " << getErrorMessage(errorCode) << 
    			"LTKLipiEngineModule::createWordRecognizer()"<<endl;


		LTKReturnError(errorCode);
	}

	// Get version information
    char currentVersion[VERSION_STR_LEN];
    int iMajor, iMinor, iBugFix;
    getToolkitVersion(iMajor, iMinor, iBugFix);
    sprintf(currentVersion, "%d.%d.%d", iMajor, iMinor, iBugFix);
    
    // Create controlInfo object
    LTKControlInfo controlInfo;

    controlInfo.lipiRoot        = m_strLipiRootPath;
    controlInfo.lipiLib         = m_strLipiLibPath;
    controlInfo.projectName     = strProjectName;
    controlInfo.profileName     = strProfileName;
    controlInfo.toolkitVersion  = currentVersion;
    
	// Map Algo DLL functions...
	errorCode = mapWordAlgoModuleFunctions(dllHandler);
	if(errorCode!= SUCCESS)
	{
		// Unable to map the functions
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
    			"Error: " << getErrorMessage(errorCode) << 
    			"LTKLipiEngineModule::createWordRecognizer()"<<endl;


		LTKReturnError(errorCode); 
	}

	// Call recognition module's createWordRecognizer(); 
	errorCode = module_createWordRecognizer(controlInfo,outWordRecoObj );
	if(errorCode != SUCCESS)
	{
		/* Error, unable to create word recognizer instance */
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
        "Error: "<< getErrorMessage(ECREATE_WORDREC) << " "<<recognizerName<<
		"LTKLipiEngineModule::createWordRecognizer()"<<endl;

		m_OSUtilPtr->unloadSharedLib(dllHandler);
        
		LTKReturnError(ECREATE_WORDREC);	
	}

	addModule(*outWordRecoObj, dllHandler);
	
	LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
		"Exiting: LTKLipiEngineModule::createWordRecognizer()"<<endl;

	return SUCCESS;
}
/******************************************************************************
* AUTHOR        : Balaji MNA 
* DATE          : 14-JUN-2009
* NAME          : createWordRecognizer
* DESCRIPTION   : resolves logical project name and call createWordRecognizer
*				  function.  
* ARGUMENTS     : strLogicalProjectName - logical project name; 
*                 outShapeRecognizerPtr - return shape recognizer object 
* RETURNS       : if success returns 0 or if fails returns error code 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
int LTKLipiEngineModule::createWordRecognizer(const string& strlogicalProjectName, 
                              LTKWordRecognizer** outWordRecPtr)
{
	LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
		"Entering: LTKLipiEngineModule::createWordRecognizer()"<<endl;

	int errorCode;
	string strProjectName;
    string strProfileName;

	errorCode = resolveLogicalNameToProjectProfile(strlogicalProjectName, 
	                                               strProjectName, 
										           strProfileName);

	if(errorCode !=SUCCESS)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
			"Error: " << getErrorMessage(errorCode) << 
			"LTKLipiEngineModule::createWordRecognizer()"<<endl;

		LTKReturnError(errorCode);
	}

	errorCode = createWordRecognizer(strProjectName,strProfileName, 
											    outWordRecPtr); 
	if (errorCode != SUCCESS)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
    			"Error: " << getErrorMessage(errorCode) << 
    			"LTKLipiEngineModule::createWordRecognizer()"<<endl;

		LTKReturnError(errorCode);
	}

	LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
		"Exiting: LTKLipiEngineModule::createWordRecognizer()"<<endl;

	return SUCCESS;
}

/******************************************************************************
* AUTHOR        : Thanigai 
* DATE          : 29-JUL-2005
* NAME          : deleteShapeRecognizer
* DESCRIPTION   : To delete the recognizer object which is created 
*				  using "createShapeRecognizer" call
* ARGUMENTS     : obj - handle the previously created recognizer object
* RETURNS       : 0 on success other values error
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
* Balaji MNA        18th Jan 2010       Receiving LTKShapeRecognizer as single pointer 
*                                       instead of double pointer in deleteShapeRecognizer
******************************************************************************/
int LTKLipiEngineModule::deleteShapeRecognizer(LTKShapeRecognizer* obj)
{
	LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
		"Entering: LTKLipiEngineModule::deleteShapeRecognizer()"<<endl;

	int errorCode;

	//Call recognition module's deleteShapeRecognizer(obj) 
	if(obj)
	{
		deleteModule(obj);		
		errorCode = module_deleteShapeRecognizer(obj);
		if(errorCode !=SUCCESS)
		{
			LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
    			"Error: " << getErrorMessage(errorCode) << 
    			"LTKLipiEngineModule::deleteShapeRecognizer()"<<endl;
            
			LTKReturnError(errorCode);
		}	
		obj = NULL;
	}
	
	LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
		"Exiting: LTKLipiEngineModule::deleteShapeRecognizer()"<<endl;
	
	return SUCCESS;
}
	
/******************************************************************************
* AUTHOR        : Thanigai 
* DATE          : 29-JUL-2005
* NAME          : deleteWordRecognizer
* DESCRIPTION   : To delete the recognizer object which is created 
*				  using "createWordRecognizer" call
* ARGUMENTS     : obj - handle the previously created recognizer object
* RETURNS       : 0 on success other values error
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
int LTKLipiEngineModule::deleteWordRecognizer(LTKWordRecognizer* obj)
{
	LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
		"Entering: LTKLipiEngineModule::deleteWordRecognizer()"<<endl;

	int errorCode;

	//	Call recognition module's deleteWordRecognizer(obj) 
	if(obj)
	{
		deleteModule(obj);		
		errorCode = module_deleteWordRecognizer(obj);
		if(errorCode !=SUCCESS)
		{
			LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
    			"Error: " << getErrorMessage(errorCode) << 
    			"LTKLipiEngineModule::deleteShapeRecognizer()"<<endl;
            
			LTKReturnError(errorCode);
		}	
		obj = NULL;
	}
	
	LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
		"Exiting: LTKLipiEngineModule::deleteWordRecognizer()"<<endl;
	
	return SUCCESS;

}

/******************************************************************************
* AUTHOR        : Thanigai 
* DATE          : 29-JUL-2005
* NAME          : getLipiRootPath
* DESCRIPTION   : To fetch the value for the environment variable LIPI_ROOT
* ARGUMENTS     : None
* RETURNS       : environment string on success & NULL on error
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
string LTKLipiEngineModule::getLipiRootPath() const
{
	LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
		"Entering: LTKLipiEngineModule::getLipiRootPath()"<<endl;

	LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
		"Exiting: LTKLipiEngineModule::getLipiRootPath()"<<endl;

	return m_strLipiRootPath;
}

/******************************************************************************
* AUTHOR        :
* DATE          :
* NAME          : getLipiLibPath
* DESCRIPTION   : To fetch the value for the environment variable LIPI_LIB
* ARGUMENTS     : None
* RETURNS       : environment string on success & NULL on error
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
string LTKLipiEngineModule::getLipiLibPath() const
{
	LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
		"Entering: LTKLipiEngineModule::getLipiLibPath()"<<endl;

	LOG(LTKLogger::LTK_LOGLEVEL_INFO)<<
		"Exiting: LTKLipiEngineModule::getLipiLibPath()"<<endl;

	return m_strLipiLibPath;
}

/******************************************************************************
* AUTHOR			: Nidhi Sharma 
* DATE				: 10-JAN-2007
* NAME				: setLipiRootPath
* DESCRIPTION		: To set the value of LIPI_ROOT
* ARGUMENTS			: String
* RETURNS			: 
* NOTES 			:
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
void LTKLipiEngineModule::setLipiRootPath(const string& appLipiPath)
{

	//Dont LOG messages as this may be called before configureLogger()

	if ( appLipiPath.empty())
	{
		m_strLipiRootPath = m_OSUtilPtr->getEnvVariable(LIPIROOT_ENV_STRING);
	}
	else
	{
		m_strLipiRootPath = appLipiPath;
	}
}

/******************************************************************************
* AUTHOR			:
* DATE				:
* NAME				: setLipiLibPath
* DESCRIPTION		: To set the value of LIPI_LIB
* ARGUMENTS			: String
* RETURNS			:
* NOTES 			:
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
void LTKLipiEngineModule::setLipiLibPath(const string& appLipiLibPath)
{

	//Dont LOG messages as this may be called before configureLogger()

	if ( appLipiLibPath.empty())
	{
		m_strLipiLibPath = m_OSUtilPtr->getEnvVariable(LIPILIB_ENV_STRING);
	}
	else
	{
		m_strLipiLibPath = appLipiLibPath;
	}
}

/******************************************************************************
* AUTHOR        : Thanigai 
* DATE          : 29-JUL-2005
* NAME          : mapShapeAlgoModuleFunctions
* DESCRIPTION   : To map function addresses of the methods exposed by 
*				  the shape recognition modules
* ARGUMENTS     : None
* RETURNS       : 0 on success and other values on error 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
int LTKLipiEngineModule::mapShapeAlgoModuleFunctions(void *dllHandle)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
    "Entering: LTKLipiEngineModule::mapShapeAlgoModuleFunctions()"<<endl;
	

	module_createShapeRecognizer = NULL;
	module_deleteShapeRecognizer = NULL;

    void* functionHandle = NULL;

    int returnVal = m_OSUtilPtr->getFunctionAddress(dllHandle, 
                                            CREATESHAPERECOGNIZER_FUNC_NAME, 
                                            &functionHandle);
    
    // Could not map the  function from the DLL
	if(returnVal != SUCCESS)
	{
	    LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
        "Error: "<<
		getErrorMessage(EDLL_FUNC_ADDRESS) <<" "<<CREATESHAPERECOGNIZER_FUNC_NAME
		<<" "<<"LTKLipiEngineModule::mapShapeAlgoModuleFunctions()"<<endl;


        m_OSUtilPtr->unloadSharedLib(dllHandle);

		LTKReturnError(EDLL_FUNC_ADDRESS);
	}

    module_createShapeRecognizer = (FN_PTR_CREATESHAPERECOGNIZER)functionHandle;

	functionHandle = NULL;

    returnVal = m_OSUtilPtr->getFunctionAddress(dllHandle, 
                                            DELETESHAPERECOGNIZER_FUNC_NAME, 
                                            &functionHandle);
    
    // Could not map the  function from the DLL
	if(returnVal != SUCCESS)
	{
	    LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
        "Error: "<<
		getErrorMessage(EDLL_FUNC_ADDRESS) <<" "<<DELETESHAPERECOGNIZER_FUNC_NAME
		<<" "<<"LTKLipiEngineModule::mapShapeAlgoModuleFunctions()"<<endl;

        m_OSUtilPtr->unloadSharedLib(dllHandle);

		LTKReturnError(EDLL_FUNC_ADDRESS);
	}

    module_deleteShapeRecognizer = (FN_PTR_DELETESHAPERECOGNIZER)functionHandle;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
    "Exiting: LTKLipiEngineModule::mapShapeAlgoModuleFunctions()"<<endl;
	
	return SUCCESS;
}

/******************************************************************************
* AUTHOR        : Thanigai 
* DATE          : 29-JUL-2005
* NAME          : mapWordAlgoModuleFunctions
* DESCRIPTION   : To map function addresses of the methods exposed by 
*				  the word recognition modules
* ARGUMENTS     : None
* RETURNS       : 0 on success and other values on error 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
int LTKLipiEngineModule::mapWordAlgoModuleFunctions(void *dllHandle)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
    "Entering: LTKLipiEngineModule::mapWordAlgoModuleFunctions()"<<endl;
	
	module_createWordRecognizer = NULL;
	module_deleteWordRecognizer = NULL;

    void* functionHandle = NULL;

    int returnVal = m_OSUtilPtr->getFunctionAddress(dllHandle, 
                                            CREATEWORDRECOGNIZER_FUNC_NAME, 
                                            &functionHandle);
    
    // Could not map the  function from the DLL
	if(returnVal != SUCCESS)
	{
	    LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
        "Error: "<<
		getErrorMessage(EDLL_FUNC_ADDRESS) <<" "<<CREATEWORDRECOGNIZER_FUNC_NAME
		<<" "<<"LTKLipiEngineModule::mapWordAlgoModuleFunctions()"<<endl;

        m_OSUtilPtr->unloadSharedLib(dllHandle);

		LTKReturnError(EDLL_FUNC_ADDRESS);
	}

    module_createWordRecognizer = (FN_PTR_CREATEWORDRECOGNIZER)functionHandle;

	functionHandle = NULL;

    returnVal = m_OSUtilPtr->getFunctionAddress(dllHandle, 
                                            DELETEWORDRECOGNIZER_FUNC_NAME, 
                                            &functionHandle);
    
    // Could not map the  function from the DLL
	if(returnVal != SUCCESS)
	{
	    LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
        "Error: "<<
		getErrorMessage(EDLL_FUNC_ADDRESS) <<" "<<DELETEWORDRECOGNIZER_FUNC_NAME
		<<" "<<"LTKLipiEngineModule::mapWordAlgoModuleFunctions()"<<endl;

        m_OSUtilPtr->unloadSharedLib(dllHandle);

		LTKReturnError(EDLL_FUNC_ADDRESS);
	}

    module_deleteWordRecognizer = (FN_PTR_DELETEWORDRECOGNIZER)functionHandle;


	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
    "Exiting: LTKLipiEngineModule::mapWordAlgoModuleFunctions()"<<endl;
	
	return SUCCESS;
}


/******************************************************************************
* AUTHOR        : Thanigai 
* DATE          : 29-JUL-2005
* NAME          : resolveLogicalNameToProjectProfile
* DESCRIPTION   : Resolves logical project name into project and profile name
* ARGUMENTS     : strLogicalProjectName - logical project name
* RETURNS       : 0 on success and -1 on error
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
int LTKLipiEngineModule::resolveLogicalNameToProjectProfile(
												const string &strLogicalName, 
                                                string &outProjectName,
                                                string &outProfileName)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
       "Entering: LTKLipiEngineModule::resolveLogicalNameToProjectProfile()"<<endl;

	char strSep[] = " ()\r";
	char *strToken;

	if (m_LipiEngineConfigEntries == NULL )
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
       "Error: " << EFILE_OPEN_ERROR  << getErrorMessage(EFILE_OPEN_ERROR) <<
       " lipiengine.cfg " <<
       "LTKLipiEngineModule::resolveLogicalNameToProjectProfile()"<<endl;

		LTKReturnError(EFILE_OPEN_ERROR);
	}

	if(m_LipiEngineConfigEntries->isConfigMapEmpty())
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
        "Error: "<<
		getErrorMessage(ENOMAPFOUND_LIPIENGINECFG) <<
		"LTKLipiEngineModule::resolveLogicalNameToProjectProfile()"<<endl;

		LTKReturnError(ENOMAPFOUND_LIPIENGINECFG);
	}

	string strLogicalProjectName ;
	m_LipiEngineConfigEntries->getConfigValue(strLogicalName, 
											  strLogicalProjectName);
    
	char *strSearch = (char *)strLogicalProjectName.c_str();
	
	strToken = strtok(strSearch, strSep);
	if(strToken)
	{
		strToken[strlen(strToken)] = '\0';
		outProjectName = strToken;
	}
	else
	{
		// No token found, invalid entry for project name...
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
        "Error: "<< getErrorMessage(ENO_TOKEN_FOUND) <<
		"LTKLipiEngineModule::resolveLogicalNameToProjectProfile()"<<endl;

		LTKReturnError(ENOMAPFOUND_LIPIENGINECFG);
	}

	strToken = strtok(NULL, strSep);
	if(strToken)
	{
		strToken[strlen(strToken)] = '\0';
		outProfileName = strToken;

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
       "Exiting: LTKLipiEngineModule::resolveLogicalNameToProjectProfile()"<<endl;

		return SUCCESS;
	}
	else
	{
		// No token found, invalid entry for profile name...
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
        "Error: "<< getErrorMessage(ENO_TOKEN_FOUND) <<
		"LTKLipiEngineModule::resolveLogicalNameToProjectProfile()"<<endl;

		LTKReturnError(ENOMAPFOUND_LIPIENGINECFG);
	}

	
	// No value found, invalid logical name
	LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
		"Error: "<< getErrorMessage(EINVALID_LOGICAL_NAME) <<
		"LTKLipiEngineModule::resolveLogicalNameToProjectProfile()"<<endl;

	LTKReturnError(EINVALID_LOGICAL_NAME);
}

/******************************************************************************
* AUTHOR		: Nidhi sharma
* DATE			: 08-Feb-2007
* NAME			: LTKLipiEngineModule::validateProject
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* 
*******************************************************************************/
int LTKLipiEngineModule::validateProject(const string& strProjectName, 
										 const string& projectType)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enterng: LTKLipiEngineModule::validateProject()"<<endl;
	
	LTKConfigFileReader* projectConfigReader = NULL;
	
	string projectTypeCfgEntry = "";

	if(strProjectName == "")
	{
		/* invalid or no entry for project name */
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
        "Error: "<< getErrorMessage(EINVALID_PROJECT_NAME) <<
		"LTKLipiEngineModule::validateProject()"<<endl;

        LTKReturnError(EINVALID_PROJECT_NAME);
	}

	string projectCfgPath = m_strLipiRootPath + PROJECTS_PATH_STRING +  
					strProjectName + PROFILE_PATH_STRING + PROJECT_CFG_STRING;
	
	try
	{
		projectConfigReader = new LTKConfigFileReader(projectCfgPath);
	}
	catch(LTKException e)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
        "Error: "<< getErrorMessage(e.getErrorCode()) << projectCfgPath <<
		" LTKLipiEngineModule::validateProject()"<<endl;

		LTKReturnError(EINVALID_PROJECT_NAME);
	}
	
	int errorCode = projectConfigReader->getConfigValue(PROJECT_TYPE_STRING,
															projectTypeCfgEntry);
	
	if( errorCode != SUCCESS || projectTypeCfgEntry != projectType)
	{
		/* Invalid configuration entry for ProjectType */	
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
            "Error: "<< getErrorMessage(EINVALID_PROJECT_TYPE) <<
    		" LTKLipiEngineModule::validateProject()"<<endl;

		delete projectConfigReader;
		//Project type in CFG is missing or an invalid value
		LTKReturnError(EINVALID_PROJECT_TYPE); 
	}

	delete projectConfigReader;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exiting: LTKLipiEngineModule::validateProject()"<<endl;
		
	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Nidhi sharma
* DATE			: 08-Feb-2007
* NAME			: LTKLipiEngineModule::validateProfile
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* 
******************************************************************************/
int LTKLipiEngineModule::validateProfile(const string& strProjectName, 
										 const string& strProfileName, 
	 									 const string& projectType, 
										 string& outRecognizerString)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Entering: LTKLipiEngineModule::validateProfile()"<<endl;
	
	int errorCode; 
	string profileCfgPath = m_strLipiRootPath + PROJECTS_PATH_STRING + 
						strProjectName + PROFILE_PATH_STRING + strProfileName 
						+ SEPARATOR + PROFILE_CFG_STRING;

	LTKConfigFileReader* profileConfigReader = NULL;
	
	try
	{
		profileConfigReader = new LTKConfigFileReader(profileCfgPath);
	}
	catch(LTKException e)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
            "Error: "<< getErrorMessage(e.getErrorCode()) << profileCfgPath <<
    		" LTKLipiEngineModule::validateProfile()"<<endl;

		LTKReturnError(e.getErrorCode());
	}

	errorCode = profileConfigReader->getConfigValue(projectType, 
													outRecognizerString);
	
	if(errorCode != SUCCESS)
	{
		/* No recognizer specified. */
		if (projectType == SHAPE_RECOGNIZER_STRING )
		{
			errorCode = ENO_SHAPE_RECOGNIZER;
		}
		else
		{
			errorCode = ENO_WORD_RECOGNIZER;
		}

		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
            "Error: "<< getErrorMessage(errorCode) << profileCfgPath <<
    		" LTKLipiEngineModule::validateProfile()"<<endl;
	
		delete profileConfigReader;		

		LTKReturnError(errorCode); 
	}

	delete profileConfigReader;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exiting: LTKLipiEngineModule::validateProfile()"<<endl;
	
	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Saravanan R.
* DATE			: 09-Feb-2007
* NAME			: LTKLipiEngineModule::validateProjectAndProfileNames
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
******************************************************************************/
int LTKLipiEngineModule::validateProjectAndProfileNames(
												const string& strProjectName, 
												const string& strProfileName,
												const string& projectType,
												string& outRecognizerString)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
		"Entering: LTKLipiEngineModule::validateProjectAndProfileNames()"<<endl;

	int errorCode;
	string recognizerType = "";
	string profileName(strProfileName);

	// Validate Project
	errorCode = validateProject(strProjectName, projectType);
	if ( errorCode!= SUCCESS )
	{
		LTKReturnError(errorCode);
	}
	
	
	if(projectType == "SHAPEREC")
    {   
		recognizerType = SHAPE_RECOGNIZER_STRING;
    }
	else
    {   
		recognizerType = WORD_RECOGNIZER_STRING;
    }

	// Validate Profile
	if(strProfileName == "")
	{
		//Assume the "default" profile
		profileName = DEFAULT_PROFILE; 
	}

	errorCode = validateProfile(strProjectName, profileName, 
								recognizerType, outRecognizerString);
	if ( errorCode!= SUCCESS)
	{
		LTKReturnError(errorCode);
	}

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
		"Exiting: LTKLipiEngineModule::validateProjectAndProfileNames()"<<endl;

	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Saravanan R.
* DATE			: 09-Feb-2007
* NAME			: 
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* 
******************************************************************************/
int LTKLipiEngineModule::loadRecognizerDLL(const string& recognizerName,
										   void **dllHandler)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
		"Entering: LTKLipiEngineModule::loadRecognizerDLL()"<<endl;
	
	string recognizerDLLPath = "";
	int returnVal = SUCCESS;

    returnVal = m_OSUtilPtr->loadSharedLib(m_strLipiLibPath, recognizerName, dllHandler);

    
	if(returnVal != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<
		"Error: "<< getErrorMessage(ELOAD_SHAPEREC_DLL) <<" "<<recognizerDLLPath <<
		"LTKLipiEngineModule::loadRecognizerDLL()"<<endl;

		LTKReturnError(ELOAD_SHAPEREC_DLL); 
	}
    
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
		"Exiting: LTKLipiEngineModule::loadRecognizerDLL()"<<endl;
	
	return SUCCESS;
}

/******************************************************************************
* AUTHOR        : Nidhi Sharma
* DATE          : 07-May-2007
* NAME          : getLogFileName
* DESCRIPTION   : To fetch the value for m_logFileName
* ARGUMENTS     : None
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
string LTKLipiEngineModule::getLogFileName() const
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
		"Entering: LTKLipiEngineModule::getLogFileName()"<<endl;
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
		"Exiting: LTKLipiEngineModule::getLogFileName()"<<endl;
	
	return m_logFileName;
}


/******************************************************************************
* AUTHOR        : Nidhi Sharma
* DATE          : 07-May-2007
* NAME          : getLogLevel
* DESCRIPTION   : To fetch the value for m_logFileName
* ARGUMENTS     : None
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
LTKLogger::EDebugLevel LTKLipiEngineModule::getLogLevel() const
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
		"Entering: LTKLipiEngineModule::getLogLevel()"<<endl;
	
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
		"Exiting: LTKLipiEngineModule::getLogLevel()"<<endl;
	
	return m_logLevel;
}

/******************************************************************************
* AUTHOR			: Nidhi Sharma 
* DATE				: 07-May-2007
* NAME				: setLogFileName
* DESCRIPTION		: To set the value of m_logFileName
* ARGUMENTS			: String
* RETURNS			: Nothing
* NOTES 				:
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
int LTKLipiEngineModule::setLipiLogFileName(const string& appLogFile)
{
	// No log messages as this function is used to configure logger
	if ( appLogFile.length() != 0 )
    {
    	m_logFileName = appLogFile;
    }
    else
    {
		LTKReturnError(EINVALID_LOG_FILENAME); 
    }
	
	return SUCCESS;
}

/******************************************************************************
* AUTHOR			: Nidhi Sharma 
* DATE				: 07-May-2007
* NAME				: setLipiLogLevel
* DESCRIPTION		: To set the value of m_logLevel
* ARGUMENTS			: String
* RETURNS			: Nothing
* NOTES 				:
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
int LTKLipiEngineModule::setLipiLogLevel(const string& appLogLevel)
{

	// No log messages as this function is used to configure logger
	string strLogLevel = "";
    
	if ( appLogLevel.length() != 0 )
	{
		strLogLevel= appLogLevel;
	}
	else
	{
		LTKReturnError(EINVALID_LOG_LEVEL); 
	}

	const char * strLogLevelPtr = strLogLevel.c_str();
	// mapping m_LogLevel to Logger log levels

	if(LTKSTRCMP(strLogLevelPtr, LOG_LEVEL_DEBUG) == 0)
	{
		m_logLevel = LTKLogger::LTK_LOGLEVEL_DEBUG;
	}
	else if(LTKSTRCMP(strLogLevelPtr, LOG_LEVEL_ALL) == 0)
	{
		m_logLevel = LTKLogger::LTK_LOGLEVEL_ALL;
	}
	else if(LTKSTRCMP(strLogLevelPtr, LOG_LEVEL_VERBOSE) == 0)
	{
		m_logLevel = LTKLogger::LTK_LOGLEVEL_VERBOSE;
	}
	else if(LTKSTRCMP(strLogLevelPtr, LOG_LEVEL_ERROR) == 0)
	{
		m_logLevel = LTKLogger::LTK_LOGLEVEL_ERR;
	}
	else if(LTKSTRCMP(strLogLevelPtr, LOG_LEVEL_OFF) == 0)
	{
		m_logLevel = LTKLogger::LTK_LOGLEVEL_OFF;
	}
	else if(LTKSTRCMP(strLogLevelPtr, LOG_LEVEL_INFO) == 0)
	{
		m_logLevel = LTKLogger::LTK_LOGLEVEL_INFO;
	}
	else
	{
		LTKReturnError(EINVALID_LOG_LEVEL); 
	}

	return SUCCESS;
}
/******************************************************************************
* AUTHOR			: Nidhi Sharma 
* DATE				: 07-May-2007
* NAME				: configureLogger
* DESCRIPTION		: Configures the logger
* ARGUMENTS			: None
* RETURNS			: Nothing
* NOTES 				:
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
int LTKLipiEngineModule::configureLogger()
{
	// No log messages as this function is used to configure logger
    string valueFromCFG = "";
    int errorCode ;
    
	if ( m_LipiEngineConfigEntries != NULL )
	{
		// Read the log file name from lipiengine.cfg
		errorCode = m_LipiEngineConfigEntries->getConfigValue(LOG_FILE_NAME,
															  valueFromCFG);

		if(errorCode ==SUCCESS)
		{
			setLipiLogFileName(valueFromCFG);
		}
        else if (errorCode == EKEY_NOT_FOUND )
        {
            // Set default log file
        }
		else
		{
			LTKReturnError(ECREATE_LOGGER);
		}

		// Read the log level
		valueFromCFG = "";
		
		errorCode = m_LipiEngineConfigEntries->getConfigValue(LOG_LEVEL, valueFromCFG);

		if(errorCode == SUCCESS)
		{
			setLipiLogLevel(valueFromCFG);
		}
		else if (errorCode == EKEY_NOT_FOUND )
        {
            // Set default log file
        }
		else
		{
			LTKReturnError(ECREATE_LOGGER);
		}
	}

    LTKLoggerUtil::createLogger(m_strLipiLibPath);
    LTKLoggerUtil::configureLogger(m_logFileName, m_logLevel);
    
	return SUCCESS;
}