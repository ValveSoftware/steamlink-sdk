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
 * $LastChangedDate: 2011-04-05 13:49:39 +0530 (Tue, 05 Apr 2011) $
 * $Revision: 844 $
 * $Author: mnab $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation of the Ink File Reader Module
 *
 * CONTENTS:
 *	readInkFile
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 * Deepu V.     14 Sept 2005    Added unipen file reading function
 *                              that reads annotation.
************************************************************************/
#include "LTKChannel.h"

#include "LTKTraceFormat.h"

#include "LTKTrace.h"

#include "LTKTraceGroup.h"

#include "LTKCaptureDevice.h"

#include "LTKScreenContext.h"

#include "LTKStringUtil.h"

#include "LTKInc.h"

#include "LTKException.h"

#include "LTKMacros.h"

#include "LTKErrors.h"

#include "LTKErrorsList.h"

#include "LTKLoggerUtil.h"
#include "LTKInkFileReader.h"

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKInkFileReader
* DESCRIPTION	: Default Constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKInkFileReader::LTKInkFileReader(){}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: readRawInkFile
* DESCRIPTION	: reads contents of a file containing raw ink stored in a specified format into
*				  a trace group object. Also the device information stored in the ink file is read
*                 into a capture device object.
* ARGUMENTS		: inkFile - name of the file containing the ink
*				  traceGroup - trace group into which the ink has to be read into
*				  captureDevice - capture device object into which device info is to be read into
*				  screenContext - writing area information
* RETURNS		: SUCCESS on successfully reading the ink file into 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKInkFileReader::readRawInkFile(const string& inkFile, LTKTraceGroup& traceGroup, LTKCaptureDevice& captureDevice, LTKScreenContext& screenContext)
{

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
	  " Entering: LTKInkFileReader::readRawInkFile()" << endl;
    
	string dataLine;

	vector<string> dataVector;

	vector<float> point;				//	a point of a trace

	int pointIndex;

	if(inkFile.empty())
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
            <<"Error : "<< EINKFILE_EMPTY <<":"<< getErrorMessage(EINKFILE_EMPTY)
            <<"LTKInkFileReader::readRawInkFile()" <<endl;

		LTKReturnError(EINKFILE_EMPTY);
	}

	//	opening the ink file
	
	ifstream infile(inkFile.c_str());

	//	checking if the file open was successful

	if(!infile)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
			<<"Error: LTKInkFileReader::readRawInkFile()"<<endl;

		LTKReturnError(EINK_FILE_OPEN);
	}

	vector<LTKChannel> channels; 				//	channels of a trace 

	LTKChannel xChannel("X", DT_FLOAT, true);	//	x-coordinate channel of the trace
	
	LTKChannel yChannel("Y", DT_FLOAT, true);	//	y-coordinate channel of the trace

	LTKChannel tChannel("T", DT_FLOAT, true);	//	time channel of the trace

	//	initializing the channels of the trace

	channels.push_back(xChannel);
	
	channels.push_back(yChannel);

	channels.push_back(tChannel);

	//	composing the trace format object
	
	LTKTraceFormat traceFormat(channels);

	//	reading the ink file

	while(infile)
	{
		LTKTrace trace(traceFormat);

		while(infile)
		{
			getline(infile, dataLine);

			LTKStringUtil::tokenizeString(dataLine, " \t", dataVector);

            if(fabs( LTKStringUtil::convertStringToFloat(dataVector[0]) + 1 ) < EPS)
			{
				traceGroup.addTrace(trace);

				break;
			}
            else if(fabs( LTKStringUtil::convertStringToFloat(dataVector[0]) + 2 ) < EPS)
			{
				return SUCCESS;
			}
            else if(fabs( LTKStringUtil::convertStringToFloat(dataVector[0]) + 6 ) < EPS)
			{
                captureDevice.setXDPI(LTKStringUtil::convertStringToFloat(dataVector[1]));

                captureDevice.setYDPI(LTKStringUtil::convertStringToFloat(dataVector[2]));
			}
            else if(LTKStringUtil::convertStringToFloat(dataVector[0]) < 0)
			{
				//	unknown tag. skipping line

				continue;
			}
			else
			{

				for(pointIndex = 0; pointIndex < dataVector.size(); ++pointIndex)
				{
                    point.push_back(LTKStringUtil::convertStringToFloat(dataVector[pointIndex]));
				}

				if(dataVector.size() == 2)
				{
					point.push_back(0.0);
				}

				trace.addPoint(point);

				point.clear();
			}
		}
	}
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
	  " Exiting: LTKInkFileReader::readRawInkFile()" << endl;

	return FAILURE;
}

/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: readUnipenInkFile
* DESCRIPTION	: reads contents of a file containing unipen ink stored in a specified format into
*				  a trace group object. Also the device information stored in the ink file is read
*                 into a capture device object.
* ARGUMENTS		: inkFile - name of the file containing the ink
*				  traceGroup - trace group into which the ink has to be read into
*				  captureDevice - capture device object into which device info is to be read into
*				  screenContext - writing area information
* RETURNS		: SUCCESS on successfully reading the ink file into 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKInkFileReader::readUnipenInkFile(const string& inkFile, LTKTraceGroup& traceGroup, LTKCaptureDevice& captureDevice, LTKScreenContext& screenContext)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
	  " Entering: LTKInkFileReader::readUnipenInkFile()" << endl;

	map<string,string> traceIndicesCommentsMap;

	string hierarchyLevel;
	string quality("ALL");
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
	  " Exiting: LTKInkFileReader::readUnipenInkFile()" << endl;

	return (readUnipenInkFileWithAnnotation(inkFile,hierarchyLevel,quality,traceGroup,traceIndicesCommentsMap,captureDevice,screenContext));
}

/**********************************************************************************
* AUTHOR		: Deepu V.
* DATE			: 14-SEP-2004
* NAME			: readUnipenInkFileWithAnnotation
* DESCRIPTION	: reads contents of a unipen file containing ink stored in a specified format into
*				  a trace group object. Also the device information stored in the ink file is read
*                 into a capture device object.The screen information is captured in screen context object.
* ARGUMENTS		: inkFile - name of the file containing the ink
*				  hierarchyLevel - level at which the ink is required, ex. WORD or CHARACTER that follows .SEGMENT
*				  quality - quality of the ink that is required. Can be GOOD,BAD,OK or ALL. Example, if ink of quality
*				  GOOD and BAD are required, then quality="GOOD,BAD" (NOTE:comma(,) is the delimiter) else if all ink 
*				  are required, quality="ALL"
*				  traceGroup - trace group into which the ink has to be read into
*				  traceIndicesCommentsMap - Map containing list of strokes separated by commas as key and the comments
*				  to that trace group unit as value (ex. key-"2,4,5" value-"delayed stroke"
*				  captureDevice - capture device object into which device info is to be read into
*				  screenContext - writing area information
* RETURNS		: SUCCESS on successfully reading the ink file into 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/



int LTKInkFileReader::readUnipenInkFileWithAnnotation(const string& inkFile,const string& hierarchyLevel,const string& quality, LTKTraceGroup& traceGroup,map<string,string>& traceIndicesCommentsMap,LTKCaptureDevice& captureDevice, LTKScreenContext& screenContext)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
	  " Entering: LTKInkFileReader::readUnipenInkFileWithAnnotation()" << endl;

	vector<float> point;				//	a point of a trace

	float xDpi;							//	device resolution in the x direction

	float yDpi;							//	device resolution in the y direction

	float bboxLeft;						//	leftmost x-coord of the writing area

	float bboxBottom;					//	bottommost y-coord of the writing area

	float bboxRight;					//	rightmost x-coord of the writing area

	float bboxTop;						//	topmost y-coord of the writing area

	float floatChannelValue;			//	channel value of type float

	long longChannelValue;				//	channel value of type long

	string channelNames;				//	string containing all the channel names

	vector<string> channelNamesVector;	//	vector of channel names

	int channelIndex;					//	index to loop over all channels in the channel list

	vector<string> qualityLevels;		//	list of quality levels required

	vector<string> coordVals;			//	list of coordinate values present

	string remainingLine;				//remaining of the line that does not contain the required hierarchy level

	bool verFlag = false, hlevelFlag = false, coordFlag = false;//  bool level that denote whether version Info, Hierarchy level and coordinate info are set

	bool pendownFlag = false;

    LTKStringUtil::tokenizeString(quality,",", qualityLevels);

	//	opening the ink file
	
	if(inkFile.empty())
	{
	     LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EINKFILE_EMPTY <<":"<< getErrorMessage(EINKFILE_EMPTY)
                  <<"LTKInkFileReader::readUnipenInkFileWithAnnotation()" <<endl;

		LTKReturnError(EINKFILE_EMPTY);
	}

	ifstream infile(inkFile.c_str());	//	

	//	checking if the file open was successful

	if(!infile)
	{
	     LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EINK_FILE_OPEN <<":"<< getErrorMessage(EINK_FILE_OPEN)
                  <<"LTKInkFileReader::readUnipenInkFileWithAnnotation()" <<endl;

		LTKReturnError(EINK_FILE_OPEN);
	}

	LTKTrace *trace = NULL;						//	initializing trace to NULL

	vector<LTKChannel> channels;				//	channels of a trace 

	LTKTraceFormat traceFormat;					//	format of the trace

	//	reading the ink file

	string keyWord;								//	a key word of the unipen format

	while(infile)
	{
		keyWord = "";
		infile >> keyWord;

		if(keyWord == ".COORD")
		{
			coordFlag = true;

			getline(infile, channelNames);

			LTKStringUtil::tokenizeString(channelNames, " \t", channelNamesVector);

			for(channelIndex = 0; channelIndex < channelNamesVector.size(); ++channelIndex)
			{
				if(channelNamesVector[channelIndex] == "T")
				{
					LTKChannel channel(channelNamesVector[channelIndex], DT_LONG, true);	
					channels.push_back(channel);
				}
				else
				{
					LTKChannel channel(channelNamesVector[channelIndex], DT_FLOAT, true);
					channels.push_back(channel);
				}
			}

			traceFormat.setChannelFormat(channels);

		}
		else if(keyWord == ".X_POINTS_PER_INCH")
		{
			infile >> xDpi;
			captureDevice.setXDPI(xDpi);

		}
		else if(keyWord == ".Y_POINTS_PER_INCH")
		{
			infile >> yDpi;
			captureDevice.setYDPI(yDpi);
		}
		else if(keyWord == ".H_LINE")
		{
			infile >> bboxBottom >> bboxTop;
			screenContext.setBboxBottom(bboxBottom);
			screenContext.setBboxTop(bboxTop);
		}
		else if(keyWord == ".V_LINE")
		{
			infile >> bboxLeft >> bboxRight;

			screenContext.setBboxLeft(bboxLeft);

			screenContext.setBboxRight(bboxRight);
		}
		else if(keyWord==".SEGMENT")
		{
			string strHierarchyLevel;	//stores the hierarchy level (ex. CHARACTER or WORD)
			string strStrokeIndices;	//comma separated stroke indices
			string strQuality;			//annotated quality of the trace/trace group
			string strComments;			//comments about the ink
			
			infile >> strHierarchyLevel;
									
			if(strHierarchyLevel==hierarchyLevel)	//if the encountered hierarchy level is the required
			{

				string checkString;
				getline(infile,checkString,'\n');
				
				if(checkString.empty())
				{

					LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
						"Annotation not found at the specified hierarchy level:" << 
						hierarchyLevel << " in "+inkFile << endl;
					//return FAILURE;
					continue;

				}

				vector<string> tokens;

				LTKStringUtil::tokenizeString(checkString," ", tokens);

				if(tokens.size()>=3)
				{

					strStrokeIndices=tokens[0];
					strQuality=tokens[1];
					for(int i=2;i<tokens.size();++i)
					{
						strComments=strComments+tokens[i]+" ";
					}

					strComments=strComments.substr(0,strComments.length()-1); //removing the last space added
				}
				else
				{
					LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
                        "Invalid annotation format at the specified hierarchy level:" <<
                        hierarchyLevel << " in "  << inkFile << endl;
					//return FAILURE;
					continue;
				}

	
				strComments=strComments.substr(1,strComments.length()-2);	//to remove the leading space and double quoutes

				bool isRequiredQuality=false;
				if(quality=="ALL")	//if no condition on quality
				{
					isRequiredQuality=true;
				}
				else
				{
					for(vector<string>::iterator iter=qualityLevels.begin();iter!=qualityLevels.end();++iter)
					{

						if((*iter)==strQuality)
						{
							isRequiredQuality=true;
							break;
						}
						
					}

				}

				if(isRequiredQuality)
				{
					//if the trace/trace group is of required quality stores the stroke
					//indices and comments in the output map
					traceIndicesCommentsMap.insert(make_pair(strStrokeIndices,strComments));

				}
			}
			else	//if not the required hierarchy level, just get the remaining line
			{
				if(keyWord == ".VERSION")
					verFlag = true;
				else if ((keyWord == ".HIERARCHY"))
					hlevelFlag = true;

				getline(infile,remainingLine);

			}

		}
		else if(keyWord == ".PEN_DOWN")
		{
			if (pendownFlag)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EINKFILE_CORRUPTED <<":"<< getErrorMessage(EINKFILE_CORRUPTED)
                  <<"LTKInkFileReader::readUnipenInkFileWithAnnotation()" <<endl;

				LTKReturnError(EINKFILE_CORRUPTED);
			}

			pendownFlag = true;

			LTKTrace trace(traceFormat); 

			
			while(infile)
			{
				infile >> keyWord;

				if(keyWord == ".PEN_UP")
				{
					if (!pendownFlag)
					{
						LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		                  <<"Error : "<< EINKFILE_CORRUPTED <<":"<< getErrorMessage(EINKFILE_CORRUPTED)
				          <<"LTKInkFileReader::readUnipenInkFileWithAnnotation()" <<endl;
						
						LTKReturnError(EINKFILE_CORRUPTED);
					}

					pendownFlag = false;

					if(trace.getNumberOfPoints() == 0)
					{
						LOG(LTKLogger::LTK_LOGLEVEL_ERR)
								<<"Error: LTKInkFileReader::readUnipenInkFileWithAnnotation()"<<endl;

						LTKReturnError(EEMPTY_TRACE);		
					}

					traceGroup.addTrace(trace);
					
					break;
				}
				else
				{
					// BUGFIX : if no attributes in input ink file, throw error and stop
					if(channelNamesVector.empty()) 
					{
						//if(!verFlag)
						//{
						//}
                       // cout<<" keyword = "<<keyWord<<endl;
                                                
						LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		                  <<"Error : "<< EINKFILE_CORRUPTED <<":"<< getErrorMessage(EINKFILE_CORRUPTED)
				          <<"LTKInkFileReader::readUnipenInkFileWithAnnotation()" <<endl;
						
						LTKReturnError(EINKFILE_CORRUPTED);
					}

					if(channelNamesVector[0] == "T")
					{
						longChannelValue = atol(keyWord.c_str());

						point.push_back(longChannelValue);
					}
					else
					{
                        floatChannelValue = LTKStringUtil::convertStringToFloat(keyWord);

						point.push_back(floatChannelValue);
					}

					getline(infile,remainingLine);

					coordVals.clear();
					
					LTKStringUtil::tokenizeString(remainingLine,string(" "), coordVals);

					if (coordVals.size() != (channelNamesVector.size() -1))
					{
                        int index;
                        for (index = 0 ; index < channelNamesVector.size(); ++index)
                        {
                                cout << "coord name at index "<<channelNamesVector.at(index) <<endl;
                        }
                        cout<<"first coord val "<<keyWord;
                        for (index = 0 ; index < coordVals.size(); ++index)
                        {
                                cout << "coord val at index "<<coordVals.at(index) <<endl;
                        }
						LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		                  <<"Error : "<< EINKFILE_CORRUPTED <<":"<< getErrorMessage(EINKFILE_CORRUPTED)
				          <<"LTKInkFileReader::readUnipenInkFileWithAnnotation()" <<endl;
						
						LTKReturnError(EINKFILE_CORRUPTED);
					}

					for(channelIndex = 1; channelIndex < channelNamesVector.size(); ++channelIndex)
					{
						if(channelNamesVector[channelIndex] == "T")
						{
							longChannelValue = atol((coordVals.at(channelIndex -1 )).c_str());

							point.push_back(longChannelValue);
						}
						else
						{
                            floatChannelValue = LTKStringUtil::convertStringToFloat(coordVals.at(channelIndex -1 ));

							point.push_back(floatChannelValue);
						}

					}

					trace.addPoint(point);

					point.clear();

				}
			}
		}
		else if(keyWord == ".PEN_UP")
		{
			if (!pendownFlag)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		                  <<"Error : "<< EINKFILE_CORRUPTED <<":"<< getErrorMessage(EINKFILE_CORRUPTED)
				          <<"LTKInkFileReader::readUnipenInkFileWithAnnotation()" <<endl;
						
				LTKReturnError(EINKFILE_CORRUPTED);
			}			
		}
		else
		{
			if(keyWord == ".VERSION")
				verFlag = true;
			else if ((keyWord == ".HIERARCHY"))
				hlevelFlag = true;

			getline(infile, keyWord);
		}
	}
		
	int numberOfTraces = traceGroup.getNumTraces();
	
	if(numberOfTraces == 0)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		      <<"Error : LTKInkFileReader::readUnipenInkFileWithAnnotation()" <<endl;
						
		LTKReturnError(EEMPTY_TRACE_GROUP);	
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
	  " Exiting: LTKInkFileReader::readUnipenInkFileWithAnnotation()" << endl;
	
	return SUCCESS;
}



/**********************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: ~LTKInkFileReader
* DESCRIPTION	: destructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKInkFileReader::~LTKInkFileReader(){}


