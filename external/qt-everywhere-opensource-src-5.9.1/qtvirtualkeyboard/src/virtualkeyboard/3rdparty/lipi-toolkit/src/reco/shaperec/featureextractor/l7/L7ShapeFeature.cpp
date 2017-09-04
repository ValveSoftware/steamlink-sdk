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
 * $LastChangedDate: 2008-02-20 10:03:51 +0530 (Wed, 20 Feb 2008) $
 * $Revision: 423 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR		: Implementation of L7ShapeFeature class		
 * AUTHOR			: Naveen Sundar G.
 * DATE				: August 30, 2005
 * CHANGE HISTORY	:
 * Author       Date            Description of change
 ************************************************************************/

#include "L7ShapeFeature.h"
#include "LTKStringUtil.h"
#include <sstream>

/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : L7ShapeFeature
* DESCRIPTION   : Constructor for the L7ShapeFeature class.
* ARGUMENTS     : none
* RETURNS       : 
* NOTES         : Initializes the string delimiter used in writing feature values to a file.
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
L7ShapeFeature::L7ShapeFeature():
m_data_delimiter(",")
{
}

/***************************************************************************
* AUTHOR           : Nidhi Sharma
* DATE               : 12-Dec-2007
* NAME               : L7ShapeFeature
* DESCRIPTION   : Parameterized Constructor for the L7ShapeFeature class.
* ARGUMENTS     : 
* RETURNS          : 
* NOTES             :
* CHANGE HISTROY
* Author            Date                Description of change
*****************************************************************************/
L7ShapeFeature::L7ShapeFeature(float inX, float inY, float inXFirstDerv, 
                                   float inYFirstDerv, float inXSecondDerv, 
                                   float inYSecondDerv, float inCurvature,
                                   bool inPenUp):
m_x(inX),
m_y(inY),
m_xFirstDerv(inXFirstDerv),
m_yFirstDerv(inYFirstDerv),
m_xSecondDerv(inXSecondDerv),
m_ySecondDerv(inYSecondDerv),
m_curvature(inCurvature),
m_penUp(inPenUp),
m_data_delimiter(",")
{
}

/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : ~L7ShapeFeature
* DESCRIPTION   : Destructor for the L7ShapeFeature class
* ARGUMENTS     : none
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
L7ShapeFeature::~L7ShapeFeature()
{

}

/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : getX
* DESCRIPTION   : Get method for the x co-ordinate value
* ARGUMENTS     : none
* RETURNS       : X co-ordinate of the shape feature
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
float L7ShapeFeature::getX() const
{
	return m_x;
}


/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : getY
* DESCRIPTION   : Get method for the y co-ordinate value
* ARGUMENTS     : none
* RETURNS       : Y co-ordinate of the shape feature
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
float L7ShapeFeature::getY() const
{
	return m_y;
}

/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : getXFirstDerv
* DESCRIPTION   : Get method for the first derivative value along the X axis
* ARGUMENTS     : none
* RETURNS       : First derivative value along the X axis. 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
float L7ShapeFeature::getXFirstDerv() const
{
	return m_xFirstDerv;
}

/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : getYFirstDerv
* DESCRIPTION   : Get method for the first derivative value along the Y axis
* ARGUMENTS     : none
* RETURNS       : First derivative value along the Y axis.
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
float L7ShapeFeature::getYFirstDerv() const
{
	return m_yFirstDerv;
}

/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : getXSecondDerv
* DESCRIPTION   : Get method for the second derivative value along the X axis.
* ARGUMENTS     : none
* RETURNS       : Second derivative value along the X axis.
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
float L7ShapeFeature::getXSecondDerv() const
{
	return m_xSecondDerv;
}

/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : getYSecondDerv
* DESCRIPTION   : Get method for the second derivative value along the Y axis.
* ARGUMENTS     : none
* RETURNS       : Second derivative value along the Y axis.
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
float L7ShapeFeature::getYSecondDerv() const
{
	return m_ySecondDerv;
}
/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : getCurvature
* DESCRIPTION   : Get method for the curvature
* ARGUMENTS     : none
* RETURNS       : The curvature value
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
float L7ShapeFeature::getCurvature() const
{
	return m_curvature;
}

/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : isPenUp
* DESCRIPTION   : Get method for the penUp
* ARGUMENTS     : none
* RETURNS       : The PenUp value
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
* Balaji MNA        01-July-2009        Rename getPenUp to isPenUp
*************************************************************************************/
bool L7ShapeFeature::isPenUp() const
{
	return m_penUp;
	
}

/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : setX
* DESCRIPTION   : Set method for the X co-ordinate value
* ARGUMENTS     : X co-ordinate value to be set
* RETURNS       : none
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
void L7ShapeFeature::setX(float x)
{
	m_x = x;
}
/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : setY
* DESCRIPTION   : Set method for the Y co-ordinate value
* ARGUMENTS     : Y co-ordinate value to be set
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
void L7ShapeFeature::setY(float y)
{
	m_y = y;
}
/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : setXFirstDerv
* DESCRIPTION   : Set method for the first derivative along the X axis
* ARGUMENTS     : The first derivative value along the X axis
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
void L7ShapeFeature::setXFirstDerv(float xFirstDerv)
{
	m_xFirstDerv = xFirstDerv;
}
/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : setYFirstDerv
* DESCRIPTION   : Set method for the first derivative along the Y axis
* ARGUMENTS     : The first derivative value along the Y axis to be set
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
void L7ShapeFeature::setYFirstDerv(float yFirstDerv)
{
	m_yFirstDerv = yFirstDerv;
}/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : setXSecondDerv
* DESCRIPTION   : Set method for the second derivative along the X axis
* ARGUMENTS     : The second derivative value along the X axis to be set
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
void L7ShapeFeature::setXSecondDerv(float xSecondDerv)
{
	m_xSecondDerv = xSecondDerv;
}
/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : setYSecondDerv
* DESCRIPTION   : Set method for the second derivative value along the y axis
* ARGUMENTS     : The second derivative value along the Y axis to be set
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
void L7ShapeFeature::setYSecondDerv(float ySecondDerv)
{
	m_ySecondDerv = ySecondDerv;
}/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : setCurvature
* DESCRIPTION   : This method sets the curvature
* ARGUMENTS     : The curvature value to be set
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
void L7ShapeFeature::setCurvature(float curvature)
{
	m_curvature=curvature;
}
/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : setPenUp
* DESCRIPTION   : This method sets the penUp
* ARGUMENTS     : The penUp value to be set
* RETURNS       : 
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
void L7ShapeFeature::setPenUp(bool penUp)
{
	m_penUp=penUp;
	

}
/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : clone
* DESCRIPTION   : Clone method for an L7ShapeFeature object
* ARGUMENTS     : none
* RETURNS       : A copy of the invoking L7ShapeFeature object
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
LTKShapeFeaturePtr L7ShapeFeature::clone() const
{
	L7ShapeFeature* l7Inst = new L7ShapeFeature();

	l7Inst->setX(this->getX());
	l7Inst->setY(this->getY());
	l7Inst->setXFirstDerv(this->getXFirstDerv());
	l7Inst->setYFirstDerv(this->getYFirstDerv());
	l7Inst->setXSecondDerv(this->getXSecondDerv());
	l7Inst->setYSecondDerv(this->getYSecondDerv());
	l7Inst->setCurvature(this->getCurvature());
	l7Inst->setPenUp(this->isPenUp());
	
	return (LTKShapeFeaturePtr)l7Inst;
}
/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : getDistance
* DESCRIPTION   : Returns the distance between two L7ShapeFeature objects
* ARGUMENTS     : An L7ShapeFeature object the distance of which is to be found from the invoking L7ShapeFeature object
* RETURNS       : The squared Euclidean distance
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
void L7ShapeFeature::getDistance(const LTKShapeFeaturePtr& shapeFeaturePtr, float& outDistance) const
{
	float xDiff = 0, yDiff = 0;
	float xFirstDervDiff=0, yFirstDervDiff=0, xSecondDervDiff=0, ySecondDervDiff=0;
	float curvatureDiff=0;
	
	L7ShapeFeature *inFeature = (L7ShapeFeature*)(shapeFeaturePtr.operator ->());
	
	xDiff			= m_x			-  inFeature->getX();
    yDiff			= m_y			-  inFeature->getY();
	xFirstDervDiff  = m_xFirstDerv  -  inFeature->getXFirstDerv();
	xFirstDervDiff  = m_yFirstDerv  -  inFeature->getYFirstDerv();
	xSecondDervDiff = m_xSecondDerv -  inFeature->getXSecondDerv();
	ySecondDervDiff = m_ySecondDerv -  inFeature->getYSecondDerv();
	curvatureDiff   = m_curvature   -  inFeature->getCurvature();

    outDistance = ( (xDiff * xDiff) + (yDiff * yDiff) );
	outDistance += ( (xFirstDervDiff * xFirstDervDiff) + (yFirstDervDiff * yFirstDervDiff) );
	outDistance += ( (xSecondDervDiff * xSecondDervDiff) + (ySecondDervDiff * ySecondDervDiff) );
	outDistance += ( (curvatureDiff * curvatureDiff));
}
/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : initialize
* DESCRIPTION   : This method initializes an L7ShapeFeature object from a string 
* ARGUMENTS     : The string containing the feature values
* RETURNS       : SUCCESS on successful completion
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
int L7ShapeFeature::initialize(const string& initString)
{	
	stringVector tokens;

	LTKStringUtil::tokenizeString(initString, m_data_delimiter, tokens);

	//Token size must be eight
	if(tokens.size() != 8)
		 return FAILURE; //Returning an error

    m_x = LTKStringUtil::convertStringToFloat(tokens[0]);
    m_y = LTKStringUtil::convertStringToFloat(tokens[1]);
    m_xFirstDerv = LTKStringUtil::convertStringToFloat(tokens[2]);
    m_yFirstDerv = LTKStringUtil::convertStringToFloat(tokens[3]);
    m_xSecondDerv = LTKStringUtil::convertStringToFloat(tokens[4]);
    m_ySecondDerv = LTKStringUtil::convertStringToFloat(tokens[5]);
    m_curvature= LTKStringUtil::convertStringToFloat(tokens[6]);

	if(atoi(tokens[7].c_str()) == 1)
		m_penUp = true;
	else
		m_penUp = false;
	
	return SUCCESS;
}
/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : toString
* DESCRIPTION   : This method prints the components of the feature object a string.
* ARGUMENTS     : pointer to char
* RETURNS       : The  feature object in string format
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
void L7ShapeFeature::toString(string& strFeat) const
{	
	ostringstream tempString;

    tempString << m_x << m_data_delimiter << m_y << m_data_delimiter << 
                  m_xFirstDerv << m_data_delimiter << 
                  m_yFirstDerv << m_data_delimiter <<
                  m_xSecondDerv << m_data_delimiter <<
                  m_ySecondDerv << m_data_delimiter <<
                  m_curvature << m_data_delimiter <<
                  m_penUp ;
    

    strFeat = tempString.str();

}
/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : addFeature
* DESCRIPTION   : This method adds the passed L7ShapeFeature object to the invoking L7ShapeFeature object
* ARGUMENTS     : The feature instance to be added
* RETURNS       : The result feature object
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
int L7ShapeFeature::addFeature(const LTKShapeFeaturePtr& secondFeature, 
                                 LTKShapeFeaturePtr& outResult ) const
{
	L7ShapeFeature* resultFeature = new L7ShapeFeature();

    L7ShapeFeature *inFeature = (L7ShapeFeature*)(secondFeature.operator ->());
    
	resultFeature->setX(m_x + inFeature->getX());
	resultFeature->setY(m_y + inFeature->getY());
	resultFeature->setXFirstDerv(m_xFirstDerv + inFeature->getXFirstDerv());
	resultFeature->setYFirstDerv(m_yFirstDerv + inFeature->getYFirstDerv());
	resultFeature->setXSecondDerv(m_xSecondDerv + inFeature->getXSecondDerv());
	resultFeature->setYSecondDerv(m_ySecondDerv + inFeature->getYSecondDerv());
	resultFeature->setCurvature(m_curvature + inFeature->getCurvature());
	resultFeature->setPenUp(m_penUp);

	outResult = LTKShapeFeaturePtr(resultFeature);
    
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : subtractFeature
* DESCRIPTION   : This method subtracts the elements of the passed L7ShapeFeature object from the invoking L7ShapeFeature object.
* ARGUMENTS     : 
* RETURNS       : The result feature object
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
int L7ShapeFeature::subtractFeature(const LTKShapeFeaturePtr& secondFeature, 
                                      LTKShapeFeaturePtr& outResult ) const
{
	L7ShapeFeature* resultFeature = new L7ShapeFeature();

    L7ShapeFeature *inFeature = (L7ShapeFeature*)(secondFeature.operator ->());

	resultFeature->setX(m_x-inFeature->getX());
	resultFeature->setY(m_y-inFeature->getY());
	resultFeature->setXFirstDerv(m_xFirstDerv - inFeature->getXFirstDerv());
	resultFeature->setYFirstDerv(m_yFirstDerv - inFeature->getYFirstDerv());
	resultFeature->setXSecondDerv(m_xSecondDerv - inFeature->getXSecondDerv());
	resultFeature->setYSecondDerv(m_ySecondDerv - inFeature->getYSecondDerv());
	resultFeature->setCurvature(m_curvature - inFeature->getCurvature());
	resultFeature->setPenUp(m_penUp);

    outResult = LTKShapeFeaturePtr(resultFeature);
    
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : scaleFeature
* DESCRIPTION   : This method mulitplies the elements of the invoking L7ShapeFeature object by alpha.
* ARGUMENTS     : alpha which is the scaling factor
* RETURNS       : The scaled feature object
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
int L7ShapeFeature::scaleFeature(float alpha, LTKShapeFeaturePtr& outResult) const
{
	L7ShapeFeature* resultFeature = new L7ShapeFeature();

	resultFeature->setX(m_x * alpha);
	resultFeature->setY(m_y * alpha);
	resultFeature->setXFirstDerv(m_xFirstDerv * alpha);
	resultFeature->setYFirstDerv(m_yFirstDerv * alpha);
	resultFeature->setXSecondDerv(m_xSecondDerv * alpha);
	resultFeature->setYSecondDerv(m_ySecondDerv * alpha);
	resultFeature->setCurvature(m_curvature * alpha);
	resultFeature->setPenUp(m_penUp);

	outResult = LTKShapeFeaturePtr(resultFeature);
    
	return SUCCESS;
}

/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : toFloatVector
* DESCRIPTION   : This method converts the feature instance to a floatVector.
* ARGUMENTS     : The float vector passed by reference
* RETURNS       : 0 on success.
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
int L7ShapeFeature::toFloatVector(floatVector& floatVec)
{
	floatVec.push_back(m_x);
	floatVec.push_back(m_y);
	floatVec.push_back(m_xFirstDerv);
	floatVec.push_back(m_yFirstDerv);
	floatVec.push_back(m_xSecondDerv);
	floatVec.push_back(m_ySecondDerv);
	floatVec.push_back(m_curvature);

	if(m_penUp == true)
		floatVec.push_back(1.0);
	else
		floatVec.push_back(0.0);

	return SUCCESS;
}

/**********************************************************************************
* AUTHOR        : Naveen Sundar G.
* DATE          : 10-AUGUST-2007
* NAME          : toIntVector
* DESCRIPTION   : This method converts the feature instance to an intVector.
* ARGUMENTS     : The integer vector passed by reference
* RETURNS       : 0 on success.
* NOTES         :
* CHANGE HISTROY
* Author            Date                Description of change
*************************************************************************************/
int L7ShapeFeature::toIntVector(intVector& intVec)
{
	intVec.push_back(m_x);
	intVec.push_back(m_y);
	intVec.push_back(m_xFirstDerv);
	intVec.push_back(m_yFirstDerv);
	intVec.push_back(m_xSecondDerv);
	intVec.push_back(m_ySecondDerv);
	intVec.push_back(m_curvature);
	intVec.push_back(m_penUp);

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
int L7ShapeFeature::initialize(const floatVector& initFloatVector)
{

    if (initFloatVector.size() == 0)
    {
        return FAILURE;
    }
    
    m_x = initFloatVector[0];
	m_y = initFloatVector[1];
	m_xFirstDerv = initFloatVector[2];
	m_yFirstDerv = initFloatVector[3];
    m_xSecondDerv = initFloatVector[4];
    m_ySecondDerv = initFloatVector[5];
    m_curvature = initFloatVector[6];        

	if(initFloatVector[7] == 1)
    {   
		m_penUp = true;
    }
	else
    {   
		m_penUp = false;
    }
	
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

int L7ShapeFeature::getFeatureDimension()
{
    return 8;
}
