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
 * $LastChangedDate: 2008-08-01 09:48:58 +0530 (Fri, 01 Aug 2008) $
 * $Revision: 583 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/***************************************************************************
 * FILE DESCR: DLL Implementation file for LipiEngine module
 *
 * CONTENTS:
 *  DllMain
 *  createLTKLipiEngine
 *
 * AUTHOR:     Thanigai Murugan K
 *
 * DATE:       July 29, 2005
 * CHANGE HISTORY:
 * Author       Date            Description of change
 ************************************************************************/
// lipiengine.cpp : Defines the entry point for the DLL application.


#include "version.h"
#include "lipiengine.h"
#include "LipiEngineModule.h"
#include "LTKErrorsList.h"
#include "LTKErrors.h"
#include "LTKOSUtilFactory.h"
#include "LTKOSUtil.h"
#include "LTKLoggerUtil.h"

vector<MODULEREFCOUNT> gLipiRefCount;
LTKLipiEngineModule* lipiEngineModule = LTKLipiEngineModule::getInstance();

/******************************************************************************
* AUTHOR        : Thanigai 
* DATE          : 29-JUL-2005
* NAME          : DllMain
* DESCRIPTION   : 
* ARGUMENTS     : 
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
#ifdef _WIN32
BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call,
					  LPVOID lpReserved)
{
    switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			break;
		case DLL_THREAD_ATTACH:
			break;

		case DLL_THREAD_DETACH:
			break;
		case DLL_PROCESS_DETACH:
			unloadAllModules();
			break;
    }
    return TRUE;
}
#endif // #ifdef _WIN32
/******************************************************************************
* AUTHOR        : Thanigai 
* DATE          : 29-JUL-2005
* NAME          : createLTKLipiEngine
* DESCRIPTION   : 
* ARGUMENTS     : 
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
LTKLipiEngineInterface* createLTKLipiEngine()
{
	return lipiEngineModule;
}

/******************************************************************************
* AUTHOR        : Nidhi Sharma 
* DATE          : 2-JUL-2008
* NAME          : deleteLTKLipiEngine
* DESCRIPTION   : 
* ARGUMENTS     : 
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
void deleteLTKLipiEngine()
{
	LTKLipiEngineModule::destroyLipiEngineInstance();
    lipiEngineModule = NULL;
}
/******************************************************************************
* AUTHOR        : Thanigai Murugan K 
* DATE          : 23-SEP-2005
* NAME          : addModule
* DESCRIPTION   : For each module that is getting loaded, vector lipiRefCount 
*				  will have an entry. For each recognizer object that is getting 
*				  created under every module, vector "vecRecoHandles" will have
*				  an entry. 
*				  If the recognizer object belongs to the existing module handle, 
*				  then only the reference count incremented and the recognizer 
*				  handle is added to the module's "vecRecoHandles" vector.
* ARGUMENTS     : RecoHandle - handle to the recognizer object
*				  handle - handle to the module
* RETURNS       : 0 on success, -1 on Failure.
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
void addModule(void* RecoHandle, void* handle)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Entering: addModule()"<<endl;

	int iModIndex = -1;

	iModIndex = findIndexIfModuleInMemory(handle);
    
	if(iModIndex == EMODULE_NOT_IN_MEMORY) // New module
	{
		MODULEREFCOUNT mModule;

		mModule.iRefCount = 1;
		mModule.modHandle = handle;
		mModule.vecRecoHandles.push_back(RecoHandle);
		gLipiRefCount.push_back(mModule);
	}
	else // Module in memory, increment the count
	{
		gLipiRefCount[iModIndex].iRefCount++;
		gLipiRefCount[iModIndex].vecRecoHandles.push_back(RecoHandle);
	}

	for(int i = 0; i < gLipiRefCount.size(); i++)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Details of module :"<< i 
			<< "[" << gLipiRefCount[i].modHandle << "]" 
			<< "No.of.recognizers in this module = " 
			<< gLipiRefCount[i].vecRecoHandles.size() << endl;
	}


	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: addModule()"<<endl;
}

/******************************************************************************
* AUTHOR        : Thanigai Murugan K 
* DATE          : 23-SEP-2005
* NAME          : deleteModule
* DESCRIPTION   : Given a recognizer handle, this function look at the module to 
*				  which this handle belongs to and decrement the reference count
*				  for that module. Also this recognizer object is deleted from 
*				  the module's recognizer object vectors.
* ARGUMENTS     : RecoHandle - handle to the recognizer object
*				  handle - handle to the module
* RETURNS       : 0 on success, -1 on Failure.
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
int deleteModule(void* RecoHandle)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering: deleteModule()"<<endl;

	int iModIndex = -1;

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
		"No.of.modules = "<< gLipiRefCount.size()<<endl;	

	for(int i = 0; i < gLipiRefCount.size(); i++)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Details of module :"<< i 
			<< "[" << gLipiRefCount[i].modHandle << "]" 
			<< "No.of.recognizers in this module = " 
			<< gLipiRefCount[i].vecRecoHandles.size() << endl;
	}

	iModIndex = getAlgoModuleIndex(RecoHandle);
    
	if(iModIndex == EMODULE_NOT_IN_MEMORY) 
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR)
        <<"Error: " << getErrorMessage(EMODULE_NOT_IN_MEMORY) <<
        " deleteModule"<<endl;
        
		return EMODULE_NOT_IN_MEMORY;
	}
	else // Module in memory, decrement the count
	{
		vector<void*>::iterator RecoIter;
		
		for(RecoIter = gLipiRefCount[iModIndex].vecRecoHandles.begin();
			RecoIter < gLipiRefCount[iModIndex].vecRecoHandles.end(); RecoIter++)
		{
			if((*RecoIter) == RecoHandle)
			{
				gLipiRefCount[iModIndex].vecRecoHandles.erase(RecoIter);	
				break;
			}
		}

		if(gLipiRefCount[iModIndex].iRefCount > 1)
		{
			gLipiRefCount[iModIndex].iRefCount--;
		}
	}

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: deleteModule()"<<endl;
	return SUCCESS;
}

/******************************************************************************
* AUTHOR        : Thanigai Murugan K 
* DATE          : 23-SEP-2005
* NAME          : findIndexIfModuleInMemory
* DESCRIPTION   : Looks into gLipiRefCount vector for handle, if exists returns 
*				  its index or returns -1
* ARGUMENTS     : handle - handle to the module
* RETURNS       : 0 on success, -1 on Failure.
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
int findIndexIfModuleInMemory(void* handle) 
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
		"Entering: findIndexIfModuleInMemory()"<<endl;

	for(int i = 0; i < gLipiRefCount.size(); i++)
	{
		if(handle == gLipiRefCount[i].modHandle)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
			"Exiting: findIndexIfModuleInMemory()"<<endl;
	
			return i;
		}
	}
	
	return EMODULE_NOT_IN_MEMORY;
}

/******************************************************************************
* AUTHOR        : Thanigai Murugan K 
* DATE          : 23-SEP-2005
* NAME          : unloadAllModules
* DESCRIPTION   : Unload all the modules that are loaded into memory
* ARGUMENTS     : None
* RETURNS       : 0 on success, -1 on Failure.
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
int unloadAllModules()
{
    LTKOSUtil* utilPtr = LTKOSUtilFactory::getInstance();
    
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering: unloadAllModules()"<<endl;

	for(int i = 0; i < gLipiRefCount.size(); i++)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
			"Unloading module, handle = " << gLipiRefCount[i].modHandle << endl;

		if(gLipiRefCount[i].vecRecoHandles.size() > 1)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"WARNING: Not all "<<
				gLipiRefCount[i].vecRecoHandles.size()<<
				"recognizers are deleted"<< endl;
		}

		utilPtr->unloadSharedLib(gLipiRefCount[i].modHandle);
	}

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<"Exiting: unloadAllModules()"<<endl;

    delete utilPtr;
	return 0;
}

/******************************************************************************
* AUTHOR        : Thanigai Murugan K 
* DATE          : 23-SEP-2005
* NAME          : getAlgoModuleIndex
* DESCRIPTION   : returns index in the modules vector for the recoHandle passed
* ARGUMENTS     : RecoHandle - handle to the recognizer object
* RETURNS       : 0 on success, -1 on Failure.
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
int getAlgoModuleIndex(void* RecoHandle) 
{
    
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
		"Entering: getAlgoModuleIndex()"<<endl;

	for(int i = 0; i < gLipiRefCount.size(); i++)
	{
		for (int j = 0; j < gLipiRefCount[i].vecRecoHandles.size(); j++)
		{
			if( gLipiRefCount[i].vecRecoHandles[j] == RecoHandle)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
					"index : "<< i <<endl;
				
				LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
						"Exiting: getAlgoModuleIndex()"<<endl;
				
				return i;
			}
		}
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
    	"Error: "<<EMODULE_NOT_IN_MEMORY<<": "<<
        getErrorMessage(EMODULE_NOT_IN_MEMORY) <<
    	"getAlgoModuleIndex()"<<endl;

	return EMODULE_NOT_IN_MEMORY;
}

/******************************************************************************
* AUTHOR        : 
* DATE          : 
* NAME          : 
* DESCRIPTION   : 
* ARGUMENTS     : 
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
void setLipiRootPath(const string& appLipiPath)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering: setLipiRootPath()"<<endl;
	
	lipiEngineModule->setLipiRootPath(appLipiPath);

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting: setLipiRootPath()"<<endl;
}

/******************************************************************************
* AUTHOR        : 
* DATE          : 
* NAME          : 
* DESCRIPTION   : 
* ARGUMENTS     : 
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
void setLTKLogFileName(const string& appLogFile)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
					"Entering: setLTKLogFileName()"<<endl;
	
	lipiEngineModule->setLipiLogFileName(appLogFile);

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
					"Exiting: setLTKLogFileName()"<<endl;
}

/******************************************************************************
* AUTHOR        : 
* DATE          : 
* NAME          : 
* DESCRIPTION   : 
* ARGUMENTS     : 
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
void setLTKLogLevel(const string& appLogLevel)
{
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering: setLTKLogLevel()"<<endl;
	
	lipiEngineModule->setLipiLogLevel(appLogLevel);

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting: setLTKLogLevel()"<<endl;
}

/******************************************************************************
* AUTHOR        : 
* DATE          : 
* NAME          : 
* DESCRIPTION   : 
* ARGUMENTS     : 
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
******************************************************************************/
void getToolkitVersion(int& iMajor, int& iMinor, int& iBugFix)
{
    
	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering: getToolkitVersion()"<<endl;
	
	getVersion(iMajor, iMinor, iBugFix);

	LOG(LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting: getToolkitVersion()"<<endl;
}
