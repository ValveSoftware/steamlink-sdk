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
 * $LastChangedDate: 2011-01-11 13:48:17 +0530 (Tue, 11 Jan 2011) $
 * $Revision: 827 $
 * $Author: mnab $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation of the Ink File Writer Module
 *
 * CONTENTS:   
 *
 * AUTHOR:     Bharath A.
 *
 * DATE:       March 22, 2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/
#include "LTKInkFileWriter.h"
#include "LTKMacros.h"
#include "LTKTrace.h"
#include "LTKTraceGroup.h"
#include "LTKLoggerUtil.h"

using namespace std;


/**********************************************************************************
* AUTHOR		: Bharath A.
* DATE			: 22-MAR-2005
* NAME			: LTKInkFileWriter
* DESCRIPTION	: Default Constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKInkFileWriter::LTKInkFileWriter()
{

}

/**********************************************************************************
* AUTHOR		: Bharath A.
* DATE			: 22-MAR-2005
* NAME			: ~LTKInkFileWriter
* DESCRIPTION	: Destructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
LTKInkFileWriter::~LTKInkFileWriter()
{

}

/**********************************************************************************
* AUTHOR		: Bharath A.
* DATE			: 22-MAR-2005
* NAME			: writeRawInkFile
* DESCRIPTION	: This is a static method which writes the trace group supplied to the file name specified with X and Y DPI info.
* ARGUMENTS			 traceGroup trace group to be written onto the file
*					 fileName name of the file
*					 xDPI x-coordinate dots per inch
*					 yDPI y-coordinate dots per inch
* 	: 
* RETURNS		: SUCCESS on successful write operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

int LTKInkFileWriter::writeRawInkFile(const LTKTraceGroup& traceGroup,const string& fileName,int xDPI,int yDPI)
{
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKInkFileWriter::writeRawInkFile()" << endl;

		if(traceGroup.getNumTraces()>=1)
		{
			LTKTrace tempTrace;
			traceGroup.getTraceAt(0, tempTrace);

			int numOfChannels = tempTrace.getTraceFormat().getNumChannels();
			
			std::ofstream output(fileName.c_str());
			output<<"-6 "<<xDPI<<" "<<yDPI<<endl;
			for(int i=0;i<traceGroup.getNumTraces();i++)
			{
				LTKTrace trace;
				
				traceGroup.getTraceAt(i, trace);

				for(int j=0;j<trace.getNumberOfPoints();j++)
				{
					floatVector pointVec;
					trace.getPointAt(j, pointVec);

					for(int k=0;k<pointVec.size();k++){
						if(k==pointVec.size()-1){
								output<<pointVec[k]<<endl;
						}
						else{
							output<<pointVec[k]<<" ";
						}
					}
				}

				for(int d=0;d<numOfChannels;d++){
					if(d==numOfChannels-1){
						output<<"-1"<<endl;
					}
					else{
						output<<"-1 ";
					}
				}
				
			}

			for(int e=0;e<numOfChannels;e++){
				if(e==numOfChannels-1){
					output<<"-2"<<endl;
				}
				else{
					output<<"-2 ";
				}
			}
			output.close();
			LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
			  " Exiting: LTKInkFileWriter::writeRawInkFile()" << endl;

			return SUCCESS;
		}
		else
		{
			LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
				  " Exiting: LTKInkFileWriter::writeRawInkFile()" << endl;
			return FAILURE;
		}
}

/**********************************************************************************
* AUTHOR		: Bharath A.
* DATE			: 22-MAR-2005
* NAME			: writeUnipenInkFile
* DESCRIPTION	: This is a static method which writes the trace group supplied to the file name specified with X and Y DPI info, in unipen format
* ARGUMENTS			 traceGroup trace group to be written onto the file
*					 fileName name of the file
*					 xDPI x-coordinate dots per inch
*					 yDPI y-coordinate dots per inch
* 	: 
* RETURNS		: SUCCESS on successful write operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
int LTKInkFileWriter::writeUnipenInkFile(const LTKTraceGroup& traceGroup,const string& fileName,int xDPI,int yDPI)
{
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
			  " Entering: LTKInkFileWriter::writeUnipenInkFile()" << endl;

		if(traceGroup.getNumTraces()>=1)
		{
			LTKTrace tempTrace;
			traceGroup.getTraceAt(0, tempTrace);

			vector<string> channelNames = tempTrace.getTraceFormat().getAllChannelNames();
			std::ofstream output(fileName.c_str());
			output<<".VERSION 1.0"<<endl;
			output<<".COORD";
			for(int c=0;c<channelNames.size();c++)
			{
				output<<" "<<channelNames[c];

			}
			output<<endl;
			output<<".X_POINTS_PER_INCH "<<xDPI<<endl;
			output<<".Y_POINTS_PER_INCH "<<yDPI<<endl;
			for(int i=0;i<traceGroup.getNumTraces();i++)
			{
				output<<".PEN_DOWN"<<endl;
				
				LTKTrace trace;
				traceGroup.getTraceAt(i, trace);
				
				for(int j=0;j<trace.getNumberOfPoints();j++){
					
					floatVector pointVec;
					trace.getPointAt(j, pointVec);

					for(int k=0;k<pointVec.size();k++){
						if(k==pointVec.size()-1){
								output<<pointVec[k]<<endl;
						}
						else{
							output<<pointVec[k]<<" ";
						}
					}
				}
				output<<".PEN_UP"<<endl;
			}
			LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
			  " Exiting: LTKInkFileWriter::writeUnipenInkFile()" << endl;
			return SUCCESS;
		}
		else
		{
			LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
			  " Exiting: LTKInkFileWriter::writeUnipenInkFile()" << endl;
			return FAILURE;
		}
}