#include "NPenShapeFeature.h"
#include "LTKStringUtil.h"
#include "LTKPreprocDefaults.h"

#include <sstream>


NPenShapeFeature::NPenShapeFeature():m_data_delimiter(",")
	{

	}

		
	NPenShapeFeature::NPenShapeFeature(float inX, float inY, float cosAlpha, float inSinAlpha,
                   float inCosBeta, float inSinBeta, float inAspect, float inCurliness,
                   float inLinearity, float inSlope, bool isPenUp):m_data_delimiter(",")
	{

	}
    
		
	NPenShapeFeature::~NPenShapeFeature()
	{

	}
	
	
	float NPenShapeFeature::getX() const
	{
		return m_x;
	}


	float NPenShapeFeature::getY() const
	{
		return m_y;
	}

	
	float NPenShapeFeature::getCosAlpha() const
	{
		return m_cosAlpha;
	}

	
	float NPenShapeFeature::getSinAlpha() const
	{
		return m_sinAlpha;
	}


	float NPenShapeFeature::getCosBeta() const
	{
		return m_cosBeta;
	}

	
	float NPenShapeFeature::getSinBeta() const
	{
		return m_sinBeta;
	}

	float NPenShapeFeature::getAspect() const
	{
		return m_aspect;
	}


	float NPenShapeFeature::getCurliness() const
	{
		return m_curliness;
	}

	
	float NPenShapeFeature::getLinearity() const
	{
		return m_linearity;
	}

	
	float NPenShapeFeature::getSlope() const
	{
		return m_slope;
	}

	
	bool NPenShapeFeature::isPenUp() const
	{
		return m_isPenUp;
	}

		
	void NPenShapeFeature::setX(float x)
	{
		m_x = x;
	}

		
	void NPenShapeFeature::setY(float y)
	{
		m_y = y;
	}

		
	void NPenShapeFeature::setCosAlpha(float cosAlpha)
	{
		m_cosAlpha = cosAlpha;
	}

	
	void NPenShapeFeature::setSinAlpha(float sinAlpha)
	{
		m_sinAlpha = sinAlpha;
	}
	
	
	void NPenShapeFeature::setCosBeta(float cosBeta)
	{
		m_cosBeta = cosBeta;
	}

		
	void NPenShapeFeature::setSinBeta(float sinBeta)
	{
		m_sinBeta = sinBeta;
	}

		
	void NPenShapeFeature::setAspect(float aspect)
	{
		m_aspect = aspect;
	}

	void NPenShapeFeature::setCurliness(float curliness)
	{
		m_curliness = curliness;
	}

	void NPenShapeFeature::setLinearity(float linearity)
	{
		m_linearity = linearity;
	}

	void NPenShapeFeature::setSlope(float slope)
	{
		m_slope = slope;
	}
	
	
	void NPenShapeFeature::setPenUp(bool isPenUp)
	{
		m_isPenUp = isPenUp;
	}

	

	int NPenShapeFeature::initialize(const string& initString)
	{
		stringVector tokens;
		LTKStringUtil::tokenizeString(initString,m_data_delimiter,tokens);
		
		if(tokens.size() != 11)
		{
			return FAILURE;
		}

         m_x = LTKStringUtil::convertStringToFloat(tokens[0]);

         m_y = LTKStringUtil::convertStringToFloat(tokens[1]);
		
		
         m_cosAlpha = LTKStringUtil::convertStringToFloat(tokens[2]);

         m_sinAlpha = LTKStringUtil::convertStringToFloat(tokens[3]);
		
         m_cosBeta = LTKStringUtil::convertStringToFloat(tokens[4]);

         m_sinBeta = LTKStringUtil::convertStringToFloat(tokens[5]);

         m_aspect = LTKStringUtil::convertStringToFloat(tokens[6]);

         m_curliness = LTKStringUtil::convertStringToFloat(tokens[7]);

         m_linearity = LTKStringUtil::convertStringToFloat(tokens[8]);

         m_slope = LTKStringUtil::convertStringToFloat(tokens[9]);

         if(fabs(LTKStringUtil::convertStringToFloat(tokens[10]) - 1.0f) < EPS)
		 {
			m_isPenUp = true;
		 }
		 else
		 {
			m_isPenUp =  false;
		 }

		 return SUCCESS;

		 
	}


	void NPenShapeFeature::toString(string& strFeat) const
	{
		ostringstream tempString;

		tempString << m_x << m_data_delimiter << m_y << m_data_delimiter << 
                  m_cosAlpha << m_data_delimiter << 
                  m_sinAlpha << m_data_delimiter <<
                  m_cosBeta << m_data_delimiter <<
                  m_sinBeta << m_data_delimiter <<
                  m_aspect << m_data_delimiter <<
				  m_curliness << m_data_delimiter <<
				  m_linearity << m_data_delimiter <<
				  m_slope << m_data_delimiter<<
				  m_isPenUp;

		strFeat = tempString.str();

	
	}
	
		
	LTKShapeFeaturePtr NPenShapeFeature::clone() const
	{
		NPenShapeFeature* npenSF = new NPenShapeFeature();


		npenSF->setX(this->getX());
		npenSF->setY(this->getY());
		npenSF->setCosAlpha(this->getCosAlpha());
		npenSF->setSinAlpha(this->getSinAlpha());
		npenSF->setCosBeta(this->getCosBeta());
		npenSF->setSinBeta(this->getSinBeta());
		npenSF->setAspect(this->getAspect());
		npenSF->setCurliness(this->getCurliness());
		npenSF->setLinearity(this->getLinearity());
		npenSF->setSlope(this->getSlope());
		npenSF->setPenUp(this->isPenUp());

		
		
		return (LTKShapeFeaturePtr)npenSF;
	}
    
    
	
	void NPenShapeFeature::getDistance(const LTKShapeFeaturePtr& shapeFeaturePtr, float& outDistance) const
	{ 
		outDistance = 0.0;
		
		NPenShapeFeature *inFeature = (NPenShapeFeature*)(shapeFeaturePtr.operator ->());
		
		outDistance += (m_x - inFeature->getX())*(m_x - inFeature->getX());
		outDistance += (m_y - inFeature->getY())*(m_y - inFeature->getY());
		outDistance += (m_cosAlpha - inFeature->getCosAlpha())*(m_cosAlpha - inFeature->getCosAlpha());
		outDistance += (m_sinAlpha - inFeature->getSinAlpha())*(m_sinAlpha - inFeature->getSinAlpha());
		outDistance += (m_cosBeta - inFeature->getCosBeta())*(m_cosBeta - inFeature->getCosBeta());
		outDistance += (m_sinBeta - inFeature->getSinBeta())*(m_sinBeta - inFeature->getSinBeta());
		outDistance += (m_aspect - inFeature->getAspect())*(m_aspect - inFeature->getAspect());
		outDistance += (m_curliness - inFeature->getCurliness())*(m_curliness - inFeature->getCurliness());
		outDistance += (m_linearity - inFeature->getLinearity())*(m_linearity - inFeature->getLinearity());
		outDistance += (m_slope - inFeature->getSlope())*(m_slope - inFeature->getSlope());
	}
	
	
	int NPenShapeFeature::toFloatVector(vector<float>& floatVec)
	{
		floatVec.push_back(m_x);
		floatVec.push_back(m_y);
		floatVec.push_back(m_cosAlpha);
		floatVec.push_back(m_sinAlpha);
		floatVec.push_back(m_cosBeta);
		floatVec.push_back(m_sinBeta);
		floatVec.push_back(m_aspect);
		floatVec.push_back(m_curliness);
		floatVec.push_back(m_linearity);
		floatVec.push_back(m_slope);

		if(isPenUp())
		{
			floatVec.push_back(1.0f);
		}
		else
		{
			floatVec.push_back(0.0f);
		}

		return SUCCESS;
	}


	int NPenShapeFeature::getFeatureDimension()
	{

		return 11;
	}


	int NPenShapeFeature::initialize(const floatVector& initFloatVector)
	{
		
		
		if(initFloatVector.size() != 11)
		{
			return FAILURE;
		}

		 m_x = initFloatVector[0];

		 m_y = initFloatVector[1];


		 m_cosAlpha = initFloatVector[2];

		 m_sinAlpha = initFloatVector[3];

		 m_cosBeta = initFloatVector[4];

		 m_sinBeta = initFloatVector[5];

		 m_aspect = initFloatVector[6];

		 m_curliness = initFloatVector[7];

		 m_linearity = initFloatVector[8];

		 m_slope = initFloatVector[9];

		 if(fabs(initFloatVector[10] - 1.0f) < EPS)
		 {
			m_isPenUp = true;
		 }
		 else
		 {
			m_isPenUp =  false;
		 }

		 return SUCCESS;
	}
