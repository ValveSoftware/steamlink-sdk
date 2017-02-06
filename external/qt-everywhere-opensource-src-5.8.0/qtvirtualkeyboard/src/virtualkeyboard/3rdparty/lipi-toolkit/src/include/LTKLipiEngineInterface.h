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
 * $LastChangedDate: 2011-01-18 15:41:43 +0530 (Tue, 18 Jan 2011) $
 * $Revision: 829 $
 * $Author: mnab $
 *
 ************************************************************************/

/************************************************************************
 * FILE DESCR: Interface definition of LipiEngine interface
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

#ifndef __LIPIENGINEINTERFACE_H_
#define __LIPIENGINEINTERFACE_H_

#include "LTKShapeRecognizer.h"
#include "LTKWordRecognizer.h"
#include "LTKRecognitionContext.h"

class LTKLipiEngineInterface{
public:


    virtual void setLipiRootPath(const string& appLipiPath)=0;

    virtual void setLipiLibPath(const string& appLipiLibPath)=0;

    virtual int setLipiLogFileName(const string& appLipiPath) = 0;

    virtual int setLipiLogLevel(const string& appLogLevel)=0;

	/* To initialize the lipiengine */
	virtual int initializeLipiEngine() = 0;

	/* use this to create shape recognizer object, by passing the logical project name */
	virtual int createShapeRecognizer(string &strLogicalProjectName, LTKShapeRecognizer** outShapeRecognizerPtr) = 0;
	
	/* use this to create shape recognizer object, by passing the project name and profile name*/   
	virtual int createShapeRecognizer(const string& strProjectName,  
                                          const string& strProfileName,
                                           LTKShapeRecognizer** outShapeRecognizerPtr) = 0;
    
	/* use this to delete the shape recognizer object created using createShapeRecognizer call */
	virtual int deleteShapeRecognizer(LTKShapeRecognizer*) = 0;

	/* use this to create word recognizer object, by passing the project name and profile name*/
    virtual int createWordRecognizer(const string& strProjectName, 
                                          const string& strProfileName,
                                          LTKWordRecognizer** outWordRecPtr) = 0;
    
	/* use this to create word recognizer object, by passing the logical project name*/
	virtual int createWordRecognizer(const string& strlogicalProjectName, 
                                  LTKWordRecognizer** outWordRecPtr) = 0;

	/* use this to delete the word recognizer object created using createShapeRecognizer call */
	virtual int deleteWordRecognizer(LTKWordRecognizer*) = 0;
   
};

#endif //#ifndef __LIPIENGINEINTERFACE_H_


