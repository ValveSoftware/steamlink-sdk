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
 * FILE DESCR: Definitions for the String Splitter Module
 *
 * CONTENTS: 
 *
 * AUTHOR:     Vijayakumara M.
 *
 * DATE:       July 25, 2005
 * CHANGE HISTORY:
 * Author		Date			Description
 ************************************************************************/
/************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2008-11-19 17:57:12 +0530 (Wed, 19 Nov 2008) $
 * $Revision: 704 $
 * $Author: mnab $
 *
 ************************************************************************/
#ifndef __LTKCHECSUMGENERATE_H
#define __LTKCHECSUMGENERATE_H

#include <sstream>

#include "LTKStringUtil.h"

#include "LTKTypes.h"

#include "LTKMacros.h"

#include "LTKErrors.h"

#include "LTKErrorsList.h"

#ifdef _WIN32
#include <windows.h>
const int BUFSIZE = 256;
#else
#include <stdio.h>
#include <sys/utsname.h>
#endif

class LTKOSUtil;

class LTKCheckSumGenerate
{
private:

	unsigned int m_CRC32Table[256]; // Lookup table array.

    LTKOSUtil* m_OSUtilPtr;

public:

	// @{

	/**
	 * Default constractor, calls initCRC32Table function for intialization.
	 */

   	LTKCheckSumGenerate();

    /**
    * Destructor
    */

    ~LTKCheckSumGenerate();

	
	/**
	 * This method generates the checksum.
	 *
	 * @param text, for which the checksum to be calculated.
	 *
	 * @return checksum for the text.
	 */
	int getCRC(string& text);

	/**
	 * This method reads the header information from the model data header information file and
	 * adds it to the data file with additional header information.
	 *
	 * @param strModelDataHeaderInfoFile, Model data header information file name.
	 * @param referenceModelFile, Model data file name.
	 * @param verson information.
	 * @algoName Name of the alogorithm.
	 *
	 * @return SUCCESS on successful addition of the header to the data file. of the respective 
	 *			errorCode on the occurence of any errors.
	 */

	int addHeaderInfo(const string& modelDataHeaderInfoFilePath, 
					       const string& mdtFilePath, 
						const stringStringMap& headerInfo);

	/**
	 * This method reads the header information from the header information file and
	 * adds it to the data file with additional header information.
	 *
	 * @param headerInfo, Model data header information file name.
	 * @param referenceModelFile, Model data file name.
	 *
	 * @return the new stringStringMap after updation
	 */
	stringStringMap updateHeaderWithMandatoryFields(const stringStringMap& headerInfo);


	/**
	 * This method is used to check the file integrity with the checksum.
	 *
	 * @param mdtFilePath, Model data file Name.
	 * @param string - string map variable, which holds the header keys and the values respectively.
	 *
	 * @return SUCCESS, 
	 */

	int readMDTHeader(const string &mdtFilePath,
	                        stringStringMap &headerSequence);	

private:

    /**
	 * This method intializes the crc32_table with 256 values representing ASCII character 
	 * codes.
	 */

   	void initCRC32Table();

    /**
	 * This method is used only by initCRC32Table() function. reflection is a requirement
	 * for the official CRC-32 standard.
	 *
	 * @param ref value.
	 * @param character.
	 *
	 * @return long integer value which will be used by initCRC32Table functon.
	 */

	unsigned int reflect(unsigned int ref, char ch);


	// @}
};

#endif	//__LTKCHECSUMGENERATE_H

