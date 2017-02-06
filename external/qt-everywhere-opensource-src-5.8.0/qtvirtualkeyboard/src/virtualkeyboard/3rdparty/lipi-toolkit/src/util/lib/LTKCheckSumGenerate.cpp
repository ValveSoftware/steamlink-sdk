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
 * FILE DESCR: Definitions of Checksum generate module
 *
 * CONTENTS:   
 *		LTKCheckSumGenerate
 *		initCRC32Table
 *		reflect
 *		getCRC
 *		addHeaderInfo
 *		readMDTHeader
 *
 * AUTHOR:  Vijayakumara M
 *
 * DATE:       Aug 02, 2005
 * CHANGE HISTORY:
 * Author		Date			Description
 ************************************************************************/

/************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2011-02-08 16:57:52 +0530 (Tue, 08 Feb 2011) $
 * $Revision: 834 $
 * $Author: mnab $
 *
 ************************************************************************/
 
#include "LTKCheckSumGenerate.h"
#include "LTKMacros.h"
#include "LTKLoggerUtil.h"
#include "LTKConfigFileReader.h"
#include "LTKException.h"

#include "LTKOSUtil.h"

#include "LTKOSUtilFactory.h"

/*****************************************************************************
* AUTHOR		: Vijayakumara M
* DATE			: 26 July 2005
* NAME			: LTKCheckSumGenerate
* DESCRIPTION	: Constractor.
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*****************************************************************************/
LTKCheckSumGenerate::LTKCheckSumGenerate():
m_OSUtilPtr(LTKOSUtilFactory::getInstance())
{
	initCRC32Table();
}

/*****************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 26 June 2008
* NAME			: ~LTKCheckSumGenerate
* DESCRIPTION	: Desctructor.
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*****************************************************************************/
LTKCheckSumGenerate::~LTKCheckSumGenerate()
{
    delete m_OSUtilPtr;
}

/****************************************************************************
* AUTHOR		: Vijayakumara M
* DATE			: 26 July 2005
* NAME			: initCRC32Table
* DESCRIPTION	: Call this function only once to initialize the CRC table.
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
****************************************************************************/
void LTKCheckSumGenerate::initCRC32Table()
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Entering: LTKCheckSumGenerate::initCRC32Table()" << endl;

	unsigned int ulPolynomial = 0x04c11db7;

	// 256 values representing ASCII character codes.
	for(int i = 0; i <= 0xFF; i++)
	{
		m_CRC32Table[i]=reflect(i, 8) << 24;
		for (int j = 0; j < 8; j++)
			m_CRC32Table[i] = (m_CRC32Table[i] << 1) ^ (m_CRC32Table[i] & (1 << 31) ? ulPolynomial : 0);
		m_CRC32Table[i] = reflect(m_CRC32Table[i], 32);
	}
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKCheckSumGenerate::initCRC32Table()" << endl;
}

/**********************************************************************************
* AUTHOR		: Vijayakumara M
* DATE			: 26 July 2005
* NAME			: reflect
* DESCRIPTION	: reflection is a requirement for the official CRC-32 standard.
*		  		  we can create CRCs without it, but they won't conform to the standard.
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
unsigned int LTKCheckSumGenerate::reflect(unsigned int ref, char ch)
{// Used only by initCRC32Table()

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Entering: LTKCheckSumGenerate::reflect()" << endl;

	unsigned int value(0);

	// Swap bit 0 for bit 7
	// bit 1 for bit 6, etc.
	for(int i = 1; i < (ch + 1); i++)
	{
		if(ref & 1)
			value |= 1 << (ch - i);
		ref >>= 1;
	}
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        " Exiting: LTKCheckSumGenerate::reflect()" << endl;
	return value;
}

/**********************************************************************************
* AUTHOR		: Vijayakumara M
* DATE			: 26 July 2005
* NAME			: getCRC
* DESCRIPTION	: Function to generate checkSum.
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
* Balaji MNA    11th Nov, 2008     Changed char* parameter to string&
*************************************************************************************/
int LTKCheckSumGenerate::getCRC(string& text) 
{
	  // Pass a text string to this function and it will return the CRC. 

      // Once the lookup table has been filled in by the two functions above, 
      // this function creates all CRCs using only the lookup table. 

      // Be sure to use unsigned variables, 
      // because negative values introduce high bits 
      // where zero bits are required. 

      // Start out with all bits set high. 
  	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Entering: LTKCheckSumGenerate::getCRC()" << endl;
      unsigned int  ulCRC(0xffffffff); 

      // Get the length. 
      int len = text.size(); 

	  // Save the text in the buffer. 
      unsigned char* buffer = (unsigned char*)text.c_str(); 

	  // Perform the algorithm on each character 
      // in the string, using the lookup table values. 
      while(len--)
	  { 
          ulCRC = (ulCRC >> 8) ^ m_CRC32Table[(ulCRC & 0xFF) ^ *buffer++]; 
      }

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Exiting: LTKCheckSumGenerate::getCRC()" << endl;

	  // Exclusive OR the result with the beginning value. 
      return ulCRC ^ 0xffffffff; 
} 

/**********************************************************************************
* AUTHOR		: Srinivasa Vithal
* DATE			: 21 November 2007
* NAME			: addHeaderInfo
* DESCRIPTION	: This function adds the Header information to the Model Data file.
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
int LTKCheckSumGenerate::addHeaderInfo(const string& modelDataHeaderInfoFilePath, 
                                       const string& mdtFilePath, 
                                       const stringStringMap& headerInfo)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Entering: LTKCheckSumGenerate::addHeaderInfo()" << endl;

	int nCRC;								//	Holds checks sum in decimal format

	long testEndian = 1;

	char *modelFileData = NULL;		//	Model File header Data.

	//unsigned int hdLen[CKSUM_HDR_STR_LEN], offsetLen[CKSUM_HDR_STR_LEN];
	char chSum[CKSUM_HDR_STR_LEN];

	long modelFileInfoSize ;

	string comment, heder;						//Header comment,and the header data
	
	stringVector strTokens;	
	
	ostringstream strHeaderContents1;
	ostringstream strHeaderContents2;
	
	string::size_type indx=0;					

	// Add the mandatory fields to the header Info
	stringStringMap tempHeaderInfo = updateHeaderWithMandatoryFields(headerInfo);

	ostringstream headInfo, newHeadInfo;
	

	ifstream readFile(mdtFilePath.c_str(), ios::in | ios::binary);

	if(!readFile )
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error : "<< EMODEL_DATA_FILE_OPEN 
		    <<": "<< "Error opening mdt file "<< mdtFilePath <<
			" LTKCheckSumGenerate::addHeaderInfo()"<<endl;
		
		LTKReturnError(EMODEL_DATA_FILE_OPEN);
	}
	
	// get fle size
	readFile.seekg(0, ios::beg);
	readFile.seekg(0, ios::end);
	modelFileInfoSize = readFile.tellg();
	readFile.seekg(0, ios::beg);
	
	try
	{
		if(!modelDataHeaderInfoFilePath.empty())
		{
			LTKConfigFileReader inputHeaderFileReader(modelDataHeaderInfoFilePath);		
			const stringStringMap& tempCfgFileMap = inputHeaderFileReader.getCfgFileMap();

			stringStringMap::const_iterator tempCfgFileIter = tempCfgFileMap.begin();
			stringStringMap::const_iterator tempCfgFileIterEnd = tempCfgFileMap.end();

			for(; tempCfgFileIter != tempCfgFileIterEnd ; ++tempCfgFileIter)
			{
				if(tempHeaderInfo.find(tempCfgFileIter->first) == tempHeaderInfo.end())
				{

					//inserting user-defined key-value pair
					tempHeaderInfo[tempCfgFileIter->first] = tempCfgFileIter->second;
				}	
			}
		}

	//Read Model Data File.
	modelFileData = new char[modelFileInfoSize+1];

	memset(modelFileData, '\0', modelFileInfoSize+1);

	readFile.read(modelFileData, modelFileInfoSize+1);

	readFile.close();

	string szBuf(modelFileData);   

	//Caluculate Checksum for the Modiel File Data.
	nCRC = getCRC(szBuf);

	//Convert the check sum into Hexadecimal Value.
	sprintf(chSum, "%x", nCRC);

	tempHeaderInfo[CKS] = chSum;





	ofstream writeFile(mdtFilePath.c_str(),ios::out|ios::binary); 

	stringStringMap::const_iterator tempHeadInfoIter = tempHeaderInfo.begin();
	stringStringMap::const_iterator tempHeadInfoIterEnd = tempHeaderInfo.end();
	for(; tempHeadInfoIter != tempHeadInfoIterEnd ; ++tempHeadInfoIter)
	{
		if((tempHeadInfoIter->first!=CKS) && (tempHeadInfoIter->first!=HEADERLEN)
			&& (tempHeadInfoIter->first!=DATAOFFSET))
		{
			strHeaderContents2 <<"<"<<tempHeadInfoIter->first
			<<"="<<tempHeadInfoIter->second<<">";
		}
		
	}

	strHeaderContents1<<"<"<<CKS<<"="<<chSum<<">"<<"<"<<HEADERLEN<<"=";


		
	string initialStrHeader = strHeaderContents1.str() + strHeaderContents2.str();

	char strHeaderLength[CKSUM_HDR_STR_LEN], strOffsetLength[CKSUM_HDR_STR_LEN];

	//Get the Length of the header.( 15 is for length of ><DATAOFFSET=> ).
	sprintf(strHeaderLength, "%d", initialStrHeader.length()+14);

	sprintf(strOffsetLength, "%d", initialStrHeader.length()+15);


	//Add the length of the pre Header length and 1 for the last ">" char.
	sprintf(strHeaderLength, "%d", initialStrHeader.length()+strlen(strHeaderLength)+strlen(strOffsetLength)+14);

	sprintf(strOffsetLength, "%d", initialStrHeader.length()+strlen(strHeaderLength)+strlen(strOffsetLength)+15);

	strHeaderContents1<<strHeaderLength<<">";

	strHeaderContents1<<"<"<<DATAOFFSET<<"="<<strOffsetLength<<">";

	writeFile<<strHeaderContents1.str();
	writeFile<<strHeaderContents2.str();

	writeFile.write(modelFileData, modelFileInfoSize);
	
	writeFile.close();

		if( modelFileData != NULL)
		{
			delete [] modelFileData;
			modelFileData = NULL;
		}


	}
	catch (LTKException e)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error : "<<EFILE_OPEN_ERROR<<":"<< e.getExceptionMessage()<<
			"LTKCheckSumGenerate::addHeaderInfo()"<<endl;
		LTKReturnError(EFILE_OPEN_ERROR);
	}
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Exiting: LTKCheckSumGenerate::addHeaderInfo()" << endl;

	return SUCCESS;
}

stringStringMap LTKCheckSumGenerate::updateHeaderWithMandatoryFields(const stringStringMap& headerInfo)
{

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Entering: LTKCheckSumGenerate::updateHeaderWithMandatoryFields()" << endl;

	long testEndian = 1;
	string comment="";
	int commentLen=0;

	stringStringMap tempHeaderInfo = headerInfo;

	if(tempHeaderInfo.find(COMMENT)!=tempHeaderInfo.end())
	{
		
		commentLen=tempHeaderInfo[COMMENT].length();
	}


	// Pushing keys to the tempHeaderInfo
	ostringstream tempString;
    string platformInfoString = "";

    string timeString ;
	m_OSUtilPtr->getSystemTimeString(timeString);
    tempString << timeString;
    
	tempHeaderInfo["CKS"] = "";
	tempHeaderInfo["HEADERLEN"] = "";
	tempHeaderInfo["DATAOFFSET"] = "";
	tempHeaderInfo["CREATETIME"] = tempString.str();
	tempHeaderInfo["MODTIME"] = tempString.str();

    // get Platform Name
    m_OSUtilPtr->getPlatformName(platformInfoString);
    tempHeaderInfo["PLATFORM"] = platformInfoString;

    //get processor architechure
    platformInfoString = "";
    m_OSUtilPtr->getProcessorArchitechure(platformInfoString);
    tempHeaderInfo["PROCESSOR_ARCHITEC"] = platformInfoString;

    // get OS info
    platformInfoString = "";
    m_OSUtilPtr->getOSInfo(platformInfoString);
	tempHeaderInfo["OSVERSION"] = platformInfoString;
	tempHeaderInfo["HEADERVER"] = HEADERVERSION;

    tempString.str("");
    tempString << commentLen;
	tempHeaderInfo["COMMENTLEN"] = tempString.str();

    tempString.str("");
    tempString << sizeof(int);
	tempHeaderInfo["SIZEOFINT"] = tempString.str();

    tempString.str("");
    tempString << sizeof(unsigned int);
	tempHeaderInfo["SIZEOFUINT"] = tempString.str();

    tempString.str("");
    tempString << sizeof(short int);
	tempHeaderInfo["SIZEOFSHORTINT"] = tempString.str();

    tempString.str("");
    tempString << sizeof(float);
	tempHeaderInfo["SIZEOFFLOAT"] = tempString.str();

    tempString.str("");
    tempString << sizeof(char);
	tempHeaderInfo["SIZEOFCHAR"] = tempString.str();

   // checking for indian ness
	if(!(*((char *)(&testEndian))))
	{
		tempHeaderInfo["BYTEORDER"] = "BE";
	}
	else
	{
		tempHeaderInfo["BYTEORDER"] = "LE";
	}
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Exiting: LTKCheckSumGenerate::updateHeaderWithMandatoryFields()" << endl;
	
	return tempHeaderInfo;
}

/****************************************************************************
* AUTHOR		: Vijayakumara M
* DATE			: 26 July 2005
* NAME			: readMDTHeader
* DESCRIPTION	: This function is used to check for checking the file integriry.
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*****************************************************************************/
int LTKCheckSumGenerate::readMDTHeader(const string &mdtFilePath,
                                             stringStringMap &headerSequence)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Entering: LTKCheckSumGenerate::readMDTHeader()" << endl;

	int headerLen, nCRC;
	
	long dSize, actDataSize;
	
	char chSum[CKSUM_HDR_STR_LEN], *sbuf,*headerData, headerInfo[51];

	stringVector strTokens;

	ifstream mdtFileHandle(mdtFilePath.c_str(), ios::in | ios::binary);	

	if(!mdtFileHandle)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR)  
			<<"Error : "<< EMODEL_DATA_FILE_OPEN <<":"<< getErrorMessage(EMODEL_DATA_FILE_OPEN)
            <<"LTKCheckSumGenerate::readMDTHeader()" <<endl;

		LTKReturnError(EMODEL_DATA_FILE_OPEN);
	}	


	mdtFileHandle.read(headerInfo, 50);

	char *ptr = strstr(headerInfo, HEADERLEN);
	if(ptr == NULL)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) 
			<<"Error : "<< EMODEL_DATA_FILE_FORMAT <<":"<< getErrorMessage(EMODEL_DATA_FILE_FORMAT)
             <<"LTKCheckSumGenerate::readMDTHeader()" <<endl;

		LTKReturnError(EMODEL_DATA_FILE_FORMAT);
	}

	strtok(ptr, "=");

	char *headerLenPtr = strtok( NULL , ">" );
	
	if(headerLenPtr == NULL)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error : "<< EMODEL_DATA_FILE_FORMAT <<":"<< getErrorMessage(EMODEL_DATA_FILE_FORMAT)
			<<"LTKCheckSumGenerate::readMDTHeader()"<<endl;

		LTKReturnError(EMODEL_DATA_FILE_FORMAT);
	}

	headerLen = atoi(headerLenPtr);

	mdtFileHandle.seekg(0, ios::beg);

	headerData = new char [headerLen+1];

	memset(headerData, '\0', headerLen+1);

	mdtFileHandle.read(headerData, headerLen);	

	LTKStringUtil::tokenizeString(headerData,  TOKENIZE_DELIMITER,  strTokens);

	int strTokensSize = strTokens.size();

	for(int indx=0; indx+1 <strTokensSize ;indx=indx+2)
	{
		headerSequence[strTokens.at(indx)] = strTokens.at(indx+1);	
	}

	// get the file size in bytes
	mdtFileHandle.seekg(0,ios::beg);
	mdtFileHandle.seekg(0, ios::end);
	dSize = mdtFileHandle.tellg();

	//Size fo the actual data excluding Header size.
	actDataSize=dSize-headerLen+1;

	string cks = headerSequence[CKS];

	//Allocate memory to read the actual data.
	sbuf = new char[actDataSize];
	memset(sbuf, '\0', actDataSize);
	
	//Read the file.
	mdtFileHandle.seekg(headerLen, ios::beg);
	mdtFileHandle.read(sbuf, actDataSize);

	//Close the file.	
	mdtFileHandle.close();

	string szBuf(sbuf);

	//Caluculate Checksum for the Model File Data.
	nCRC = getCRC(szBuf);
	sprintf(chSum, "%x", nCRC);

	delete [] sbuf;
	delete [] headerData;

	if(strcmp(cks.c_str(), chSum) != 0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error : "<< EINVALID_INPUT_FORMAT <<":"<< getErrorMessage(EINVALID_INPUT_FORMAT)
			<<"LTKCheckSumGenerate::readMDTHeade()r"<<endl;
		LTKReturnError(EINVALID_INPUT_FORMAT);
	}
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Exiting: LTKCheckSumGenerate::readMDTHeader()" << endl;

	return SUCCESS;
}