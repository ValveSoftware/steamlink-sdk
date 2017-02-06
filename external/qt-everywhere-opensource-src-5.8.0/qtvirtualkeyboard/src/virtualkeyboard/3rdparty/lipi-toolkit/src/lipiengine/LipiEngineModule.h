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
 * $LastChangedDate: 2011-01-18 15:41:43 +0530 (Tue, 18 Jan 2011) $
 * $Revision: 829 $
 * $Author: mnab $
 *
 ************************************************************************/

/************************************************************************
 * FILE DESCR: LTKLipiEngineModule implementation header derives from LipiEngineModule 
 *
 * CONTENTS:
 *  
 *
 * AUTHOR:     Thanigai Murugan K
 *
 * DATE:       July 29 2005
 * CHANGE HISTORY:
 * Author       Date            Description of change
 ************************************************************************/

#ifndef __LIPIENGINEMODULE_H_
#define __LIPIENGINEMODULE_H_


#ifndef _WIN32
#include  <dlfcn.h>
#else
#include <windows.h>
#endif

#include "LTKLipiEngineInterface.h"
#include "LTKMacros.h"
#include "LTKErrorsList.h"
#include "LTKErrors.h"
#include "LTKLoggerUtil.h"

class LTKConfigFileReader;
class LTKOSUtil;

/** @defgroup LipiEngineModule LipiEngineModule
*@brief The LipiEngineModule
*/

/** @ingroup  LipiEngineModule
* @brief Controlling class responsible for creating, 
  initializing and  deleting the shape/word recognizers.
* @class LTKLipiEngineModule
*/
class LTKLipiEngineModule : public LTKLipiEngineInterface
{
private:
    /** @brief 
    * Pointer to the createShapeRecognizer method in the respective shape recognizers
    */
	typedef int (*FN_PTR_CREATESHAPERECOGNIZER)(const LTKControlInfo&,  
												LTKShapeRecognizer**);
	FN_PTR_CREATESHAPERECOGNIZER module_createShapeRecognizer;

    /** @brief 
	* Pointer to the deleteShapeRecognizer method in the respective shape recognizers
    */
	typedef int (*FN_PTR_DELETESHAPERECOGNIZER)(LTKShapeRecognizer*);
	FN_PTR_DELETESHAPERECOGNIZER module_deleteShapeRecognizer;

    /** @brief 
	* Pointer to the createWordRecognizer method in the respective shape recognizers
    */
	typedef int (*FN_PTR_CREATEWORDRECOGNIZER)(const LTKControlInfo&, 
	                                               LTKWordRecognizer**);
	FN_PTR_CREATEWORDRECOGNIZER module_createWordRecognizer;

    /** @brief 
    * Pointer to the deleteWordRecognizer method in the respective shape recognizers
    */
	typedef int (*FN_PTR_DELETEWORDRECOGNIZER)(LTKWordRecognizer*);
	FN_PTR_DELETEWORDRECOGNIZER module_deleteWordRecognizer;

	
	string m_strLipiRootPath; // LIPI_ROOT environment path 
	string m_strLipiLibPath; // LIPI_LIB environment path

    string m_logFileName;

    LTKLogger::EDebugLevel m_logLevel;

    LTKOSUtil* m_OSUtilPtr;

	static LTKLipiEngineModule* lipiEngineModuleInstance;


public:
    
	/** @brief  Stores the key-value pairs defined in lipiengine.cfg
	*/
    LTKConfigFileReader* m_LipiEngineConfigEntries;


public:

	static LTKLipiEngineModule* getInstance();

    static void destroyLipiEngineInstance();


	/** @brief Returns LTKLipiEngineModule::m_strLipiRootPath 
	*/
	string getLipiRootPath() const;

	/** @brief Returns LTKLipiEngineModule::m_strLipiLibPath
	*/
	string getLipiLibPath() const;

	/** @brief Sets the data member LTKLipiEngineModule::m_strLipiRootPath
	*/
	void setLipiRootPath(const string& appLipiPath="");

	/** @brief Sets the data member LTKLipiEngineModule::m_strLipiLibPath
	*/
	void setLipiLibPath(const string& appLipiLibPath="");

    /** @brief Returns LTKLipiEngineModule::m_logFileName 
	*/
	string getLogFileName() const;

	/** @brief Sets the data member LTKLipiEngineModule::m_logFileName
	*/
    
	int setLipiLogFileName(const string& appLogFile);

    /** @brief Returns LTKLipiEngineModule::m_logLevel 
	*/
    LTKLogger::EDebugLevel getLogLevel() const;

    /** @brief Sets the data member LTKLipiEngineModule::m_logLevel
	*/
	int setLipiLogLevel(const string& appLogLevel);

	/** @brief Initializes the lipiengine.
	* <p>
	* Reads the lipiengine.cfg and intializes the data member 
	* LTKLipiEngineModule::m_LipiEngineConfigEntries
	* 
	* Semantics
	* - Return error LTKErrorsList::ELIPI_ROOT_PATH_NOT_SET if 
    *   LTKLipiEngineModule::m_strLipiRootPath is empty
	*
	* - Read the contents of lipiengine.cfg to 
	*   LTKLipiEngineModule::m_LipiEngineConfigEntries
	*<pre> m_LipiEngineConfigEntries = new LTKConfigFileReader(temp) </pre>
	*
	* </p>
	* @return SUCCESS/Error
	* @error LTKErrorsList::ELIPI_ROOT_PATH_NOT_SET
	*/
	int initializeLipiEngine();

	/* destructor */
	virtual ~LTKLipiEngineModule();

	/**
	* @brief Returns the current version of the lipiengine module
	* @param iMajor, iMinor, iBugFix - integers to receive the version 
	* @return 0 on success
	*/
    int getcurrentversion(int& iMajor, int& iMinor, int& iBugFix);
	

	/**
	* @brief Given project and profile, returns an instance of shape recognizers
	*
	* Semantics
	*
	* - Validate project using LTKLipiEngineModule::validateProject
	*
	* - Validate profile using LTKLipiEngineModule::validateProfile
	*
	* - Load the recognizer DLL
	*
	* - Map the DLL functions using LTKLipiEngineModule::mapShapeAlgoModuleFunctions
	*
	* - Create the shape recognizers
	*
	* - Add the recognizer to the list of active modules using 
	*   LTKLipiEngineModule::AddModule
	* - Initialize the shape recognizers calling the recognizers initialize function
	*
	* @param strProjectName  Type : string Project Name 
    *								
	* @param strProfileName  Type : string Profile Name  
	*	
	* @return handle on success, NULL on failure
	* @return NULL in following scenarios
	*   - Project not valid
	*       - Project name empty : LTKErrorsList::EINVALID_PROJECT_NAME
	*       - Could not open project.cfg : LTKErrorsList::ECONFIG_FILE_OPEN
	*       - ProjectType is not SHAPEREC : LTKErrorsList::EINVALID_CONFIG_ENTRY
	*
	*   - Profile not valid
	*       - Could not open profile.cfg : LTKErrorsList::ECONFIG_FILE_OPEN
	*       - For shape recognizers
	*           - profile.cfg does not contain key ShapeRecMethod: 
	*                          LTKErrorsList::ENO_SHAPE_RECOGNIZER
	*
	*   - Fail to load recognizer DLL
	*       - could not load DLL : LTKErrorsList::ELOAD_SHAPEREC_DLL
	*
    *   - Fail to map recognizer DLL functions: LTKErrorsList::EDLL_FUNC_ADDRESS
	*   - Recogniser createShapeRecognizer failed
	*   - Recognizers initialize function failed
	*/
    int createShapeRecognizer(const string& strProjName, const string& strProfName, 
						      LTKShapeRecognizer** outShapeRecoObj);

	/**
	* @brief Given project and profile, returns an instance of shape recognizers
	*
	* Semantics
	* - resolve the Logical name passed into logical  
    *   project and profile name using 
    *   LTKLipiEngineModule::resolveLogicalNameToProjectProfile <pre>
	*
	* - Calls createShapeRecognizer method by passed project name 
	*   and profile name.
	*
	* @return handle on success, NULL on failure
	* @return NULL in following scenarios
	*   - Project not valid
	*       - Project name empty : LTKErrorsList::EINVALID_PROJECT_NAME
	*       - Could not open project.cfg : LTKErrorsList::ECONFIG_FILE_OPEN
	*       - ProjectType is not SHAPEREC : LTKErrorsList::EINVALID_CONFIG_ENTRY
	*
	*   - Profile not valid
	*       - Could not open profile.cfg : LTKErrorsList::ECONFIG_FILE_OPEN
	*       - For shape recognizers
	*           - profile.cfg does not contain key ShapeRecMethod: 
	*                          LTKErrorsList::ENO_SHAPE_RECOGNIZER
	*
	*/
	int createShapeRecognizer(string &strLogicalProjectName, LTKShapeRecognizer** outShapeRecognizerPtr) ;

	/**
	* Use this to delete the shape recognizer object created using 
    * createShapeRecognizer call
	* @param handle to the recognizer object to delete
	* @return 0 on success
	*/
	int deleteShapeRecognizer(LTKShapeRecognizer* obj);

	/**
	* Use this to create word recognizer object, by passing the project name 
    * and profile name
	* @param projectname + profile name  
	*        
	* @return 0 on success
	*/
	int createWordRecognizer(const string& strProjectName, 
                                 const string& strProfileName,
                                  LTKWordRecognizer** outWordRecPtr);

	/**
	* Use this to create word recognizer object, by passing the logical project name 
    * 
	* @return 0 on success
	*/
	int createWordRecognizer(const string& strlogicalProjectName, 
                                  LTKWordRecognizer** outWordRecPtr);

	/**
	* Use this to delete the word recognizer object created using createWordRecognizer call
	* @param handle to the recognizer object to delete
	* @return 0 on success
	*/
	int deleteWordRecognizer(LTKWordRecognizer* obj);
   


private :

	LTKLipiEngineModule();

   	int validateProject(const string& strProjectName, const string& projectType);
	
    int validateProfile(const string& strProjectName, const string& strProfileName, 
	 				    const string& projectType, string& outRecognizerString);
	
    int validateProjectAndProfileNames(const string& strProjectName, 
										const string& strProfileName,
										const string& projectType,
									    string& outRecognizerString );
									   

	int loadRecognizerDLL(const string& recognizerName, void **dllHandler);
	
	/**
	* This map functions get the function address from the DLL/SO module
	* Maps the following functions
	*  - createShapeRecognizer
	*  - deleteShapeRecognizer
	* 
	* @param DLLHandle - handle to the DLL/SO module
	* @return 0 on success
	* @error EDLL_FUNC_ADDRESS  :  Returned if the function could not be maped
	*/
	int mapShapeAlgoModuleFunctions(void *DLLHandle);

	/**
	* This map functions get the function address from the DLL/SO module
	* Maps the following functions
	*  - createWordRecognizer
	*  - deleteWordRecognizer
	* 
	* @param DLLHandle - handle to the DLL/SO module
	* @return 0 on success
	* @error EDLL_FUNC_ADDRESS  :  Returned if the function could not be maped
	*/
	int mapWordAlgoModuleFunctions(void *DLLHandle);

	/* To resolve the logical name into Project + Profile name */
	int resolveLogicalNameToProjectProfile(const string& strLogicalProjectName, 
								string &outProjectName, string &outProfileName);

    int configureLogger();
	

};

#endif //#ifndef __LIPIENGINEMODULE_H_

