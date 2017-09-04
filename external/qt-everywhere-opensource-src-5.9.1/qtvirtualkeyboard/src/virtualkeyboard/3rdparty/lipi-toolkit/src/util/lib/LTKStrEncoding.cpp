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
 * $LastChangedDate: 2011-02-08 11:00:11 +0530 (Tue, 08 Feb 2011) $
 * $Revision: 832 $
 * $Author: dineshm $
 *
 ************************************************************************/
 
#include "LTKStrEncoding.h"
#include "LTKMacros.h"
#include "LTKLoggerUtil.h"

/************************************************************************
 * FILE DESCR: Implementation of LTKInkUtils that computes the statistics
 *             of a trace group
 *
 * CONTENTS: 
 *   shapeStrToUnicode 
 *   tamilShapeStrToUnicode
 *
 * AUTHOR:     Deepu V.
 *
 * DATE:       September 8, 2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/


/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 08-SEP-2005
* NAME			: LTKStrEncoding
* DESCRIPTION	: Initialization constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKStrEncoding::LTKStrEncoding()
{

}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 08-SEP-2005
* NAME			: shapeStrToUnicode 
* DESCRIPTION	: Do the mapping from shaperecognition ID to Unicode string
* ARGUMENTS		: shapeRecProjectName - Shape Recognition Project name
*               : shapeIDs - The shape recognizer output IDs
*               : unicodeString - output unicode string 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKStrEncoding::shapeStrToUnicode(const string shapeRecProjectName, const vector<unsigned short>&shapeIDs, vector<unsigned short>& unicodeString)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
	  " Entering: LTKStrEncoding::shapeStrToUnicode()" << endl;


	/*if(shapeRecProjectName.empty())
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EMAP_NOT_FOUND <<":"<< getErrorMessage(EMAP_NOT_FOUND)
                  <<"LTKStrEncoding::shapeStrToUnicode()" <<endl;

		LTKReturnError(EMAP_NOT_FOUND);
	}*/

	if(shapeRecProjectName == "tamil_iso_char")
	{
		return (tamilShapeStrToUnicode(shapeIDs,unicodeString));
	}
	else
	{
		return (numShapeStrToUnicode(shapeIDs,unicodeString));
	}
	
	//uncomment following if you need to implement mappings for 
	//additional shape rec projects
	//else if (shapeRecProjectName == <YOUR PROJECT NAME>)
	//{
	//  //Implement the logic here
	//}
	

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
	  " Exiting: LTKStrEncoding::shapeStrToUnicode()" << endl;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 08-SEP-2005
* NAME			: numShapeStrToUnicode 
* DESCRIPTION	: Do the mapping from shaperecognition ID 
*               : to Unicode string for numerals project
* ARGUMENTS		: shapeIDs - The shape recognizer output IDs
*               : unicodeString - output unicode string 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKStrEncoding::numShapeStrToUnicode(const vector<unsigned short>& shapeIDs, vector<unsigned short>& unicodeString)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
	  " Entering: LTKStrEncoding::numShapeStrToUnicode()" << endl;

	vector<unsigned short>::const_iterator shapeIDsIter, shapeIDsEnd;

	//iterating through the shape IDs
	shapeIDsEnd = shapeIDs.end();
	for(shapeIDsIter = shapeIDs.begin();shapeIDsIter != shapeIDsEnd;++shapeIDsIter)
	{
		if(*shapeIDsIter == SHRT_MAX )
			unicodeString.push_back(L' ');
		else
			unicodeString.push_back((L'0')+ (*shapeIDsIter) ); 
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
	  " Exiting: LTKStrEncoding::numShapeStrToUnicode()" << endl;

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 08-SEP-2005
* NAME			: tamilShapeStrToUnicode 
* DESCRIPTION	: Do the mapping from shaperecognition ID 
*               : to Unicode string for tamil_iso_char  project
* ARGUMENTS		: shapeIDs - The shape recognizer output IDs
*               : unicodeString - output unicode string 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKStrEncoding::tamilShapeStrToUnicode(const vector<unsigned short>& shapeIDs, vector<unsigned short>& unicodeString)
{
	int errorCode;
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
	  " Entering: LTKStrEncoding::tamilShapeStrToUnicode()" << endl;

	vector<unsigned short>::const_iterator shapeIDsIter, shapeIDsEnd;
	int charIndex;  //index variable for unicode string
	unsigned short currentChar; //unicode representation of the character 
	bool matraFlag;   //indicates whether a matra is following the current character

	//iterating through the shape IDs
	shapeIDsEnd = shapeIDs.end();
	for(shapeIDsIter = shapeIDs.begin();shapeIDsIter != shapeIDsEnd;++shapeIDsIter)
	{
		if(*shapeIDsIter == SHRT_MAX )
			unicodeString.push_back(L' ');
		else if(*shapeIDsIter < 35)
		{
			if((errorCode = tamilCharToUnicode(*shapeIDsIter,unicodeString)) != SUCCESS)
			{
				return errorCode;
			} 
		}
		else if (*shapeIDsIter < 58 )
		{
			if((errorCode = tamilCharToUnicode((*shapeIDsIter-23),unicodeString)) != SUCCESS)
			{
				return errorCode;
			}
			unicodeString.push_back(0x0bbf); //i mAtra
		}
		else if (*shapeIDsIter < 81 )
		{
			if((errorCode = tamilCharToUnicode((*shapeIDsIter-46),unicodeString)) != SUCCESS)
			{
				return errorCode;
			}
			unicodeString.push_back(0x0bc0); //ii mAtra
		}
		else if (*shapeIDsIter < 99 )
		{
			if((errorCode = tamilCharToUnicode((*shapeIDsIter-69),unicodeString)) != SUCCESS)
			{
				return errorCode;
			}
			unicodeString.push_back(0x0bc1); //u mAtra
		}
		else if (*shapeIDsIter < 117 )
		{
			if((errorCode = tamilCharToUnicode((*shapeIDsIter-87),unicodeString)) != SUCCESS)
			unicodeString.push_back(0x0bc2); //uu mAtra
		}
		else if (*shapeIDsIter < 118 )
		{
			unicodeString.push_back(0x0bbe); //aa mAtra
		}
		else if (*shapeIDsIter < 119 )
		{
			unicodeString.push_back(0x0bc6); //e mAtra
		}
		else if (*shapeIDsIter < 120 )
		{
			unicodeString.push_back(0x0bc7); //E mAtra
		}
		else if (*shapeIDsIter < 121 )
		{
			unicodeString.push_back(0x0bc8); //ai mAtra
		}
		else if(*shapeIDsIter < 122 )
		{
			//letter shri
			unicodeString.push_back(0x0bb8);//ss
			unicodeString.push_back(0x0bcd);//halant
			unicodeString.push_back(0x0bb0);//r
			unicodeString.push_back(0x0bc0);//ii
		}
		else if(*shapeIDsIter < 127 )
		{
			if((errorCode = tamilCharToUnicode((*shapeIDsIter-92),unicodeString)) != SUCCESS)
			{
				return errorCode;
			}
			unicodeString.push_back(0x0bc1); //u mAtra
		}
		else if(*shapeIDsIter < 132 )
		{
			if((errorCode = tamilCharToUnicode((*shapeIDsIter-97),unicodeString)) != SUCCESS)
			{
				return errorCode;
			}
			unicodeString.push_back(0x0bc2); //u mAtra
		}
		else if(*shapeIDsIter < 155 )
		{
			if((errorCode = tamilCharToUnicode((*shapeIDsIter-120),unicodeString)) != SUCCESS)
			{
				return errorCode;
			}
			unicodeString.push_back(0x0bcd); //halant
		}
		else if (*shapeIDsIter < 156 )
		{
			unicodeString.push_back(0x0b94);
		}
		else
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EINVALID_SHAPEID <<":"<< getErrorMessage(EINVALID_SHAPEID)
                  <<"LTKStrEncoding::tamilShapeStrToUnicode()" <<endl;

			LTKReturnError(EINVALID_SHAPEID);
		}
	}

	//Applying rules for e, E , ai, o, O, and au matras
	charIndex = 0;
	while( charIndex < unicodeString.size() )
	{
		currentChar = unicodeString[charIndex];
		switch(currentChar)
		{
		case 0x0bc6://e
		case 0x0bc7://E
		case 0x0bc8://ai
			if( (charIndex +1) < unicodeString.size() )
			{
				unicodeString[charIndex]   = unicodeString[charIndex+1];
				unicodeString[charIndex+1] = currentChar;
				charIndex += 2;
			}
			else
			{
				++charIndex;
			}
			break;
		case 0x0bbe: //check for `o' or `O'
			if(charIndex>0)//within string bounds
			{
				if(unicodeString[charIndex-1] == 0x0bc6 )
				{
					unicodeString[charIndex-1] = 0x0bca;
					unicodeString.erase(unicodeString.begin()+charIndex);
				}
				else if(unicodeString[charIndex-1] == 0x0bc7 )
				{
					unicodeString[charIndex-1] = 0x0bcb;
					unicodeString.erase(unicodeString.begin()+charIndex);
				}
				else
				{
					++charIndex;
				}
			}
			else
			{
				++charIndex;
			}
			break;
		case 0x0bb3: //check for au
			matraFlag = (charIndex+1<unicodeString.size() && (unicodeString[charIndex+1] > 0x0bbd && unicodeString[charIndex+1] < 0x0bc3 ) );

			//if la is not follwed by a matra and is preceded by an e matra it is an au matra
			if((charIndex >0)&&(unicodeString[charIndex-1] == 0x0bc6) && (!matraFlag))
			{
				unicodeString[charIndex-1] = 0x0bcc;
				unicodeString.erase(unicodeString.begin()+charIndex);
			}
			else
			{
				++charIndex;
			}
			break;
		default:
			++charIndex;
			
		}
		
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
	  " Exiting: LTKStrEncoding::tamilShapeStrToUnicode()" << endl;

	return SUCCESS;
}

int LTKStrEncoding::tamilCharToUnicode(const unsigned short& shapeID,  vector<unsigned short>& unicodeString)
{

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
	  " Entering: LTKStrEncoding::tamilCharToUnicode()" << endl;


		if(shapeID < 35)
		{
			if(shapeID == 34)
			{
				unicodeString.push_back(0x0b95);
				unicodeString.push_back(0x0bcd);
				unicodeString.push_back(0x0bb7);
			}
			else
			{
				unicodeString.push_back(tamilIsoCharMap[shapeID]);
			}
		}
		else
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EINVALID_SHAPEID <<":"<< getErrorMessage(EINVALID_SHAPEID)
                  <<"LTKStrEncoding::tamilCharToUnicode()" <<endl;

			LTKReturnError(EINVALID_SHAPEID);
		}
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKStrEncoding::tamilCharToUnicode()" << endl;

	return SUCCESS;
}


/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 08-SEP-2005
* NAME			: ~LTKStrEncoding
* DESCRIPTION	: destructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
LTKStrEncoding::~LTKStrEncoding()
{

}
