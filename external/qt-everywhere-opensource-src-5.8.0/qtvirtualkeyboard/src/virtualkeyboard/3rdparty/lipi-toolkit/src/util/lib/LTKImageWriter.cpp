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
 * $LastChangedDate: 2007-10-08 22:10:54 +0530 (Mon, 08 Oct 2007) $
 * $Revision: 252 $
 * $Author: bharatha $
 *
 ************************************************************************/

/************************************************************************
 * FILE DESCR: Definitions of Image Writer module
 *
 * CONTENTS:
 *		drawLTKTraceGroupToImage
 *		drawRawInkFileToImage
 *		drawUnipenFileToImage
 *		drawUnipenFileToImageWithBB
 *		showStartingPoint
 *		setColor
 *		setAlternateColor
 *		setOffstet
 *		normalizeSize
 *		getBoundingBox
 *		findMinXOfTrace
 *		findMaxXOfTrace
 *		drawPoint
 *		drawLine
 *		drawRectangle
 *		fillRectangle
 *		createTraceOrderInTraceGroup
 *
 * AUTHOR:     Bharath A
 *
 * DATE:       February 22, 2005
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#include "LTKImageWriter.h"
#include "LTKChannel.h"
#include "LTKTraceFormat.h"
#include "LTKTrace.h"
#include "LTKTraceGroup.h"
#include "LTKInkFileReader.h"
#include "LTKCaptureDevice.h"
#include "LTKPreprocessorInterface.h"
#include "LTKErrors.h"
#include "LTKErrorsList.h"
#include "LTKScreenContext.h"
#include "LTKLoggerUtil.h"

#include "LTKException.h"


using namespace std;
/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: LTKImageWriter
* DESCRIPTION	: Default Constructor
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

LTKImageWriter::LTKImageWriter():
m_width(0),
m_height(0),
m_pixels(NULL),
m_showBB(false),
m_showSP(false),
m_red(255),
m_green(0),
m_blue(0),
m_altRed(0),
m_altGreen(0),
m_altBlue(255),
m_offset(0)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		"Entering: LTKImageWriter::LTKImageWriter()" <<endl;

	xChannelstr = "X";
	yChannelstr = "Y";

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		"Exiting: LTKImageWriter::LTKImageWriter()" <<endl;
}



/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: ~LTKImageWriter
* DESCRIPTION	: Destructor deletes the pixels array
* ARGUMENTS		:
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	LTKImageWriter::~LTKImageWriter()
	{
		if(m_pixels!=NULL){
			delete [] m_pixels;
		}

	}



/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: drawLTKTraceGroupToImage
* DESCRIPTION	: The function draws LTKTraceGroup to image file with specified width,height,color and offset.
					If offset not equal to zero, the image would be in the trace order.
* ARGUMENTS		:
*		traceGroup the trace group that is to be drawn
*		imgFileName name of the file that is to be created to draw the image
*		imgWidth width of the image
*		imgHeight height of the image
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	void LTKImageWriter::drawLTKTraceGroupToImage(const LTKTraceGroup& traceGroup,
							const string imgFileName,int imgWidth,int imgHeight)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Entering: LTKImageWriter::drawLTKTraceGroupToImage()" << endl;

		m_width=imgWidth+5;
		m_height=imgHeight+5;
		delete [] m_pixels;
		m_pixels=new unsigned char[3*m_width*m_height];

		for(int c=0;c<(3*m_width*m_height);c++)
        {
			m_pixels[c]=0xff;
		}

		LTKTraceGroup normTraceGroup;

		if(m_offset)
        {
			createTraceOrderInTraceGroup(traceGroup,normTraceGroup);

		}
		else
		{
			normTraceGroup=traceGroup;
		}

		normTraceGroup.getBoundingBox(m_xMin,m_yMin,m_xMax,m_yMax);
		float newXScaleFactor = (float) imgWidth/((float)(m_xMax-m_xMin)/normTraceGroup.getXScaleFactor());

		float newYScaleFactor = (float)imgHeight / (float) ((m_yMax-m_yMin)/normTraceGroup.getYScaleFactor());

		float origin = 0;

		normTraceGroup.affineTransform(newXScaleFactor,newYScaleFactor,origin,origin,XMIN_YMIN);

		int numOfTracesToDraw=normTraceGroup.getNumTraces();

		if(m_showBB)
        {
			numOfTracesToDraw = normTraceGroup.getNumTraces() - 1;

			LTKTrace trace ;
			normTraceGroup.getTraceAt(numOfTracesToDraw, trace);

			for(int j=1; j< trace.getNumberOfPoints(); j++)
            {
				floatVector fromPoint, toPoint;

				trace.getPointAt(j-1, fromPoint);

				trace.getPointAt(j, toPoint);

				drawLine((int)fromPoint[0]+2, (int)fromPoint[1]+2,
					     (int)toPoint[0]+2, (int)toPoint[1]+2, 255, 255, 0);
			}
		}


		for(int i=0;i<numOfTracesToDraw;i++)
		{
			LTKTrace trace;

			normTraceGroup.getTraceAt(i, trace);

			for(int j=1 ; j < trace.getNumberOfPoints() ; j++)
			{
				floatVector fromPoint, toPoint;

				trace.getPointAt(j-1, fromPoint);

				trace.getPointAt(j, toPoint);

					if((i%2)==0)
                    {
						if(j == 1)
						{
							if(m_showSP)
							{
								drawRectangle((int)fromPoint[0]+2-2,(int)fromPoint[1]+2+2,(int)fromPoint[0]+2+2,(int)fromPoint[1]+2-2,0,0,0);

							}

						}
						drawLine((int)fromPoint[0]+2,(int)fromPoint[1]+2,(int)toPoint[0]+2,(int)toPoint[1]+2,m_red,m_green,m_blue);
					}
					else
                    {
						if(j==1)
						{
							if(m_showSP)
							{
								drawRectangle((int)fromPoint[0]+2-2,(int)fromPoint[1]+2+2,(int)fromPoint[0]+2+2,(int)fromPoint[1]+2-2,0,0,0);

							}

						}
						drawLine((int)fromPoint[0]+2,(int)fromPoint[1]+2,(int)toPoint[0]+2,(int)toPoint[1]+2,m_altRed,m_altGreen,m_altBlue);

					}

			}

		}

		drawBMPImage(imgFileName,m_pixels,m_width,m_height);
		delete [] m_pixels;

		m_pixels=NULL;

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Exiting: LTKImageWriter::drawLTKTraceGroupToImage()" << endl;
	}



/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: drawLTKTraceGroupToImage
* DESCRIPTION	: The function draws LTKTraceGroup to image file with specified color and offset. The width and height would be determined from the trace group. If offset not equal to zero, the image would be in the trace order.
* ARGUMENTS		:
*		 traceGroup the trace group that is to be drawn
*		 imgFileName name of the file that is to be created to draw the image
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	void LTKImageWriter::drawLTKTraceGroupToImage(const LTKTraceGroup& traceGroup,const string imgFileName)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKImageWriter::drawLTKTraceGroupToImage(const LTKTraceGroup&,const string)" << endl;


		traceGroup.getBoundingBox(m_xMin,m_yMin,m_xMax,m_yMax);


		drawLTKTraceGroupToImage(traceGroup,imgFileName,(int)(m_xMax-m_xMin),(int)(m_yMax-m_yMin));
		
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKImageWriter::drawLTKTraceGroupToImage(const LTKTraceGroup&,const string)" << endl;
	}




/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: drawLTKTraceGroupToImage
* DESCRIPTION	: The function draws LTKTraceGroup to image file with specified color and offset. The size specified is the bound on larger dimension and the trace group is normalized to maintain the aspect ratio.
* ARGUMENTS		:
*		 traceGroup the trace group that is to be drawn
*		 imgFileName name of the file that is to be created to draw the image
*		 size bound on larger dimension
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	void LTKImageWriter::drawLTKTraceGroupToImage(const LTKTraceGroup& traceGroup,
													const string imgFileName, int size)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
			" Entering: LTKImageWriter::drawLTKTraceGroupToImage(const LTKTraceGroup&,const string,size)" << endl;

			LTKTraceGroup normTraceGroup = traceGroup;

			normTraceGroup.getBoundingBox(m_xMin,m_yMin,m_xMax,m_yMax);

			float	xScale = ((float)fabs(m_xMax - m_xMin))/normTraceGroup.getXScaleFactor();

			float	yScale = ((float)fabs(m_yMax - m_yMin))/normTraceGroup.getYScaleFactor();


			if(yScale > xScale)
			{
				xScale = yScale;
			}
			else
			{
				yScale = xScale;
			}

			float xScaleFactor =  size / xScale ;

			float yScaleFactor = size / yScale;

			float origin = 0.0;

			normTraceGroup.affineTransform(xScaleFactor,yScaleFactor,origin,origin,XMIN_YMIN);

			drawLTKTraceGroupToImage(normTraceGroup,imgFileName);

			LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
			  " Exiting: LTKImageWriter::drawLTKTraceGroupToImage(const LTKTraceGroup&,const string,size)" << endl;
	}


/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: drawLTKTraceGroupToImage
* DESCRIPTION	: The function draws LTKTraceGroup to image file with specified color and offset. The size specified is the bound on larger dimension and the trace group is normalized to maintain the aspect ratio.The bounding box of the image is also drawn to show relative position.
* ARGUMENTS		:
*		 traceGroup the trace group that is to be drawn
*		 screenContext screenContext the reference to screen Context for determining the bounding box
*		 imgFileName name of the file that is to be created to draw the image
*		 size bound on larger dimension
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	void LTKImageWriter::drawLTKTraceGroupToImageWithBB(const LTKTraceGroup& traceGroup,const LTKScreenContext& screenContext,
															const string imgFileName,int size)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKImageWriter::drawLTKTraceGroupToImageWithBB()" << endl;

		LTKTraceGroup newTG;
		newTG=traceGroup;
		float x1=screenContext.getBboxLeft();
		float y1=screenContext.getBboxBottom();
		float x2=screenContext.getBboxRight();
		float y2=screenContext.getBboxTop();

		vector<LTKChannel> channels;

		LTKChannel xChannel("X", DT_FLOAT, true);
		LTKChannel yChannel("Y", DT_FLOAT, true);

		channels.push_back(xChannel);
		channels.push_back(yChannel);

		LTKTraceFormat traceFormat(channels);

		LTKTrace trace(traceFormat);

		vector<float> point1;
		point1.push_back(x1);
		point1.push_back(y1);
		trace.addPoint(point1);

		vector<float> point2;
		point2.push_back(x2);
		point2.push_back(y1);
		trace.addPoint(point2);

		vector<float> point3;
		point3.push_back(x2);
		point3.push_back(y2);
		trace.addPoint(point3);

		vector<float> point4;
		point4.push_back(x1);
		point4.push_back(y2);
		trace.addPoint(point4);

		vector<float> point5;
		point5.push_back(x1);
		point5.push_back(y1);
		trace.addPoint(point5);

		newTG.addTrace(trace);

		m_showBB=true;

		drawLTKTraceGroupToImage(newTG,imgFileName,size);
		m_showBB=false;

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKImageWriter::drawLTKTraceGroupToImageWithBB()" << endl;
	}

/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: drawRawInkFileToImage
* DESCRIPTION	: The function creates LTKTraceGroup from the specified data file and draws it to image file with specified width,height,color and offset. If offset not equal to zero, the image would be in the trace order.
* ARGUMENTS		:
*		 fileName path to the datafile
*		 imgFileName name of the file that is to be created to draw the image
*		 imgWidth width of the image
*		 imgHeight height of the image
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	void LTKImageWriter::drawRawInkFileToImage(const string fileName, const string imgFileName,
														int imgWidth,int imgHeight)
	{
		int errorCode;
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKImageWriter::drawRawInkFileToImage(const string, const string,int,int)" << endl;

		LTKTraceGroup traceGroup;
		LTKCaptureDevice captureDevice;
		LTKScreenContext screenContext;
		string strFileName(fileName);
		try
		{
			LTKInkFileReader::readRawInkFile(strFileName,traceGroup,captureDevice,screenContext);
		}
		catch(LTKException e)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EINK_FILE_OPEN <<":"<< getErrorMessage(EINK_FILE_OPEN)
                  <<"LTKImageWriter::drawRawInkFileToImage(const string, const string,int,int)" <<endl;

		errorCode = EINK_FILE_OPEN;
		}

		drawLTKTraceGroupToImage(traceGroup,imgFileName,imgWidth,imgHeight);

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKImageWriter::drawRawInkFileToImage(const string, const string,int,int)" << endl;
	}


/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: drawRawInkFileToImage
* DESCRIPTION	: The function creates LTKTraceGroup from the specified data file and draws it to image file with specified color and offset. The width and height would be determined from the trace group.
* ARGUMENTS		:
*		 fileName path to the datafile
*		 imgFileName name of the file that is to be created to draw the image
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	void LTKImageWriter::drawRawInkFileToImage(const string fileName, const string imgFileName)
	{
		int errorCode;
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKImageWriter::drawRawInkFileToImage(const string, const string)" << endl;

		LTKTraceGroup traceGroup;
		LTKCaptureDevice captureDevice;
		LTKScreenContext screenContext;
		string strFileName(fileName);

		try
		{
			LTKInkFileReader::readRawInkFile(strFileName,traceGroup,captureDevice,screenContext);
		}
		catch(LTKException e)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EINK_FILE_OPEN <<":"<< getErrorMessage(EINK_FILE_OPEN)
                  <<"LTKImageWriter::drawRawInkFileToImage(const string, const string)" <<endl;

			errorCode = EINK_FILE_OPEN;
		}

		drawLTKTraceGroupToImage(traceGroup,imgFileName);

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKImageWriter::drawRawInkFileToImage(const string, const string)" << endl;
	}


/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: drawRawInkFileToImage
* DESCRIPTION	: The function draws LTKTraceGroup to image file with specified color and offset. The size specified is the bound on larger dimension and the trace group is normalized to maintain the aspect ratio.
* ARGUMENTS		:
*		 fileName path to the data file
*		 imgFileName name of the file that is to be created to draw the image
*		 size bound on larger dimension
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	void LTKImageWriter::drawRawInkFileToImage(const string fileName,const string imgFileName,int size)
	{
		int errorCode;
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
			" Entering: LTKImageWriter::drawRawInkFileToImage(const string,const string,int)" << endl;

		LTKTraceGroup traceGroup;
		LTKCaptureDevice captureDevice;
		LTKScreenContext screenContext;
		string strFileName(fileName);
		try
		{
			LTKInkFileReader::readRawInkFile(strFileName,traceGroup,captureDevice,screenContext);
		}
		catch(LTKException e)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EINK_FILE_OPEN <<":"<< getErrorMessage(EINK_FILE_OPEN)
                  <<"LTKImageWriter::drawRawInkFileToImage(const string,const string,int)" <<endl;

			errorCode = EINK_FILE_OPEN;
		}

		drawLTKTraceGroupToImage(traceGroup,imgFileName,size);

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKImageWriter::drawRawInkFileToImage(const string,const string,int)" << endl;
	}




/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: drawUnipenFileToImage
* DESCRIPTION	: The function creates LTKTraceGroup from the specified unipen data file and draws it to image file with specified width,height,color and offset. If offset not equal to zero, the image would be in the trace order.
* ARGUMENTS		:
*		 unipenFileName path to the datafile
*		 imgFileName name of the file that is to be created to draw the image
*		 imgWidth width of the image
*		 imgHeight height of the image
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	void LTKImageWriter::drawUnipenFileToImage(const string unipenFileName,const string imgFileName,int imgWidth,int imgHeight)
	{
		int errorCode;
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKImageWriter::drawUnipenFileToImage(const string,const string,int,int)" << endl;

		LTKTraceGroup traceGroup;
		LTKCaptureDevice captureDevice;
		LTKScreenContext screenContext;
		string strFileName(unipenFileName);
		try
		{
			LTKInkFileReader::readUnipenInkFile(strFileName,traceGroup,captureDevice,screenContext);
		}
		catch(LTKException e)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EINK_FILE_OPEN <<":"<< getErrorMessage(EINK_FILE_OPEN)
                  <<"LTKImageWriter::drawUnipenFileToImage(const string,const string,int,int)" <<endl;

			errorCode = EINK_FILE_OPEN;
		}
		drawLTKTraceGroupToImage(traceGroup,imgFileName,imgWidth,imgHeight);
		
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKImageWriter::drawUnipenFileToImage(const string,const string,int,int)" << endl;
	}



/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: drawUnipenFileToImage
* DESCRIPTION	: The function creates LTKTraceGroup from the specified unipen data file and draws it to image file with specified color and offset. The width and height would be determined from the trace group.
* ARGUMENTS		:
*		 unipenFileName path to the datafile
*		 imgFileName name of the file that is to be created to draw the image
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	void LTKImageWriter::drawUnipenFileToImage(const string& unipenFileName,const string& imgFileName)
	{
		int errorCode;
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKImageWriter::drawUnipenFileToImage(const string&,const string&)" << endl;

		LTKTraceGroup traceGroup;
		LTKCaptureDevice captureDevice;
		LTKScreenContext screenContext;
		string strFileName(unipenFileName);
		try
		{
			LTKInkFileReader::readUnipenInkFile(strFileName,traceGroup,captureDevice,screenContext);
		}
		catch(LTKException e)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EINK_FILE_OPEN <<":"<< getErrorMessage(EINK_FILE_OPEN)
                  <<"LTKImageWriter::drawUnipenFileToImage(const string&,const string&)" <<endl;

			errorCode = EINK_FILE_OPEN;
		}

		drawLTKTraceGroupToImage(traceGroup,imgFileName);

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKImageWriter::drawUnipenFileToImage(const string&,const string&)" << endl;
	}




/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: drawUnipenFileToImage
* DESCRIPTION	: The function creates LTKTraceGroup from the unipen data file specified and draws it to image file with specified color and offset. The size specified is the bound on larger dimension and the trace group is normalized to maintain the aspect ratio.
* ARGUMENTS		:
*		 unipenFileName path to the unipen data file
*		 imgFileName name of the file that is to be created to draw the image
*		 size bound on larger dimension
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	void LTKImageWriter::drawUnipenFileToImage(const string& unipenFileName,const string& imgFileName,int size)
	{
		int errorCode;
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Entering: LTKImageWriter::drawUnipenFileToImage(const string&,const string&,int)" << endl;


		LTKTraceGroup traceGroup;
		LTKCaptureDevice captureDevice;
		LTKScreenContext screenContext;
		string strFileName(unipenFileName);
		try
		{
			LTKInkFileReader::readUnipenInkFile(strFileName,traceGroup,captureDevice,screenContext);
		}
		catch(LTKException e)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EINK_FILE_OPEN <<":"<< getErrorMessage(EINK_FILE_OPEN)
                  <<"LTKImageWriter::drawUnipenFileToImage(const string&,const string&,int)" <<endl;


			errorCode = EINK_FILE_OPEN;
		}

		drawLTKTraceGroupToImage(traceGroup,imgFileName,size);

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKImageWriter::drawUnipenFileToImage(const string&,const string&,int)" << endl;
	}



/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: drawUnipenFileToImageWithBB
* DESCRIPTION	: The function creates LTKTraceGroup from the unipen data file specified and draws it to image file with specified color and offset. The size specified is the bound on larger dimension and the trace group is normalized to maintain the aspect ratio.The bounding box of the image is also drawn to show relative position.
* ARGUMENTS		:
*		 unipenFileName path to the unipen data file
*		 imgFileName name of the file that is to be created to draw the image
*		 size bound on larger dimension
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	void LTKImageWriter::drawUnipenFileToImageWithBB(const string& unipenFileName,const string& imgFileName,int size)
	{
		int errorCode;
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKImageWriter::drawUnipenFileToImageWithBB()" << endl;

		LTKTraceGroup traceGroup;
		string strFileName(unipenFileName);
		LTKCaptureDevice captureDevice;
		LTKScreenContext screenContext;
		try
		{
			LTKInkFileReader::readUnipenInkFile(strFileName,traceGroup,captureDevice,screenContext);
		}
		catch(LTKException e)
		{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
                  <<"Error : "<< EINK_FILE_OPEN <<":"<< getErrorMessage(EINK_FILE_OPEN)
                  <<"LTKImageWriter::drawUnipenFileToImageWithBB()" <<endl;

			errorCode = EINK_FILE_OPEN;
		}
		drawLTKTraceGroupToImageWithBB(traceGroup,screenContext,imgFileName,size);
		
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKImageWriter::drawUnipenFileToImageWithBB()" << endl;
	}



/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: showStartingPoint
* DESCRIPTION	: The set function for showing the starting point of each stroke
* ARGUMENTS		:
*			toShow flag to show the starting point
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	void LTKImageWriter::showStartingPoint(bool toShow)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKImageWriter::showStartingPoint()" << endl;

		m_showSP=toShow;

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKImageWriter::showStartingPoint()" << endl;
	}


/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: findMinXOfTrace
* DESCRIPTION	: The function finds the minimum x value of the given trace.
* ARGUMENTS		:
*		 trace input trace
*
* RETURNS		: minimum value of x
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
	int LTKImageWriter::findMinXOfTrace(const LTKTrace& trace,float& minX)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKImageWriter::findMinXOfTrace(const LTKTrace&,float&)" << endl;

		minX=numeric_limits<float>::infinity();
		for(int i=0;i<trace.getNumberOfPoints();i++){
			vector<float> point;

			trace.getPointAt(i, point);
			if(point[0]<minX){
				minX=point[0];
			}
		}

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		" Exiting: LTKImageWriter::findMinXOfTrace(const LTKTrace&,float&)" << endl;

		return SUCCESS;

	}



/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: findMaxXOfTrace
* DESCRIPTION	: The function finds the maximum x value of the given trace.
* ARGUMENTS		:
*		 trace input trace
*
* RETURNS		: maximum value of x
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	int LTKImageWriter::findMaxXOfTrace(const LTKTrace& trace,float& maxX)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKImageWriter::findMaxXOfTrace(const LTKTrace&,float&)" << endl;

		maxX = -FLT_MAX;

		for(int i=0;i<trace.getNumberOfPoints();i++){

			vector<float> point;

			trace.getPointAt(i, point);
			if(point[0]>maxX){

				maxX=point[0];
			}
		}
		
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKImageWriter::findMaxXOfTrace(const LTKTrace&,float&)" << endl;

		return SUCCESS;
	}




/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: drawPoint
* DESCRIPTION	: The function makes entries in the pixels array for the specified x,y and color.
* ARGUMENTS		:
*		 x,y coordinates of the point to be drawn
*		 red,green,blue RGB values of the color of the point
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	void LTKImageWriter::drawPoint(int x,int y,unsigned char red,unsigned char green,unsigned char blue){
		
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKImageWriter::drawPoint()" << endl;


		m_pixels[(3*((x)+y*m_width))]=red;
		m_pixels[(3*((x)+y*m_width))+1]=green;
		m_pixels[(3*((x)+y*m_width))+2]=blue;

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKImageWriter::drawPoint()" << endl;
	}




/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: drawLine
* DESCRIPTION	: The function draws line between the specified end points.
* ARGUMENTS		:
*		x1,y1,x2,y2 coordinates of the end points of the line to be drawn
*		red,green,blue RGB values of the color of the line
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	void LTKImageWriter::drawLine(int x1, int y1, int x2, int y2,unsigned char red,unsigned char green,unsigned char blue)
	{
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
	  " Entering: LTKImageWriter::drawLine()" << endl;

	  int x, y;
	  double k, s;

	  if (y1==y2) {
		if (x1>x2)
		 swap(x1,x2);
		for (x=x1; x<=x2; x++)
		  drawPoint(x, y1,red,green,blue);
	  } else {
		k = (double)(y2-y1)/(x2-x1);
		if (-1<=k && k<=1) {
		  if (x1>x2) {
		swap(x1, x2);
		swap(y1, y2);
		  }
		  for (x=x1, s=y1; x<=x2; x++, s+=k)
		drawPoint(x, (int)(s+.5),red,green,blue);
		} else {
		  if (y1>y2) {
		swap(x1, x2);
		swap(y1, y2);
		  }
		  k = 1/k;
		  for (s=x1, y=y1; y<=y2; s+=k, y++)
		drawPoint((int)(s+.5), y,red,green,blue);
		}
	  }

	  LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting:LTKImageWriter::drawLine()" << endl;
	}




/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: drawRectangle
* DESCRIPTION	: The function draws hollow rectange with specified color and diagonal end points.
* ARGUMENTS		:
*		x1,y1,x2,y2 coordinates of the end points of the diagonal of the rectangle
*		red,green,blue RGB values of the color of the lines
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	void LTKImageWriter::drawRectangle(int x1,int y1,int x2,int y2,unsigned char red,unsigned char green,unsigned char blue)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKImageWriter::drawRectangle()" << endl;


		int xmin,ymin,xmax,ymax;

		if(x1<x2){

			xmin=x1;xmax=x2;
		}
		else{
			xmin=x2;xmax=x1;
		}

		if(y1<y2){

			ymin=y1;ymax=y2;
		}
		else{
			ymin=y2;ymax=y1;
		}

		drawLine(xmin,ymin,xmin,ymax,red,green,blue);
		drawLine(xmin,ymin,xmax,ymin,red,green,blue);
		drawLine(xmin,ymax,xmax,ymax,red,green,blue);
		drawLine(xmax,ymin,xmax,ymax,red,green,blue);

		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKImageWriter::drawRectangle()" << endl;
	}




/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: drawRectangle
* DESCRIPTION	: The function draws a rectange with specified diagonal end points and fills it with the specified color.
* ARGUMENTS		:
*		x1,y1,x2,y2 coordinates of the end points of the diagonal of the rectangle
*		red,green,blue RGB values of the color of the lines
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
	void LTKImageWriter::fillRectangle(int x1,int y1,int x2,int y2,unsigned char red,unsigned char green,unsigned char blue)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKImageWriter::fillRectangle()" << endl;

		int xmin,ymin,xmax,ymax;

		if(x1<x2){

			xmin=x1;xmax=x2;
		}
		else{
			xmin=x2;xmax=x1;
		}

		if(y1<y2){

			ymin=y1;ymax=y2;
		}
		else{
			ymin=y2;ymax=y1;
		}


		for(int i=xmin;i<=xmax;i++)
		{
			drawLine(i,ymin,i,ymax,red,green,blue);
		}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		 " Exiting: LTKImageWriter::fillRectangle()" << endl;
	}



/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: createTraceOrderInTraceGroup
* DESCRIPTION	: The function offsets the input trace group by specified offset value and in the original trace order.
* ARGUMENTS		:
*		traceGroup input trace group
*		offsetTraceGroup trace group with offset
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	void  LTKImageWriter::createTraceOrderInTraceGroup(const LTKTraceGroup& traceGroup,LTKTraceGroup& offsetTraceGroup)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKImageWriter::createTraceOrderInTraceGroup()" << endl;


		float maxOfX=0.0;
		LTKTrace tempTrace;
		traceGroup.getTraceAt(0, tempTrace);

		findMaxXOfTrace(tempTrace,maxOfX);

		maxOfX += m_offset;


		offsetTraceGroup.addTrace(tempTrace);

		for(int i = 1 ; i < traceGroup.getNumTraces(); i++)
		{
			LTKTrace trace;
			traceGroup.getTraceAt(i, trace);

			float minOfTrace =0;

			findMinXOfTrace(trace,minOfTrace);

			LTKTrace offsetTrace=trace;
			int pointToShift=0;
			vector<float> newXChannel;
			for(int j=0;j<trace.getNumberOfPoints();j++){
				vector<float> point;

				trace.getPointAt(j, point);
				point[0]+=fabs(maxOfX-minOfTrace);
				newXChannel.push_back(point[0]);
			}

			offsetTrace.reassignChannelValues(xChannelstr,newXChannel);
			offsetTraceGroup.addTrace(offsetTrace);

			findMaxXOfTrace(offsetTrace,maxOfX);
			maxOfX += m_offset;

		}
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKImageWriter::createTraceOrderInTraceGroup()" << endl;
	}


/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: setColor
* DESCRIPTION	: Setter method for color of the starting stroke and subsequent alternate strokes
* ARGUMENTS		:
*		red value of Red in RGB combination
*		green value of Green in RGB combination
*		blue value of Blue in RGB combination
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	void LTKImageWriter::setColor(unsigned char red,unsigned char green,unsigned char blue)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKImageWriter::setColor()" << endl;

         m_red=red;
         m_green=green;
         m_blue=blue;

		 LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKImageWriter::setColor()" << endl;
    }


/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: setAlternateColor
* DESCRIPTION	: Setter method for color of alternate strokes
* ARGUMENTS		:
*		altRed value of Red in RGB combination
*		altGreen value of Green in RGB combination
*		altBlue value of Blue in RGB combination
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
	void LTKImageWriter::setAlternateColor(unsigned char altRed,unsigned char altGreen,unsigned char altBlue)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKImageWriter::setAlternateColor()" << endl;

         m_altRed=altRed;
         m_altGreen=altGreen;
         m_altBlue=altBlue;

		 LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKImageWriter::setAlternateColor()" << endl;
    }


/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 22-FEB-2005
* NAME			: setOffstet
* DESCRIPTION	: Setter method for offset value between strokes
* ARGUMENTS		:
*		offset value of offset
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/

	void LTKImageWriter::setOffset(int offset)
    {
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Entering: LTKImageWriter::setOffset()" << endl;

		if(offset < 0)
		{
			m_offset=0;
		}
		else
		{
			m_offset=offset;
		}
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKImageWriter::setOffset()" << endl;
    }



/**********************************************************************************
* AUTHOR		: Bharath A
* DATE			: 19-MAY-2005
* NAME			: drawBMPImage
* DESCRIPTION	: The function writes the given pixel array to the specified BMP file
* ARGUMENTS		:
*			fileName name of output bmp file name with extension as 'bmp'
*			pixelArray pixel array
*			width width of the image
*			height height of the image
*
* RETURNS		:
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
*************************************************************************************/
	void LTKImageWriter::drawBMPImage(string fileName,unsigned char* pixelArray,int width,int height)
	{
			LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
			  " Entering: LTKImageWriter::drawBMPImage()" << endl;

			short type;
			int size;
			short reserved1;
			short reserved2;
			int offset;
			int i,j;
			int biSize,biWidth,biHeight;
			short biPlanes, biBitCount;
			int biCompression,biSizeImage,biXPelsPerMeter,biYPelsPerMeter,biClrUsed,biClrImportant;
			FILE *bmp = fopen(fileName.c_str(),"wb");

			offset = 54;
			biSize = 40;

			biWidth=width;
			biHeight=height;
			biPlanes = 1;
			biBitCount = 24;
			biCompression = 0;
			biSizeImage =0;
			biXPelsPerMeter = 0;
			biYPelsPerMeter = 0;
			biClrUsed = 0;
			biClrImportant = 0;
			type = 19778;

			int padding = ( 4 - ( ( 3 * width ) % 4 ) ) % 4;

			size = 54 + ( ( 3 * width ) + padding ) *  height;
			reserved1 = 0;
			reserved2 = 0;
			fwrite(&type,sizeof(short),1,bmp);
			fwrite(&size,sizeof(int),1,bmp);
			fwrite(&reserved1,sizeof(short),1,bmp);
			fwrite(&reserved1,sizeof(short),1,bmp);
			fwrite(&offset,sizeof(int),1,bmp);
			fwrite(&biSize,sizeof(int),1,bmp);
			fwrite(&biWidth,sizeof(int),1,bmp);
			fwrite(&biHeight,sizeof(int),1,bmp);
			fwrite(&biPlanes,sizeof(short),1,bmp);
			fwrite(&biBitCount,sizeof(short),1,bmp);
			fwrite(&biCompression,sizeof(int),1,bmp);
			fwrite(&biSizeImage,sizeof(int),1,bmp);
			fwrite(&biXPelsPerMeter,sizeof(int),1,bmp);
			fwrite(&biYPelsPerMeter,sizeof(int),1,bmp);
			fwrite(&biClrUsed,sizeof(int),1,bmp);
			fwrite(&biClrImportant,sizeof(int),1,bmp);

			unsigned char zeroValue=0x00;
			//if(height%4 ==0) ++height;
			for ( i = height-1; i >=0 ; --i )
			{
				for ( j = 0; j < width ; ++j )
				{

				  fwrite(&m_pixels[(3*((i*m_width)+j))+2],sizeof(char),1,bmp);
				  fwrite(&m_pixels[(3*((i*m_width)+j))+1],sizeof(char),1,bmp);
				  fwrite(&m_pixels[(3*((i*m_width)+j))],sizeof(char),1,bmp);

				}


				for ( int k = 0; k < padding; k++ )
				{

				 fwrite(&zeroValue,sizeof(char),1,bmp);
				}
			}

			fclose(bmp);
		LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		  " Exiting: LTKImageWriter::drawBMPImage()" << endl;
	}
