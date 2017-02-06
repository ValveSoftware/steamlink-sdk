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
******************************************************************************/

/******************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2008-07-18 15:00:39 +0530 (Fri, 18 Jul 2008) $
 * $Revision: 561 $
 * $Author: sharmnid $
 *
 *****************************************************************************/
/*****************************************************************************
 * FILE DESCR: DLL header for LipiEngine DLL module
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

#ifndef __LIPIENGINE_H_
#define __LIPIENGINE_H_

#include "LipiEngineModule.h"

/* The following ifdef block is the standard way of creating macros which make
   exporting from a DLL simpler. All files within this DLL are compiled with 
   the LIPIENGINE_EXPORTS symbol defined on the command line.This symbol should 
   not be defined on any project that uses this DLL. This way any other project 
   whose source files include this file see LIPIENGINE_API functions as being 
   imported from a DLL, wheras this DLL sees symbols defined with this macro 
   as being exported.
*/

#ifdef _WIN32
#ifdef LIPIENGINE_EXPORTS
#define LIPIENGINE_API __declspec(dllexport)
#else
#define LIPIENGINE_API __declspec(dllimport)
#endif
#else
#define LIPIENGINE_API
#endif // #ifdef _WIN32

extern "C" LIPIENGINE_API LTKLipiEngineInterface* createLTKLipiEngine();

extern "C" LIPIENGINE_API void setLipiRootPath(const string& appLipiPath);

extern "C" LIPIENGINE_API void setLTKLogFileName(const string& appLogFile);

extern "C" LIPIENGINE_API void setLTKLogLevel(const string& appLogLevel);

extern "C" LIPIENGINE_API void getToolkitVersion(int& iMajor, int& iMinor, 
												 int& iBugFix);

extern "C" LIPIENGINE_API void deleteLTKLipiEngine();

/**
* This function Unload all the modules that are loaded into memory
* @param none
* @return 0 on success
*/
int unloadAllModules();

/**
* This function Looks into m_lipiRefCount vector for handle, if exists  
* returns its index or returns -1
* @param handle to the module
* @return 0 on success
*/
int findIndexIfModuleInMemory(void* handle);

/**
* Given a recognizer handle, this function look at the module to 
* which this handle belongs to and decrement the reference count
* for that module. Also this recognizer object is deleted from 
* the module's recognizer object vectors.
* @param recoHandle - handle to the recognizer object to delete
* @return 0 on success
*/
int deleteModule(void* RecoHandle);

/**
* For each module that is getting loaded, vector m_lipiRefCount 
* will have an entry. For each recognizer object that is getting 
* created under every module, vector "vecRecoHandles" will have
* an entry. 
* If the recognizer object belongs to the existing module handle, 
* then only the reference count incremented and the recognizer handle
* is added to the module's "vecRecoHandles" vector.
* @param Handle - handle to the module, 
*        RecoHandle - handle to the recognizer object
*/
void addModule(void* RecoHandle, void* handle);

/**
* This function returns the index in the modules vector for the 
  recoHandle passed
* @param RecoHandle - handle to the recognizer object
* @return 0 on success
*/
int getAlgoModuleIndex(void* RecoHandle);


#endif //#ifndef __LIPIENGINE_H_
