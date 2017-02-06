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
 * FILE DESCR: Implementation for SubStrokeShapeFeature  module
 *
 * CONTENTS:
 *
 * AUTHOR:     Tanmay Mondal
 *
 * DATE:       February 2009
 * CHANGE HISTORY:
 * Author      Date           Description of change
 ************************************************************************/
#include "SubStrokeShapeFeature.h"
#include "LTKStringUtil.h"
#include <sstream>

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: Default Constructor
 * DESCRIPTION	: 
 * ARGUMENTS	: 
 * RETURNS		:
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

SubStrokeShapeFeature::SubStrokeShapeFeature():
m_data_delimiter(",")
{
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: Parameterized Constructor
 * DESCRIPTION	: 
 * ARGUMENTS	: 
 * RETURNS		:
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

SubStrokeShapeFeature::SubStrokeShapeFeature(vector<float>& inSlopeVector,float inCgX,float inCgY,float inLength):
m_slopeVector(inSlopeVector),
m_xComponentOfCenterOfGravity(inCgX),
m_yComponentOfCenterOfGravity(inCgY),
m_subStrokeLength(inLength),
m_data_delimiter(",")
{
}


/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: Default Destructor
 * DESCRIPTION	: 
 * ARGUMENTS	: 
 * RETURNS		:
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

SubStrokeShapeFeature::~SubStrokeShapeFeature()
{
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: clone
 * DESCRIPTION	: 
 * ARGUMENTS	: 
 * RETURNS		:
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

LTKShapeFeaturePtr SubStrokeShapeFeature::clone() const
{
	SubStrokeShapeFeature* pointInst = new SubStrokeShapeFeature();
	
	//set the value of the tan theta
	vector<float> tempSlopeVector;

	this->getSlopeVector(tempSlopeVector);

	pointInst->setSlopeVector(tempSlopeVector);

	//set the value of center of gravity
	pointInst->setXcomponentOfCenterOfGravity(this->getXcomponentOfCenterOfGravity());
	pointInst->setYcomponentOfCenterOfGravity(this->getYcomponentOfCenterOfGravity());

	//set the value of the length
	pointInst->setSubStrokeLength(this->getSubStrokeLength());

	return (LTKShapeFeaturePtr)pointInst;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: getDistance
 * DESCRIPTION	: 
 * ARGUMENTS	: 
 * RETURNS		: int
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

void SubStrokeShapeFeature::getDistance(const LTKShapeFeaturePtr& shapeFeaturePtr, 
                                            float& outDistance) const
{

	float xDiff = 0.0, yDiff =0.0;
	
	float sumSlopeDiff = 0.0;

	int slopeIndex = 0;

	int numSlope = 0;

	float slopeDiff = 0.0, tempSlopeDiff1 = 0.0, tempSlopeDiff2 = 0.0;
	
	float lengthDiff = 0.0;

	floatVector featureVector;

	SubStrokeShapeFeature *inSubStrokeFeature = (SubStrokeShapeFeature*)(shapeFeaturePtr.operator ->());

	inSubStrokeFeature->getSlopeVector(featureVector);

	numSlope = featureVector.size();

	// feature vector dimension for each substroke is 8 (NUMBER_OF_SLOPE + 3)
	// The constant 3 in the above corresponds to x,y coordinates of center of gravity of the substroke and its length
	
	if(numSlope  != (inSubStrokeFeature->getFeatureDimension()-3))
	{
		//write log
		return; // error
	}

	// compute the distance between the slope vectors of two instances of SubStrokeShapeFeature
	for(slopeIndex = 0; slopeIndex < numSlope; ++slopeIndex)
	{
		// difference between two slope components
		tempSlopeDiff1 = fabs((m_slopeVector[slopeIndex]) - featureVector[slopeIndex]);
		 
		tempSlopeDiff2 = fabs((360.0 - tempSlopeDiff1));
		
		// take the minimum of the above two differences to take care of the circular nature of slope function
		if(tempSlopeDiff1 > tempSlopeDiff2)
			slopeDiff = tempSlopeDiff2;
		else
			slopeDiff = tempSlopeDiff1;

		sumSlopeDiff += slopeDiff;
		
	}
	
	// compute the distance between the center of gravity of two instances of SubStrokeShapeFeature 
	xDiff = fabs((m_xComponentOfCenterOfGravity) - inSubStrokeFeature->getXcomponentOfCenterOfGravity());

	yDiff = fabs((m_yComponentOfCenterOfGravity) - inSubStrokeFeature->getYcomponentOfCenterOfGravity());

	// compute the difference between the length of two instances of SubStrokeShapeFeature 
	lengthDiff = fabs((m_subStrokeLength) - inSubStrokeFeature->getSubStrokeLength());

	outDistance	= (sumSlopeDiff + (xDiff * xDiff) + (yDiff * yDiff) + lengthDiff);
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: initialize
 * DESCRIPTION	: initialize the feature instance from string
 * ARGUMENTS	: string: initString
 * RETURNS		: int: 
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

int SubStrokeShapeFeature::initialize(const string& initString)
{	

	int index;

	stringVector tokens;

	LTKStringUtil::tokenizeString(initString, m_data_delimiter, tokens);
	
	int tokensSize = tokens.size();

	//code for converting the string feature in to value and initialised the feature vector
	if(tokensSize != getFeatureDimension())
	{
		//write log
		//invalide size of feature vector
		return FAILURE; //returning an error
	}

	for( index = 0; index < tokensSize - 3; ++index )
	{
        m_slopeVector.push_back(LTKStringUtil::convertStringToFloat(tokens[index]));
	}

    m_xComponentOfCenterOfGravity = LTKStringUtil::convertStringToFloat(tokens[index]);

    m_yComponentOfCenterOfGravity = LTKStringUtil::convertStringToFloat(tokens[index+1]);

    m_subStrokeLength = LTKStringUtil::convertStringToFloat(tokens[index+2]);

	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: initialize
 * DESCRIPTION	: initialize the feature instance
 * ARGUMENTS	: floatVector: initFloatVector
 * RETURNS		: int: 
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

int SubStrokeShapeFeature::initialize(const floatVector& initFloatVector)
{

	int index;

	int vectorSize = initFloatVector.size();

	if(vectorSize != getFeatureDimension() )
	{
		//write log
		//invalide size of feature vector
		 return FAILURE; //returning an error
	}

	for( index = 0; index < vectorSize - 3; ++index )
	{
		m_slopeVector.push_back(initFloatVector[index]);
	}

	m_xComponentOfCenterOfGravity = initFloatVector[index];

	m_yComponentOfCenterOfGravity = initFloatVector[index+1];

	m_subStrokeLength = initFloatVector[index+2];

	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: toFloatVector
 * DESCRIPTION	: To convert feature to a float vector
 * ARGUMENTS	: vector<float>: holds the floating point value of feature
 * RETURNS		: int
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

int SubStrokeShapeFeature::toFloatVector(vector<float>& outFloatVec)
{

	int slopeIndex;

	int numSlope = m_slopeVector.size();

	if(numSlope != (getFeatureDimension() - 3))
	{
		//write log
		//invalide size of angle vector
		 return FAILURE; //returning an error
	}

	for( slopeIndex = 0; slopeIndex < numSlope; ++slopeIndex)
	{
			outFloatVec.push_back(m_slopeVector[slopeIndex]);
	}

	outFloatVec.push_back(m_xComponentOfCenterOfGravity);

	outFloatVec.push_back(m_yComponentOfCenterOfGravity);

	outFloatVec.push_back(m_subStrokeLength);

	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: toString
 * DESCRIPTION	: To convert the feature to a string
 * ARGUMENTS	: string : holds the string value of feature
 * RETURNS		: NONE
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

void SubStrokeShapeFeature::toString(string& outStrFeat) const
{	

	ostringstream tempString;

	int slopeIndex;

	int numSlope = m_slopeVector.size();

	if(numSlope  != NUMBER_OF_SLOPE)
	{
		//write log
		//invalide size of angle vector
		 return; //returning an error
	}

    //convert the value of the feture vector to string
	for( slopeIndex = 0; slopeIndex < numSlope; slopeIndex++)
	{ 
		tempString << m_slopeVector[slopeIndex] << m_data_delimiter;
	}

	tempString << m_xComponentOfCenterOfGravity << m_data_delimiter
			   << m_yComponentOfCenterOfGravity << m_data_delimiter 
			   << m_subStrokeLength;


    outStrFeat = tempString.str();

}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: getSlopeVector
 * DESCRIPTION	: 
 * ARGUMENTS	: vector<float>: outSlopeVector 
 * RETURNS		: none
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

void SubStrokeShapeFeature::getSlopeVector(vector<float>& outSlopeVector) const
{
	outSlopeVector = m_slopeVector;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: getSubStrokeLength
 * DESCRIPTION	: Ratio of the length and normalised height
 * ARGUMENTS	: none
 * RETURNS		: float: m_subStrokeLength
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

float SubStrokeShapeFeature::getSubStrokeLength() const
{
	return m_subStrokeLength;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: getXcomponentOfCenterOfGravity
 * DESCRIPTION	: X co-ordinate of CG
 * ARGUMENTS	: none
 * RETURNS		: float: m_xComponentOfCenterOfGravity
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

float SubStrokeShapeFeature::getXcomponentOfCenterOfGravity() const
{
	return m_xComponentOfCenterOfGravity;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: getYcomponentOfCenterOfGravity
 * DESCRIPTION	: Y co-ordinate of CG
 * ARGUMENTS	: none
 * RETURNS		: float: m_yComponentOfCenterOfGravity
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

float SubStrokeShapeFeature::getYcomponentOfCenterOfGravity() const
{
	return m_yComponentOfCenterOfGravity;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: setSlopeVector
 * DESCRIPTION	: set the slope vector of substroke
 * ARGUMENTS	: vector<float>: inSlopeVector
 * RETURNS		: none
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

void SubStrokeShapeFeature::setSlopeVector(const vector<float>& inSlopeVector)
{
	m_slopeVector = inSlopeVector;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: setSubStrokeLength
 * DESCRIPTION	: set the length of the substroke
 * ARGUMENTS	: float: length
 * RETURNS		: none
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

void SubStrokeShapeFeature::setSubStrokeLength(float inSubStrokeLength)
{
	m_subStrokeLength = inSubStrokeLength;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: setXcomponentOfCenterOfGravity
 * DESCRIPTION	: 
 * ARGUMENTS	: float: x value
 * RETURNS		: none
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

void SubStrokeShapeFeature::setXcomponentOfCenterOfGravity(float inX)
{
	m_xComponentOfCenterOfGravity = inX;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: setYcomponentOfCenterOfGravity
 * DESCRIPTION	: 
 * ARGUMENTS	: float: y value
 * RETURNS		: none
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/

void SubStrokeShapeFeature::setYcomponentOfCenterOfGravity(float inY)
{
	m_yComponentOfCenterOfGravity = inY;	
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: addFeature
 * DESCRIPTION	: 
 * ARGUMENTS	: 
 * RETURNS		: int
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int SubStrokeShapeFeature::addFeature(const LTKShapeFeaturePtr& secondFeature, 
                                          LTKShapeFeaturePtr& outResult ) const
{
	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: subtractFeature
 * DESCRIPTION	: 
 * ARGUMENTS	: 
 * RETURNS		: int
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int SubStrokeShapeFeature::subtractFeature(const LTKShapeFeaturePtr& secondFeature, 
                                           LTKShapeFeaturePtr& outResult ) const 
{
	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: scaleFeature
 * DESCRIPTION	: 
 * ARGUMENTS	: 
 * RETURNS		: int
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int SubStrokeShapeFeature::scaleFeature(float alpha, LTKShapeFeaturePtr& outResult) const
{
	return SUCCESS;
}

/******************************************************************************
 * AUTHOR		: Tanmay Mondal
 * SUPERVISOR   : Ujjwal Bhattacharya
 * OTHER PLP    : Kalikinkar Das
 * ORGANIZATION : Indian Statistical Institute, Kolkata, India
 * DEPARTMENT   : Computer Vision and Pattern Recognition Unit
 * DATE			: Feb-2009
 * NAME			: getFeatureDimension
 * DESCRIPTION	: 
 * ARGUMENTS	: none
 * RETURNS		: int: dimension of the feature
 * NOTES		: 
 * CHANGE HISTROY
 * Author			Date				Description
 ******************************************************************************/
int SubStrokeShapeFeature::getFeatureDimension()
{
	// The constant 3 in the above corresponds to x,y coordinates of center of gravity of the substroke and its length
	return (NUMBER_OF_SLOPE + 3);
}
/**********************************************************************************
* AUTHOR        : Balaji MNA.
* DATE          : 01-July-2009
* NAME          : isPenUp
* DESCRIPTION   : Get method for the penUp
* ARGUMENTS     : none
* RETURNS       : The PenUp value
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
bool SubStrokeShapeFeature::isPenUp() const
{
	return 0;
	
}
