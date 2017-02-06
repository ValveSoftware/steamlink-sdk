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
#include "LTKShapeSample.h"

#include "LTKShapeFeature.h"

#include "LTKRefCountedPtr.h"

#include "LTKLoggerUtil.h"


/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 22-Mar-2007
 * NAME			: LTKShapeSample
 * DESCRIPTION	: Default Constructor
 * ARGUMENTS		: 
 * RETURNS		: 
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description of change
 * ************************************************************************************/

LTKShapeSample::LTKShapeSample()
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering LTKShapeSample::LTKShapeSample()"  <<endl;
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting LTKShapeSample::LTKShapeSample()"  <<endl;

}

/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 22-Mar-2007
 * NAME			: LTKShapeSample
 * DESCRIPTION	: Copy Constructor
 * ARGUMENTS		: sampleFeatures - LTKShapeSample to be copied
 * RETURNS		: 
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description of change
 * ************************************************************************************/

LTKShapeSample::LTKShapeSample(const LTKShapeSample& sampleFeatures)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering LTKShapeSample::LTKShapeSample(const LTKShapeSample& )"  <<endl;

    m_featureVector = sampleFeatures.m_featureVector;
    m_classId = sampleFeatures.m_classId;

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting LTKShapeSample::LTKShapeSample(const LTKShapeSample& )"  <<endl;
}

/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 22-Mar-2007
 * NAME			: operator=
 * DESCRIPTION	: Overloaded assignment operator
 * ARGUMENTS		: sampleFeatures - LTKShapeSample to be assigned to
 * RETURNS		: 
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description of change
 * ************************************************************************************/

LTKShapeSample& LTKShapeSample::operator=(const LTKShapeSample& sampleFeatures)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering LTKShapeSample::operator=(const LTKShapeSample& )"  <<endl;

    if ( this != &sampleFeatures )
    {
        m_featureVector = sampleFeatures.m_featureVector;
        m_classId = sampleFeatures.m_classId;
    }	

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering LTKShapeSample::operator=(const LTKShapeSample& )"  <<endl;
    return *this;
}

/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 22-Mar-2007
 * NAME			: ~LTKShapeSample
 * DESCRIPTION	: destructor
 * ARGUMENTS		: 
 * RETURNS		: 
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description of change
 * ************************************************************************************/

LTKShapeSample::~LTKShapeSample()
{
}

/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 22-Mar-2007
 * NAME			: setFeatureVector
 * DESCRIPTION	: 
 * ARGUMENTS		: 
 * RETURNS		: 
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description of change
 * ************************************************************************************/
void LTKShapeSample::setFeatureVector(const vector<LTKShapeFeaturePtr>& inFeatureVec)
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering LTKShapeSample:setFeatureVector()"  <<endl;

    m_featureVector = inFeatureVec;

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting LTKShapeSample:setFeatureVector()"  <<endl;
}

/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 22-Mar-2007
 * NAME			: setClassID
 * DESCRIPTION	: 
 * ARGUMENTS		: 
 * RETURNS		: 
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description of change
 * ************************************************************************************/
void LTKShapeSample::setClassID(int inClassId) 
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering LTKShapeSample:setClassID()"  <<endl;

    m_classId = inClassId;

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting LTKShapeSample:setClassID()"  <<endl;
}

/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 22-Mar-2007
 * NAME			: getFeatureVector
 * DESCRIPTION	: 
 * ARGUMENTS		: 
 * RETURNS		: 
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description of change
 * ************************************************************************************/
const vector<LTKShapeFeaturePtr>& LTKShapeSample::getFeatureVector() const
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering LTKShapeSample:getFeatureVector()"  <<endl;

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting LTKShapeSample:getFeatureVector()"  <<endl;

    return m_featureVector;
}

/**********************************************************************************
 * AUTHOR		: The Qt Company
 * DATE			: 17-Mar-2015
 * NAME			: getFeatureVectorRef
 * DESCRIPTION	:
 * ARGUMENTS		:
 * RETURNS		:
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description of change
 * ************************************************************************************/
vector<LTKShapeFeaturePtr>& LTKShapeSample::getFeatureVectorRef()
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering LTKShapeSample:getFeatureVectorRef()"  <<endl;

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting LTKShapeSample:getFeatureVectorRef()"  <<endl;

    return m_featureVector;
}

/**********************************************************************************
 * AUTHOR		: Saravanan R.
 * DATE			: 22-Mar-2007
 * NAME			: getClassID
 * DESCRIPTION	: 
 * ARGUMENTS		: 
 * RETURNS		: 
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description of change
 * ************************************************************************************/
int LTKShapeSample::getClassID() const
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering LTKShapeSample:getClassID()"  <<endl;

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting LTKShapeSample:getClassID()"  <<endl;

    return m_classId;
}

/**************************************************************************
 * AUTHOR		: Nidhi Sharma
 * DATE			: 28-Nov-2007
 * NAME			: clearShapeSampleFeatures
 * DESCRIPTION	: 
 * ARGUMENTS		: 
 * RETURNS		: 
 * NOTES			:
 * CHANGE HISTROY
 * Author			Date				Description of change
 * ****************************************************************************/
void LTKShapeSample::clearShapeSampleFeatures()
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering LTKShapeSample:clearShapeSampleFeatures()"  <<endl;

    m_featureVector.clear();
    m_classId = -1;

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting LTKShapeSample:clearShapeSampleFeatures()"  <<endl;
}
/**************************************************************************
 * AUTHOR		: Balaji MNA
 * DATE			: 30-June-2009
 * NAME			: getCountStrokes
 * DESCRIPTION	: get strokes count
 * ARGUMENTS	: NONE
 * RETURNS		: return number of strokes
 * NOTES		:
 * CHANGE HISTROY
 * Author			Date				Description of change
 * ****************************************************************************/
int LTKShapeSample::getCountStrokes() const
{
    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Entering LTKShapeSample::getCountStrokes()"  <<endl;

	int countStrokes = 0; 

	vector<LTKShapeFeaturePtr>::const_iterator featureIter = m_featureVector.begin(); 
	vector<LTKShapeFeaturePtr>::const_iterator featureEnd = m_featureVector.end(); 
	 
	for(; featureIter != featureEnd; ++featureIter) 
	{ 
		if ((*featureIter)->isPenUp()) 
			++countStrokes; 
	} 

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
        "Exiting LTKShapeSample::getCountStrokes()"  <<endl;

	return countStrokes; 
}
