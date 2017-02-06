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
 * $LastChangedDate: 2008-08-01 09:48:58 +0530 (Fri, 01 Aug 2008) $
 * $Revision: 583 $
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

#ifndef LTKVERSIONCOMPATIBILITYCHECK_H
#define LTKVERSIONCOMPATIBILITYCHECK_H

#include "LTKInc.h"

/**
* @ingroup util
*/

enum ELTKVersionComparison
{
	GREATER,
	LESSER,
	EQUAL
};

class LTKVersionCompatibilityCheck 
{
private:
	string m_delimiter;
	
public:

	/**
	* @name Constructors and Destructor
	*/

	// @{

	/**
	 * Default Constructor
	 */
	LTKVersionCompatibilityCheck();


	/**
	* Destructor
	*/

	~LTKVersionCompatibilityCheck();

	// @}

	/**
	 * @name Methods
	 */

	// @{

	bool checkCompatibility(const string& supportedMinVersion,const string& currentVersion, const string& versionRead);	

	bool isFirstVersionHigher(const string& firstVersion, const string& secondVersion);	

private:

       
	ELTKVersionComparison compareTokens(int firstTokenIntegerValue, 
                                            int secondTokenIntegerValue);

	// @}
};

#endif


