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
 * $LastChangedDate: 2009-07-01 20:12:22 +0530 (Wed, 01 Jul 2009) $
 * $Revision: 784 $
 * $Author: mnab $
 *
 ************************************************************************/
#include "PointFloatShapeFeature.h"
#include "LTKStringUtil.h"
#include <sstream>

const string PointFloatShapeFeature::m_data_delimiter = ",";

/**********************************************************************************
* AUTHOR		: Saravanan R.
* DATE			: 15-Mar-2007
* NAME			: Default Constructor
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
PointFloatShapeFeature::PointFloatShapeFeature()
{
}

/******************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 10-Dec-2007
* NAME			: Parameterized Constructor
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/
PointFloatShapeFeature::PointFloatShapeFeature(float inX, float inY, 
                          float inSinTheta, float inCosTheta, bool inPenUp):
m_x(inX),
m_y(inY),
m_sinTheta(inSinTheta),
m_cosTheta(inCosTheta),
m_penUp(inPenUp)
{
}

/*****************************************************************************
* AUTHOR		: Saravanan R.
* DATE			: 15-Mar-2007
* NAME			: Default Constructor
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
******************************************************************************/
PointFloatShapeFeature::~PointFloatShapeFeature()
{
}

/**********************************************************************************
* AUTHOR		: Saravanan R.
* DATE			: 15-Mar-2007
* NAME			: getX
* DESCRIPTION	: 
* ARGUMENTS		: none
* RETURNS		: float : x value
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
float PointFloatShapeFeature::getX() const
{
	return m_x;
}

/**********************************************************************************
* AUTHOR		: Saravanan R.
* DATE			:15-Mar-2007
* NAME			: getY
* DESCRIPTION	: 
* ARGUMENTS		: none
* RETURNS		: float : y value
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
float PointFloatShapeFeature::getY() const
{
	return m_y;
}

/**********************************************************************************
* AUTHOR		: Naveen Sundar G.
* DATE			:13-Sept-2007
* NAME			: getSinTheta
* DESCRIPTION	: 
* ARGUMENTS		: none
* RETURNS		: float : sintheta value
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
float PointFloatShapeFeature::getSinTheta() const
{
	return m_sinTheta;
}

/**********************************************************************************
* AUTHOR		: Naveen Sundar G.
* DATE			:13-Sept-2007
* NAME			: getCosTheta
* DESCRIPTION	: 
* ARGUMENTS		: none
* RETURNS		: float : costheta value
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
float PointFloatShapeFeature::getCosTheta() const
{
	return m_cosTheta;
}

/***************************************************************************
* AUTHOR		: Naveen Sundar G.
* DATE			:13-Sept-2007
* NAME			: isPenUp
* DESCRIPTION	: 
* ARGUMENTS		: none
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
* Balaji MNA        01-July-2009        Rename getPenUp to isPenUp
******************************************************************************/
bool PointFloatShapeFeature::isPenUp() const
{
	return m_penUp;
}

/**********************************************************************************
* AUTHOR		: Saravanan R.
* DATE			: 15-Mar-2007
* NAME			: setX
* DESCRIPTION	: 
* ARGUMENTS		: float : x value
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
void PointFloatShapeFeature::setX(float x)
{
	m_x = x;
}

/**********************************************************************************
* AUTHOR		: Saravanan R.
* DATE			: 15-Mar-2007
* NAME			: setY
* DESCRIPTION	: 
* ARGUMENTS		: float : y value
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
void PointFloatShapeFeature::setY(float y)
{
	m_y = y;
}
/**********************************************************************************
* AUTHOR		: Naveen Sundar G.
* DATE			: 13-Sept_2007
* NAME			: setSinTheta
* DESCRIPTION	: 
* ARGUMENTS		: float : sintheta value
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
void PointFloatShapeFeature::setSinTheta(float sintheta)
{
	m_sinTheta = sintheta;
}

/**********************************************************************************
* AUTHOR		: Naveen Sundar G.
* DATE			: 13-Sept_2007
* NAME			: setTheta
* DESCRIPTION	: 
* ARGUMENTS		: float : costheta value
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*************************************************************************************/
void PointFloatShapeFeature::setCosTheta(float costheta)
{
	m_cosTheta = costheta;
}

/**************************************************************************
* AUTHOR		: Naveen Sundar G.
* DATE			: 13-Sept_2007
* NAME			: setPenUp
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description
*****************************************************************************/
void PointFloatShapeFeature::setPenUp(bool penUp)
{
	m_penUp = penUp;
}

/**********************************************************************************
* AUTHOR		: Saravanan R.
* DATE			: 15-Mar-2007
* NAME			: clone
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author:  Naveen Sundar G.			Date: 13-Sept-2007				Description: Implemented Support for theta
*************************************************************************************/
LTKShapeFeaturePtr PointFloatShapeFeature::clone() const
{
	PointFloatShapeFeature* pointInst = new PointFloatShapeFeature();

	pointInst->setX(this->getX());
	pointInst->setY(this->getY());
	pointInst->setSinTheta(this->getSinTheta());
	pointInst->setCosTheta(this->getCosTheta());
	pointInst->setPenUp(this->isPenUp());
	
	return (LTKShapeFeaturePtr)pointInst;
}

/**********************************************************************************
* AUTHOR		: Saravanan R.
* DATE			: 15-Mar-2007
* NAME			: getDistance
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author:  Naveen Sundar G.			Date: 13-Sept-2007				Description: Implemented Support for theta
*************************************************************************************/
void PointFloatShapeFeature::getDistance(const LTKShapeFeaturePtr& shapeFeaturePtr, 
                                            float& outDistance) const
{
	PointFloatShapeFeature *inPointFloatFeature = (PointFloatShapeFeature*)(shapeFeaturePtr.operator ->());
	
	float xDiff = (m_x) - inPointFloatFeature->m_x;

    float yDiff = (m_y) - inPointFloatFeature->m_y;

    float sinthetaDiff = (m_sinTheta) - inPointFloatFeature->m_sinTheta;

    float costhetaDiff = (m_cosTheta) - inPointFloatFeature->m_cosTheta;
    
    outDistance = ( (xDiff * xDiff) + (yDiff * yDiff) + 
                  (sinthetaDiff * sinthetaDiff) + (costhetaDiff * costhetaDiff) );
}

/**********************************************************************************
* AUTHOR		: Saravanan R.
* DATE			: 15-Mar-2007
* NAME			: initialize
* DESCRIPTION	: To convert the string to xy value
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author:  Naveen Sundar G.			Date: 13-Sept-2007				Description: Implemented Support for theta
*************************************************************************************/
int PointFloatShapeFeature::initialize(const string& initString)
{	
	stringVector tokens;

	LTKStringUtil::tokenizeString(initString, m_data_delimiter, tokens);

	//Token size must be 4
//	if(tokens.size() != 4)
	if(tokens.size() != 5)
		 return FAILURE; //Returning an error

    m_x = LTKStringUtil::convertStringToFloat(tokens[0]);
    m_y = LTKStringUtil::convertStringToFloat(tokens[1]);
    m_sinTheta = LTKStringUtil::convertStringToFloat(tokens[2]);
    m_cosTheta = LTKStringUtil::convertStringToFloat(tokens[3]);

	if(atoi(tokens[4].c_str()) == 1)
    {   
		m_penUp = true;
    }
	else
    {   
		m_penUp = false;
    }
	
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Saravanan R.
* DATE			: 15-Mar-2007
* NAME			: toString
* DESCRIPTION	: To convert the points to a string
* ARGUMENTS		: string : holds the string value of xy points
* RETURNS		: none
* NOTES			:
* CHANGE HISTROY
* Author:  Naveen Sundar G.			Date: 13-Sept-2007				Description: Implemented Support for theta
*************************************************************************************/
void PointFloatShapeFeature::toString(string& strFeat) const
{	
	ostringstream tempString;

    tempString << m_x << m_data_delimiter << m_y << m_data_delimiter << 
                  m_sinTheta << m_data_delimiter << m_cosTheta << 
                  m_data_delimiter << m_penUp;

    strFeat = tempString.str();
}

/**********************************************************************************
* AUTHOR		: Saravanan R.
* DATE			: 30-Mar-2007
* NAME			: addFeature
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author:  Naveen Sundar G.			Date: 13-Sept-2007				Description: Implemented Support for theta
*************************************************************************************/
int PointFloatShapeFeature::addFeature(const LTKShapeFeaturePtr& secondFeature, 
                                          LTKShapeFeaturePtr& outResult ) const
{
	PointFloatShapeFeature* resultFeature = new PointFloatShapeFeature();

    PointFloatShapeFeature *inFeature = (PointFloatShapeFeature*)(secondFeature.operator ->());
    
	resultFeature->setX(m_x + inFeature->getX());
	resultFeature->setY (m_y + inFeature->getY());
	resultFeature->setSinTheta (m_sinTheta + inFeature->getSinTheta());
	resultFeature->setCosTheta (m_cosTheta + inFeature->getCosTheta());
	resultFeature->setPenUp (m_penUp);

    outResult = LTKShapeFeaturePtr(resultFeature);

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Saravanan R.
* DATE			: 30-Mar-2007
* NAME			: subtractFeature
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author:  Naveen Sundar G.			Date: 13-Sept-2007				Description: Implemented Support for theta
*************************************************************************************/
int PointFloatShapeFeature::subtractFeature(const LTKShapeFeaturePtr& secondFeature, 
                                           LTKShapeFeaturePtr& outResult ) const 
{
	PointFloatShapeFeature* resultFeature=new PointFloatShapeFeature();

    PointFloatShapeFeature *inFeature = (PointFloatShapeFeature*)(secondFeature.operator ->());

	resultFeature->setX (m_x - inFeature->getX());
	resultFeature->setY (m_y - inFeature->getY());
	resultFeature->setSinTheta (m_sinTheta - inFeature->getSinTheta());
	resultFeature->setCosTheta (m_cosTheta - inFeature->getCosTheta());
	resultFeature->setPenUp (m_penUp);

	outResult = LTKShapeFeaturePtr(resultFeature);

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR		: Saravanan R.
* DATE			: 30-Mar-2007
* NAME			: scaleFeature
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author:  Naveen Sundar G.			Date: 13-Sept-2007				Description: Implemented Support for theta
*************************************************************************************/
int PointFloatShapeFeature::scaleFeature(float alpha, LTKShapeFeaturePtr& outResult) const
{
	PointFloatShapeFeature* resultFeature=new PointFloatShapeFeature();

	resultFeature->setX (m_x * alpha);
	resultFeature->setY (m_y * alpha);
	resultFeature->setSinTheta (m_sinTheta * alpha);
	resultFeature->setCosTheta (m_cosTheta * alpha);
	resultFeature->setPenUp(m_penUp);

	outResult = LTKShapeFeaturePtr(resultFeature);

	return SUCCESS;
}

/***************************************************************************
* AUTHOR		: Saravanan R.
* DATE			: 30-Mar-2007
* NAME			: scaleFeature
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author:  Naveen Sundar G.			Date: 13-Sept-2007		Description: 
                                                                                      Implemented Support for theta
*****************************************************************************/
int PointFloatShapeFeature::toFloatVector(floatVector& floatVec)
{
	floatVec.push_back(m_x);
	floatVec.push_back(m_y);
	floatVec.push_back(m_sinTheta);
	floatVec.push_back(m_cosTheta);
	if(m_penUp == true)
		floatVec.push_back(1.0);
	else
		floatVec.push_back(0.0);

	return SUCCESS;
}

/***************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 23-Apr-2008
* NAME			: initialize
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author:  			Date		            Description: 
                                                                                      
*****************************************************************************/
int PointFloatShapeFeature::initialize(const floatVector& initFloatVector)
{
    return initialize(initFloatVector.data(), initFloatVector.size());
}

int PointFloatShapeFeature::initialize(floatVector::const_pointer initFloatData, size_t dataSize)
{
    if (dataSize < 5)
    {
        return FAILURE;
    }
    
    m_x = *(initFloatData++);
	m_y = *(initFloatData++);
	m_sinTheta = *(initFloatData++);
	m_cosTheta = *(initFloatData++);
	m_penUp = *(initFloatData++) != 0;
	
	return SUCCESS;
}

/***************************************************************************
* AUTHOR		: Nidhi Sharma
* DATE			: 23-Apr-2008
* NAME			: getFeatureDimension
* DESCRIPTION	: 
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author:  			Date		            Description: 
                                                                                      
*****************************************************************************/

int PointFloatShapeFeature::getFeatureDimension()
{
    return 5;
}
