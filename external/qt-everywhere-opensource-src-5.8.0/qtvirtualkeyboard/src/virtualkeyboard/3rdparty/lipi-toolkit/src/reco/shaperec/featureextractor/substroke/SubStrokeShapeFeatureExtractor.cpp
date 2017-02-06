/*****************************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in
* the Software without restriction, including without limitation the rights to use,
* copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the
* Software, and to permit persons to whom the Software is furnished to do so,
* subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all 
* copies or substantial portions of the Software.
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
 * $LastChangedDate:  $
 * $Revision:  $
 * $Author:  $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Implementation for SubStroke feature extractor module
 *
 * CONTENTS:
 *
 * AUTHOR:     Tanmay Mondal
 *
 * DATE:       February 2009
 * CHANGE HISTORY:
 * Author      Date           Description of change
 ************************************************************************/

#include "SubStrokeShapeFeatureExtractor.h"
#include "SubStrokeShapeFeature.h"
#include "LTKTraceGroup.h"
#include "LTKTrace.h"
#include "LTKChannel.h"
#include "LTKTraceFormat.h"
#include "LTKConfigFileReader.h"
#include "LTKMacros.h"
#include "LTKPreprocDefaults.h"
#include "LTKException.h"
#include "LTKErrors.h"
#include "LTKLoggerUtil.h"


/******************************************************************************
 * AUTHOR	: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE		: Feb-2009
 * NAME		: SubStrokeShapeFeatureExtractor
 * DESCRIPTION	: 
 * ARGUMENTS	: 
 * RETURNS	:
 * NOTES	: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

SubStrokeShapeFeatureExtractor::SubStrokeShapeFeatureExtractor(const LTKControlInfo& controlInfo)                                            
{
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "SubStrokeShapeFeatureExtractor::SubStrokeShapeFeatureExtractor()" << endl;

	string cfgFilePath = "";
    
	// Config file
	if ( ! ((controlInfo.lipiRoot).empty()) && 
			! ((controlInfo.projectName).empty()) && 
			! ((controlInfo.profileName).empty()) &&
			! ((controlInfo.cfgFileName).empty()))
	{
		cfgFilePath = (controlInfo.lipiRoot) + PROJECTS_PATH_STRING +  
			(controlInfo.projectName) + PROFILE_PATH_STRING +
			(controlInfo.profileName) + SEPARATOR + 
			(controlInfo.cfgFileName) + CONFIGFILEEXT;
	}
	else if ( ! ((controlInfo.cfgFilePath).empty() ))
	{
		cfgFilePath = controlInfo.cfgFilePath;
	}
	else
	{
		throw LTKException(EINVALID_PROJECT_NAME);
	}

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "SubStrokeShapeFeatureExtractor::SubStrokeShapeFeatureExtractor()" << endl;
}

/******************************************************************************
 * AUTHOR	: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: getShapeFeatureInstance
 * DESCRIPTION	: 
 * ARGUMENTS	: 
 * RETURNS		:
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

LTKShapeFeaturePtr SubStrokeShapeFeatureExtractor::getShapeFeatureInstance()
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "SubStrokeShapeFeatureExtractor::getShapeFeatureInstance()" << endl;

	LTKShapeFeaturePtr tempPtr(new SubStrokeShapeFeature);	

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "SubStrokeShapeFeatureExtractor::getShapeFeatureInstance()" << endl;

	return tempPtr;
}


/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: extractFeatures
 * DESCRIPTION	: calculate the feature of the Ink file
 * ARGUMENTS	: 
 * RETURNS		: int
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

int SubStrokeShapeFeatureExtractor::extractFeatures(const LTKTraceGroup& inTraceGroup,
								vector<LTKShapeFeaturePtr>& outFeatureVec)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "SubStrokeShapeFeatureExtractor::extractFeatures()" << endl;

	int numberOfTraces = inTraceGroup.getNumTraces();

	//validating inTraceGroup
	if (numberOfTraces == 0 )
	{
        	LOG( LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << 
            EEMPTY_TRACE_GROUP << " : " << getErrorMessage(EEMPTY_TRACE_GROUP)<<
            " SubStrokeShapeFeatureExtractor::extractFeatures" <<endl;
        
        	LTKReturnError(EEMPTY_TRACE_GROUP);
        }
    
   	 SubStrokeShapeFeature *featurePtr = NULL;

	int numberOfSubstrokes = 0;			// number of substrokes from a trace group

	int slopeIndex = 0;				// index of the point

	int totalNumberOfSlopeValues = 0;		// total number of slope values  for input tracegroup

	int lengthIndex=0;				// index of the substroke length vector

	int cgIndex = 0;				// index of the center of gravity vector

	float maxX=0.0,maxY=0.0,minX=0.0,minY=0.0;

	float subStokeLength = 0.0;			// length of one substroke

	float centerOfGravityX = 0.0;			// x component of center of gravity of one substroke

	float centerOfGravityY = 0.0;			// y component of center of gravity of one substroke

	vector<float> subStrokeSlopeVector;		// slope values obtained from all the substrokes

	vector<float> subStrokeLengthVector;		// lengths of all substrokes

	vector<float> subStrokeCenterOfGravityVector;	// center of gravity of all substrokes

	vector<float> tempSlope;			// slope values of one substroke							

	vector<struct subStrokePoint> subStrokeVec;	// store all the sub-strokes
	
	// extract the substrokes from input trace group, and populate subStrokeVec
	int errorCode = extractSubStrokesFromInk(inTraceGroup, subStrokeVec);

	if( errorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
               " SubStrokeShapeFeatureExtractor::extractFeatures" <<endl;

		LTKReturnError(errorCode);
	}

	// compute the features from each substroke in subStrokeVec
	errorCode = extractFeaturesFromSubStroke(subStrokeVec,subStrokeSlopeVector,subStrokeLengthVector,subStrokeCenterOfGravityVector);

	if(errorCode != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
               " SubStrokeShapeFeatureExtractor::extractFeatures" <<endl;

		LTKReturnError(errorCode);
	}

	inTraceGroup.getBoundingBox(minX,minY,maxX,maxY);

	//computing size of subStrokeSlopeVector
	totalNumberOfSlopeValues = subStrokeSlopeVector.size();

	if(totalNumberOfSlopeValues == 0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << 
			EEMPTY_SLOPE_VECTOR << " : " << getErrorMessage(EEMPTY_SLOPE_VECTOR)<<
			" SubStrokeShapeFeatureExtractor::extractFeatures" <<endl;

		LTKReturnError(EEMPTY_SLOPE_VECTOR);
	}


	//populating outFeatureVec with the features computed 	
	for(slopeIndex=0; slopeIndex < totalNumberOfSlopeValues; ++slopeIndex)
	{
		if(subStrokeSlopeVector.at(slopeIndex) == SUBSTROKES_ANGLE_DELIMITER)
		{
			if(tempSlope.size() != NUMBER_OF_SLOPE)
			{
				LOG( LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << 
					EINVALID_SLOPE_VECTOR_DIMENSION << " : " << getErrorMessage(EINVALID_SLOPE_VECTOR_DIMENSION)<<
					" SubStrokeShapeFeatureExtractor::extractFeatures" <<endl;

				return FAILURE;	
			}

			centerOfGravityX = ((subStrokeCenterOfGravityVector.at(cgIndex) / (maxX - minX)) * 100.0);

			centerOfGravityY = ((subStrokeCenterOfGravityVector.at((cgIndex + 1)) / (maxY - minY)) * 100.0);

			subStokeLength = ((subStrokeLengthVector.at(lengthIndex) / (maxY - minY)) * 100.0);

			featurePtr = new SubStrokeShapeFeature(tempSlope,centerOfGravityX ,centerOfGravityY,subStokeLength);

			//***FEATURE VECTOR***

			outFeatureVec.push_back(LTKShapeFeaturePtr(featurePtr));

			featurePtr = NULL;

			++numberOfSubstrokes;
			
			tempSlope.clear();

			++lengthIndex;

			cgIndex += 2;

			continue;
		}
		tempSlope.push_back(subStrokeSlopeVector.at(slopeIndex));
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "SubStrokeShapeFeatureExtractor::extractFeatures()" << endl;
	
	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: computeSlope
 * DESCRIPTION	: calculate slope of the line joining given two points
 * ARGUMENTS	: 
 * RETURNS		: float
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

int SubStrokeShapeFeatureExtractor::computeSlope(const float inDeltaX, const float inDeltaY, float& outSlope)
{

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "SubStrokeShapeFeatureExtractor::computeSlope()" << endl;

	outSlope = -1.0;

	if(inDeltaX == 0.0)
	{
		if(inDeltaY > 0.0)
		{
			outSlope = 90.0;
		}
		if(inDeltaY < 0.0)
		{
			outSlope = 270.0;
		}
		if(inDeltaY == 0.0)
		{
			outSlope = 0.0;
		}
	} 
	
	if(inDeltaX > 0.0)
	{
		outSlope = (((atan(inDeltaY/inDeltaX)) * PI_DEGREE) / PI);
		
		if(outSlope < 0.0)
		{
			outSlope += (2 * PI_DEGREE);
		}
	}
	
	if(inDeltaX < 0.0)
	{
		outSlope = (((atan(inDeltaY/inDeltaX)) * PI_DEGREE) / PI);
		
		outSlope += PI_DEGREE;
	} 

	if(outSlope < 0.0)
	{
			LOG( LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << 
					EINVALID_SLOPE << " : " << getErrorMessage(EINVALID_SLOPE)<<
						" SubStrokeShapeFeatureExtractor::computeSlope" <<endl;

			LTKReturnError(EINVALID_SLOPE);
	}
	

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "SubStrokeShapeFeatureExtractor::computeSlope()" << endl;

	return SUCCESS;	
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: extractSubStrokesFromInk
 * DESCRIPTION	: extract substroke from the Ink
 * ARGUMENTS	: 
 * RETURNS		: int
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

int SubStrokeShapeFeatureExtractor::extractSubStrokesFromInk(const LTKTraceGroup& inTraceGroup, vector<struct subStrokePoint>& outSubStrokeVector)
{

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "SubStrokeShapeFeatureExtractor::extractSubStrokesFromInk()" << endl;

	int numberOfTraces = inTraceGroup.getNumTraces();

    if (numberOfTraces == 0 )
    {
        LOG( LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << 
            EEMPTY_TRACE_GROUP << " : " << getErrorMessage(EEMPTY_TRACE_GROUP)<<
            " SubStrokeShapeFeatureExtractor::extractSubStrokesFromInk" <<endl;
        
        LTKReturnError(EEMPTY_TRACE_GROUP);
    }

	int errorCode = -1;              

	int dimension = 0;				// stores the number of points in each trace of the inTraceGroup
	
	int landMarkPoint = 0;				// first point of each sub-stroke

	float landMarkSlope = 0.0;			// slope of the line joining the land mark point and the following point

	float nextSlope = 0.0;				// slope of the line joining the consicutive two points

	struct subStrokePoint tempSubStroke;		// stores sub-stroke

	vector<struct subStrokePoint> subStrokeVec;	// stores all the sub-strokes

	vector<float> slopeVector;			// stores the angle made with x-axis for each point of trace

	bool segment;					// true if stroke segment into substroke

	LTKTraceVector allTraces = inTraceGroup.getAllTraces();

	LTKTraceVector::iterator traceIter = allTraces.begin();
	LTKTraceVector::iterator traceEnd = allTraces.end();

	// Segmenting inTraceGroup into substrokes
	for (; traceIter != traceEnd ; ++traceIter)
	{
		floatVector tempxVec, tempyVec;

		//compute all the slope values from points on the trace
		if( (errorCode = getSlopeFromTrace(*traceIter,slopeVector)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
                    " SubStrokeShapeFeatureExtractor::extractSubStrokesFromInk" <<endl;

			LTKReturnError(errorCode);
		}
		
		// computing total number of points present in a trace
		dimension = (*traceIter).getNumberOfPoints();
		
		// Validating the slopeVector, its size should be equal to one less than number of points in trace  
		if((dimension-1) != slopeVector.size())
		{
			LOG( LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << 
            EINVALID_SLOPE_VECTOR_DIMENSION << " : " << getErrorMessage(EINVALID_SLOPE_VECTOR_DIMENSION)<<
            " SubStrokeShapeFeatureExtractor::extractSubStrokesFromInk" <<endl;

			LTKReturnError(EINVALID_SLOPE_VECTOR_DIMENSION);
		}
		
		(*traceIter).getChannelValues("X", tempxVec);

		(*traceIter).getChannelValues("Y", tempyVec);

		// initialised landmark point for next stroke, assume that the first point is 
		// landmark point for every stroke
		landMarkPoint = 0;


		int pointIndex = 0;
		for( pointIndex = 0; pointIndex < (dimension - 1); ++pointIndex)
		{
			
			//for the first point in the trace, landMarkSlope and
			//nextSlope will be the same
			landMarkSlope = slopeVector[landMarkPoint];

			nextSlope = slopeVector[pointIndex];

			//check of the trace can be segmented at the current point
			if( (errorCode = canSegmentStrokes(landMarkSlope,nextSlope, segment)) != SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
                    " SubStrokeShapeFeatureExtractor::extractSubStrokesFromInk" <<endl;

				LTKReturnError(errorCode);
			}
							
			if( segment )
			{
				// Segment the stroke into substroke
				outSubStrokeVector[outSubStrokeVector.size() - 1].penUp = true;
				 
				// set new landmark point for next substrokes of stroke
				landMarkPoint = pointIndex;

			} // end if ( segment )
			
			//substrokes continue
			tempSubStroke.X = tempxVec[pointIndex];
			tempSubStroke.Y = tempyVec[pointIndex];
			tempSubStroke.penUp = false;

			outSubStrokeVector.push_back(tempSubStroke);


		} // end for
		
		// terminal point of a stroke
		tempSubStroke.X = tempxVec[pointIndex];
		tempSubStroke.Y = tempyVec[pointIndex];
		tempSubStroke.penUp = true;

		outSubStrokeVector.push_back(tempSubStroke);

	} // end traceItr

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "SubStrokeShapeFeatureExtractor::extractSubStrokesFromInk()" << endl;

	
	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: extractFeatureFromSubStroke
 * DESCRIPTION	: 
 * ARGUMENTS	: extract feature vector of dimension (NUMBER_OF_THETA+3) from a given substroke
 * RETURNS		: int
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

 int SubStrokeShapeFeatureExtractor::extractFeaturesFromSubStroke(const vector<struct subStrokePoint>& inSubStrokeVector, vector<float>& outSlope, vector<float>& outLength, vector<float>& outCenterOfGravity)
{
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "SubStrokeShapeFeatureExtractor::extractFeatureFromSubStroke()" << endl;

	int numSubStrokesPoints = 0;			

	//total number of points present in all substrokes
	numSubStrokesPoints = inSubStrokeVector.size();

	//validating inSubStrokeVector
	if( numSubStrokesPoints <= 0 )
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << 
            ENO_SUBSTROKE << " : " << getErrorMessage(ENO_SUBSTROKE)<<
            " SubStrokeShapeFeatureExtractor::extractFeaturesFromSubStroke" <<endl;

		LTKReturnError(ENO_SUBSTROKE);
	}

	int ptIndex = 0;							// index of point in a trace

	int pointIndex = 0;							// counter variable to loop over the points in a trace

	int currentPointIndex =0;						// index of the current point

	int startIndxOfSubStrokes = 0;						// starting index of the substroke

	float x = 0.0;								// value of x-coordinate

	float y = 0.0;								// value of y-coordinate

	float slope = 0.0;							// store slope value
 
	float xDiff = 0.0;							// difference in x-direction

	float yDiff = 0.0;							// difference in y-direction

	float xTemp = 0.0;							// temporary variable for x-coordinate

	float yTemp = 0.0;							// temporary variable for y-coordinate

	float unitLength = 0.0;							// length of each substroke after resampling
	
	float pointDistance = 0.0;						// distance detween two consecutive point

	float residualDistance = 0.0;						// distance between the last resampled point and the next original sample point

	float cumulativeDistance = 0.0;						// sum of distances between consecutive points

	float segmentRatioLeft = 0.0, segmentRatioRight = 0.0; 			// ratio for segmenting the line joining two points

	float dx = 0.0, dy = 0.0;
	
	floatVector distanceVec;						// vector to store distances between points 

	struct subStrokePoint tempStroke;

	//vector to store all the substrokes
	vector<struct subStrokePoint> subStrokeVec;

	//*************************************************************************//
	//  represent each extracted substroke from ink by six equidistant points
	//*************************************************************************//
	for(int ptIdx = 0; ptIdx < numSubStrokesPoints; ++ptIdx)
	{
		if(inSubStrokeVector[ptIdx].penUp)
		{
			// ignore substrokes whose length are less than thresold value
			if(unitLength < SUBSTROKES_LENGTH_REJECT_THRESHOLD)
			{
				// initialised for next substroke
				unitLength = 0.0;

				residualDistance = 0.0;

				ptIndex = 0;

				distanceVec.clear();
				
				//starting index of the next substroke
				startIndxOfSubStrokes = ptIdx+1;

				continue;
			}
			
			// length of the extracted sub-strokes(curve length)
			outLength.push_back(unitLength);

			// dividing to get NUMBER_OF_SLOPE number of points on each substroke
			unitLength /= (NUMBER_OF_SLOPE);
			
			// adding x of first point
			x = inSubStrokeVector[startIndxOfSubStrokes].X; 
			
			// adding y of first point
			y = inSubStrokeVector[startIndxOfSubStrokes].Y;

			tempStroke.X = x;
			tempStroke.Y = y;
			tempStroke.penUp = false;
			
			// pushing back the first point to subStrokeVec
			subStrokeVec.push_back(tempStroke);
			
			// Genareting four equidistant points
			for(pointIndex = 1; pointIndex < (NUMBER_OF_SLOPE); ++pointIndex)
			{
				cumulativeDistance = residualDistance;

				while(cumulativeDistance < unitLength)
				{
					cumulativeDistance += distanceVec.at(ptIndex++);

					if(ptIndex == 1)
					{
						currentPointIndex = (startIndxOfSubStrokes + ptIndex);
					}
					else
					{
						currentPointIndex++;
					}
				}

				if(ptIndex < 1) ptIndex = 1;

				segmentRatioRight = cumulativeDistance - unitLength;

				segmentRatioLeft = distanceVec.at(ptIndex -1) - segmentRatioRight;
				
				//interpolating to get equidistant points
				if( fabs(segmentRatioLeft+segmentRatioRight) > EPS)
				{
					xTemp =  (segmentRatioLeft* inSubStrokeVector[currentPointIndex].X + segmentRatioRight* inSubStrokeVector[currentPointIndex - 1].X)/(segmentRatioLeft+segmentRatioRight);

					yTemp =  (segmentRatioLeft* inSubStrokeVector[currentPointIndex].Y + segmentRatioRight* inSubStrokeVector[currentPointIndex - 1].Y)/(segmentRatioLeft+segmentRatioRight);
				}
				else
				{
					xTemp = inSubStrokeVector[currentPointIndex].X;

					yTemp = inSubStrokeVector[currentPointIndex].Y;
				}

				tempStroke.X = xTemp;
				tempStroke.Y = yTemp;
				tempStroke.penUp = false;

				subStrokeVec.push_back(tempStroke);

				residualDistance = segmentRatioRight;
			}
			
			// adding x of the last point of the substroke
			x = inSubStrokeVector[ptIdx].X;
			
			// adding y of the last point of the substroke
			y = inSubStrokeVector[ptIdx].Y;

			tempStroke.X = x;
			tempStroke.Y = y;
			tempStroke.penUp = true;

			subStrokeVec.push_back(tempStroke);
			
			// initialised for next substroke
			unitLength = 0.0;

			residualDistance = 0.0;

			ptIndex = 0;

			distanceVec.clear();
			
			//starting index of the next substroke
			startIndxOfSubStrokes = ptIdx+1;
		}
		else
		{
			
			xDiff = (inSubStrokeVector[ptIdx].X - inSubStrokeVector[ptIdx + 1].X);

			yDiff = (inSubStrokeVector[ptIdx].Y - inSubStrokeVector[ptIdx + 1].Y);
			
			//distance between points.
			pointDistance = (float) (sqrt(xDiff*xDiff + yDiff*yDiff));
			
			// finding the length of a substroke.
			unitLength += pointDistance;
			
			//storing distances between every two consecutive points in a substroke.
			distanceVec.push_back(pointDistance);
		}
	}

	
	//************************************************************************************************//
	// compute the feature vector for each substroke after representing them by six equidistant points
	//************************************************************************************************//
	
	// total number of points in all substrokes after genarating equidistant points
	numSubStrokesPoints = 0;

	numSubStrokesPoints = subStrokeVec.size();

	if( numSubStrokesPoints <= 0 )
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << 
            ENO_SUBSTROKE << " : " << getErrorMessage(ENO_SUBSTROKE)<<
            " SubStrokeShapeFeatureExtractor::extractFeaturesFromSubStroke" <<endl;

		LTKReturnError(ENO_SUBSTROKE);
	}


	int errorCode = -1;

	float sumOfX = 0.0;
	float sumOfY = 0.0;

	//computing slopes and center of gravity values from the equidistant points extracted from each substroke
	for(pointIndex = 0; pointIndex < numSubStrokesPoints; ++pointIndex)
	{
		if(!subStrokeVec[pointIndex].penUp)
		{

			// Compute slope of line joining consecutive points in a substrokes
			dx = (subStrokeVec[pointIndex + 1].X - subStrokeVec[pointIndex].X);
			dy = (subStrokeVec[pointIndex + 1].Y - subStrokeVec[pointIndex].Y);

			if( (errorCode = computeSlope(dx,dy,slope)) != SUCCESS)
			{
				LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
                    " SubStrokeShapeFeatureExtractor::extractFeaturesFromSubStroke" <<endl;

				LTKReturnError(errorCode);
			}


			outSlope.push_back(slope);

			sumOfX += subStrokeVec[pointIndex].X;

			sumOfY += subStrokeVec[pointIndex].Y;

		}
		else
		{
			int div = (NUMBER_OF_SLOPE + 1);

			sumOfX += subStrokeVec[pointIndex].X;

			sumOfY += subStrokeVec[pointIndex].Y;

			sumOfX = sumOfX / (float)div;

			sumOfY = sumOfY / (float)div;

			outCenterOfGravity.push_back(sumOfX);

			outCenterOfGravity.push_back(sumOfY);

			outSlope.push_back(SUBSTROKES_ANGLE_DELIMITER);

			sumOfX = 0.0;

			sumOfY = 0.0;
			
		}
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "SubStrokeShapeFeatureExtractor::extractFeatureFromSubStroke()" << endl;

	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: getDirectionCode
 * DESCRIPTION	: quantize the slope
 * ARGUMENTS	: 
 * RETURNS		: int
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
 int SubStrokeShapeFeatureExtractor::getDirectionCode(const float inSlope, int& outDirectionCode)
{
	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "SubStrokeShapeFeatureExtractor::getDirectionCode()" << endl;

	//Validating inSlope
	if(inSlope < 0.0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << 
			EINVALID_SLOPE << " : " << getErrorMessage(EINVALID_SLOPE)<<
			" SubStrokeShapeFeatureExtractor::getDirectionCode" <<endl;

			LTKReturnError(EINVALID_SLOPE);
	}

	outDirectionCode = 0;
	
	// return the octant value as slope value

	if((inSlope< ANGLE_HIGHER_LIMIT_1) || (inSlope >= ANGLE_HIGHER_LIMIT_8))
	{
		outDirectionCode = DIRECTION_CODE_EAST;
	}

	else if((inSlope < ANGLE_HIGHER_LIMIT_2) && (inSlope >=ANGLE_HIGHER_LIMIT_1))
	{
		outDirectionCode = DIRECTION_CODE_NORTH_EAST;
	}

	else if(((inSlope >= ANGLE_HIGHER_LIMIT_2) && (inSlope < ANGLE_HIGHER_LIMIT_3)) )
	{
		outDirectionCode = DIRECTION_CODE_NORTH;
	}

	else if( (inSlope < ANGLE_HIGHER_LIMIT_4 ) && (inSlope >= ANGLE_HIGHER_LIMIT_3))
	{
		outDirectionCode = DIRECTION_CODE_NORTH_WEST;
	}

	else if((inSlope >= ANGLE_HIGHER_LIMIT_4) && (inSlope < ANGLE_HIGHER_LIMIT_5))
	{
		outDirectionCode = DIRECTION_CODE_WEST;
	}

	else if((inSlope >= ANGLE_HIGHER_LIMIT_5) && (inSlope < ANGLE_HIGHER_LIMIT_6))
	{
		outDirectionCode = DIRECTION_CODE_SOUTH_WEST;
	}

	else if(((inSlope >= ANGLE_HIGHER_LIMIT_6) && (inSlope < ANGLE_HIGHER_LIMIT_7)))
	{
		outDirectionCode = DIRECTION_CODE_SOUTH;
	}

	else if((inSlope >= ANGLE_HIGHER_LIMIT_7) && (inSlope < ANGLE_HIGHER_LIMIT_8))
	{
		outDirectionCode = DIRECTION_CODE_SOUTH_EAST;
	}

	if(outDirectionCode == 0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << 
			EINVALID_DIRECTION_CODE << " : " <<
			getErrorMessage(EINVALID_DIRECTION_CODE)<<
			" SubStrokeShapeFeatureExtractor::getDirectionCode" <<endl;

		LTKReturnError(EINVALID_DIRECTION_CODE);;
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "SubStrokeShapeFeatureExtractor::getDirectionCode()" << endl;

	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: canSegmentStrokes
 * DESCRIPTION	: decision is taken towards dividing a stroke into substroke
 * ARGUMENTS	: 
 * RETURNS		: int
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int SubStrokeShapeFeatureExtractor::canSegmentStrokes(const float inFirstSlope, const float inSecondSlope, bool& outSegment)
{

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "SubStrokeShapeFeatureExtractor::canSegmentStrokes()" << endl;

	//Validating slope values
	if(inFirstSlope < 0.0 || inSecondSlope < 0.0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << 
			EINVALID_SLOPE << " : " << getErrorMessage(EINVALID_SLOPE)<<
			" SubStrokeShapeFeatureExtractor::canSegmentStrokes" <<endl;

			LTKReturnError(EINVALID_SLOPE);
	}

	int directionCodeOfFirstSlope = 0;
	int directionCodeOfSecondSlope = 0;

	outSegment = false;
	
	int errorCode = -1;

	// Computing octants of hte inFirstSlope and inSecondSlope
	 if( (errorCode = getDirectionCode(inFirstSlope, directionCodeOfFirstSlope)) != SUCCESS)
	 {
		 LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " SubStrokeShapeFeatureExtractor::canSegmentStrokes" <<endl;

		LTKReturnError(errorCode);
	 }

	 if( (errorCode = getDirectionCode(inSecondSlope, directionCodeOfSecondSlope)) != SUCCESS)
	 {
		 LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " SubStrokeShapeFeatureExtractor::canSegmentStrokes" <<endl;

		LTKReturnError(errorCode);
	 }

	if(abs(directionCodeOfSecondSlope - directionCodeOfFirstSlope) <= STROKE_SEGMENT_VALUE )
		outSegment = false;
	else
		outSegment = true;

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "SubStrokeShapeFeatureExtractor::canSegmentStrokes()" << endl;

	return SUCCESS;	
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: getSlopeFromTrace
 * DESCRIPTION	: 
 * ARGUMENTS	: 
 * RETURNS		: int
 * NOTES			: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int SubStrokeShapeFeatureExtractor::getSlopeFromTrace(const LTKTrace& inTrace, vector<float>& outSlopeVector)
{

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Entering " <<
        "SubStrokeShapeFeatureExtractor::getSlopeFromTrace()" << endl;

	int dimension = 0;	//Stores the number of points in a trace

	dimension = inTrace.getNumberOfPoints();

	if(dimension == 0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << "Error: " << 
			EEMPTY_TRACE << " : " << getErrorMessage(EEMPTY_TRACE)<<
			" SubStrokeShapeFeatureExtractor::getSlopeFromTrace" <<endl;

		LTKReturnError(EEMPTY_TRACE);
	}

	int pointIndex = 0;
	
	float dx = 0.0, dy = 0.0;	//Variables to store differences in x and y-directions to compute slope

	float slope = 0.0;

	floatVector xVec, yVec;

	int errorCode = -1;

	if( (errorCode = inTrace.getChannelValues(X_CHANNEL_NAME, xVec))!= SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		<<"Error: SubStrokeShapeFeatureExtractor::getSlopeFromTrace"<<endl;

		LTKReturnError(errorCode);
	}

	if( (errorCode = inTrace.getChannelValues(Y_CHANNEL_NAME, yVec)) != SUCCESS)
	{
		LOG(LTKLogger::LTK_LOGLEVEL_ERR)
		<<"Error: SubStrokeShapeFeatureExtractor::getSlopeFromTrace"<<endl;

		LTKReturnError(errorCode);
	}

	outSlopeVector.clear();

	for(pointIndex=0; pointIndex < dimension-1; ++pointIndex)
	{
		dx = xVec[pointIndex+1] - xVec[pointIndex];

		dy = yVec[pointIndex+1] - yVec[pointIndex];

		if( (errorCode = computeSlope(dx, dy, slope)) != SUCCESS)
		{
			LOG(LTKLogger::LTK_LOGLEVEL_ERR)<<"Error: "<< errorCode << " " <<
            " SubStrokeShapeFeatureExtractor::getSlopeFromTrace" <<endl;

			LTKReturnError(errorCode);
		}

		outSlopeVector.push_back(slope);
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << "Exiting " <<
        "SubStrokeShapeFeatureExtractor::getSlopeFromTrace()" << endl;

	return SUCCESS;	
}
