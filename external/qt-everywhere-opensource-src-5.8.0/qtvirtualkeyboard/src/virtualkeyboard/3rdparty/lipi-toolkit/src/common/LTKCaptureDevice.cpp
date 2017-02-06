/******************************************************************************
* Copyright (c) 2007 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy 
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights 
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
* copies of the Software, and to permit persons to whom the Software is 
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or 
* substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
* SOFTWARE
********************************************************************************/

/*******************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2008-07-18 15:00:39 +0530 (Fri, 18 Jul 2008) $
 * $Revision: 561 $
 * $Author: sharmnid $
 *
 *****************************************************************************/
/*****************************************************************************
 * FILE DESCR: Implementation of LTKCaptureDevice which holds the information 
 *             about the digitizer.
 *
 * CONTENTS:
 *	getSampleRate
 *	getLatency
 *	getXDPI
 *	getYDPI
 *	isUniformSamplingRate
 *	setXDPI
 *	setYDPI
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 *****************************************************************************/

#include "LTKCaptureDevice.h"
#include "LTKErrorsList.h"
#include "LTKLoggerUtil.h"
#include "LTKMacros.h"
#include "LTKException.h"
#include "LTKErrors.h"

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKCaptureDevice
* DESCRIPTION	: Default Constructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

LTKCaptureDevice::LTKCaptureDevice():
	m_samplingRate(DEFAULT_SAMPLING_RATE),
	m_xDpi(DEFAULT_X_DPI),
	m_yDpi(DEFAULT_Y_DPI),
	m_latency(DEFAULT_LATENCY),
	m_isUniformSamplingRate(true)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKCaptureDevice::LTKCaptureDevice()" << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKCaptureDevice::LTKCaptureDevice()" << endl;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKCaptureDevice
* DESCRIPTION	: Initializes the members of the class
* ARGUMENTS		: sRate - sampling rate of the digitizer
*				  uniform - type of the sampling
*				  lValue - latency of the digitizer
*				  xDpiVal - resolution in the x direction of the digitizer 
*                           (in dots per inch)
*				  yDpiVal - resolution in the y direction of the digitizer 
                            (in dots per inch)			  
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

LTKCaptureDevice::LTKCaptureDevice(int sRate, bool uniform, float lValue,
                                   int xDpiVal, int yDpiVal) : 
	m_samplingRate(sRate), 
	m_xDpi(xDpiVal), 
	m_yDpi(yDpiVal),
	m_latency(lValue), 
	m_isUniformSamplingRate(uniform)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Enter: LTKCaptureDevice::LTKCaptureDevice(int, bool, float, int, int)"
		<< endl;

	if (m_samplingRate <= 0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
        "Error: "<<EINVALID_SAMPLING_RATE <<": "<<
        getErrorMessage(EINVALID_SAMPLING_RATE) <<
		"LTKCaptureDevice::LTKCaptureDevice(int, bool, float, int, int)"<<endl;

		throw LTKException(EINVALID_SAMPLING_RATE);
	}

	if (m_xDpi <= 0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
        "Error: "<<EINVALID_X_RESOLUTION <<": "
         <<getErrorMessage(EINVALID_X_RESOLUTION) <<
		"LTKCaptureDevice::LTKCaptureDevice(int, bool, float, int, int)"<<endl;

		throw LTKException(EINVALID_X_RESOLUTION);
	}

	if (m_yDpi <= 0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
        "Error: "<<EINVALID_Y_RESOLUTION <<": "
        <<getErrorMessage(EINVALID_Y_RESOLUTION) <<
		"LTKCaptureDevice::LTKCaptureDevice(int, bool, float, int, int)"<<endl;

		throw LTKException(EINVALID_Y_RESOLUTION);
	}

    if (m_latency < 0)
	{
		LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
        "Error: "<<EINVALID_LATENCY <<": "
        <<getErrorMessage(EINVALID_LATENCY) <<
		"LTKCaptureDevice::LTKCaptureDevice(int, bool, float, int, int)"<<endl;

		throw LTKException(EINVALID_LATENCY);
	}

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
        " m_samplingRate = " << m_samplingRate <<
        " m_xDpi = " << m_xDpi << 
        " m_yDpi = " << m_yDpi << 
        " m_latency = " << m_latency << 
        " m_isUniformSamplingRate = " << m_isUniformSamplingRate << endl;
    
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
        "Exit: LTKCaptureDevice::LTKCaptureDevice(int, bool, float, int, int)"
		<< endl;

}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: LTKCaptureDevice
* DESCRIPTION	: Copy Constructor
* ARGUMENTS		: captureDevice - LTKCaptureDevice to be copied
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/
LTKCaptureDevice::LTKCaptureDevice(const LTKCaptureDevice& captureDevice)
{

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        " Enter: LTKCaptureDevice:LTKCaptureDevice(const LTKCaptureDevice&)"
		<< endl;

	m_samplingRate = captureDevice.m_samplingRate;

	m_isUniformSamplingRate = captureDevice.m_isUniformSamplingRate;

	m_latency = captureDevice.m_latency;

	m_xDpi = captureDevice.m_xDpi;

	m_yDpi = captureDevice.m_yDpi;

    LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
        " m_samplingRate = " << m_samplingRate <<
        " m_xDpi = " << m_xDpi << 
        " m_yDpi = " << m_yDpi << 
        " m_latency = " << m_latency << 
        " m_isUniformSamplingRate = " << m_isUniformSamplingRate << endl;
    
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
    "Exit: LTKCaptureDevice::LTKCaptureDevice(const LTKCaptureDevice&)"<<endl;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: operator=
* DESCRIPTION	: Overloaded assignment operator
* ARGUMENTS		: captureDevice - LTKCaptureDevice to be assigned to
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

LTKCaptureDevice& LTKCaptureDevice::operator = 
									(const LTKCaptureDevice& captureDevice)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Enter: LTKCaptureDevice:: operator =()" << endl;

	if(this != &captureDevice)
	{
		m_samplingRate = captureDevice.m_samplingRate;

		m_isUniformSamplingRate = captureDevice.m_isUniformSamplingRate;

		m_latency = captureDevice.m_latency;

		m_xDpi = captureDevice.m_xDpi;

		m_yDpi = captureDevice.m_yDpi;

        LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
            " m_samplingRate = " << m_samplingRate <<
            " m_xDpi = " << m_xDpi << 
            " m_yDpi = " << m_yDpi << 
            " m_latency = " << m_latency << 
            " m_isUniformSamplingRate = " << m_isUniformSamplingRate << endl;
	}

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exit: LTKCaptureDevice:: operator =()"  << endl;

	return *this;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getSampleRate
* DESCRIPTION	: returns back the sampling rate of the digitizer
* ARGUMENTS		: 
* RETURNS		: sampling rate of the digitizer
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

int LTKCaptureDevice::getSamplingRate()   const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Enter: LTKCaptureDevice::getSampleRate()"  << endl;



	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exit: LTKCaptureDevice::getSampleRate()" << endl;

	return m_samplingRate;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getLatency
* DESCRIPTION	: gets the latency of the digitizer
* ARGUMENTS		: 
* RETURNS		: latency of the digitizer
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

float LTKCaptureDevice::getLatency()  const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Enter: LTKCaptureDevice::getLatency()"  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exit: LTKCaptureDevice::getLatency()" << endl;

	return m_latency;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getXDPI
* DESCRIPTION	: gets the x resolution of the digitizer (in dots per inch)
* ARGUMENTS		: 
* RETURNS		: x resolution of the digitizer (in dots per inch)
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

int LTKCaptureDevice::getXDPI()  const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Enter: LTKCaptureDevice::getXDPI()"  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exit: LTKCaptureDevice::getXDPI()" << endl;

	return m_xDpi;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: getYDPI
* DESCRIPTION	: gets the y resolution of the digitizer (in dots per inch)
* ARGUMENTS		: 
* RETURNS		: y resolution of the digitizer (in dots per inch)
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

int LTKCaptureDevice::getYDPI()  const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Enter: LTKCaptureDevice::getYDPI()"  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exit: LTKCaptureDevice::getYDPI()" << endl;

	return m_yDpi;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: isUniformSamplingRate
* DESCRIPTION	: gets the type of sampling
* ARGUMENTS		: 
* RETURNS		: type of sampling : true if uniform, false if not
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

bool LTKCaptureDevice::isUniformSampling()  const
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Enter: LTKCaptureDevice::isUniformSamplingRate()"  << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exit: LTKCaptureDevice::isUniformSamplingRate()" << endl;

	return m_isUniformSamplingRate;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setSamplingRate
* DESCRIPTION	: sets the sampling rate of the device
* ARGUMENTS		: samplingRate - sampling rate of the device
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

int LTKCaptureDevice::setSamplingRate(int samplingRate)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Enter: LTKCaptureDevice::setSamplingRate()"  << endl;

    if (samplingRate <= 0 )
    {
        LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
        "Error: "<<EINVALID_SAMPLING_RATE <<": "<<
		getErrorMessage(EINVALID_SAMPLING_RATE) <<
		"LTKCaptureDevice::setSamplingRate()"<<endl;

        LTKReturnError(EINVALID_SAMPLING_RATE);
    }

    m_samplingRate = samplingRate;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "m_samplingRate = " << m_samplingRate << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exit: LTKCaptureDevice::setSamplingRate()"  << endl;

	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setLatency
* DESCRIPTION	: sets the latency of the device
* ARGUMENTS		: latency - latency of the device
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

int LTKCaptureDevice::setLatency(float latency)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Enter: LTKCaptureDevice::setLatency()"  << endl;

    if (m_latency < 0)
	{
	    LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
        "Error: "<<EINVALID_LATENCY <<": "<<
		getErrorMessage(EINVALID_LATENCY) << 
		"LTKCaptureDevice::setLatency()"<<endl;

		LTKReturnError(EINVALID_LATENCY);
	}

	m_latency = latency;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "m_latency = " << m_latency << endl;

	
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
        "Exit: LTKCaptureDevice::setLatency()"  << endl;

	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setUniformSampling
* DESCRIPTION	: sets the type of sampling
* ARGUMENTS		: uniform - type of sampling
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

void LTKCaptureDevice::setUniformSampling(bool uniform)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		"Enter: LTKCaptureDevice::setUniformSampling()"  << endl;

	m_isUniformSamplingRate = uniform;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) << 
		"Exit: LTKCaptureDevice::setUniformSampling()"  << endl;

	
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setXDPI
* DESCRIPTION	: sets the x resolution of the digitizer (in dots per inch)
* ARGUMENTS		: xDpiVal - x resolution of the digitizer (in dots per inch)
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

int LTKCaptureDevice::setXDPI(int xDpiVal)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<  
		"Enter: LTKCaptureDevice::setXDPI()"  << endl;

    if (xDpiVal <= 0)
	{
	    LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
        "Error: "<<EINVALID_X_RESOLUTION <<": "<<
		getErrorMessage(EINVALID_X_RESOLUTION) << 
		"LTKCaptureDevice::setXDPI()"<<endl;

		LTKReturnError(EINVALID_X_RESOLUTION);
	}
    
	m_xDpi = xDpiVal;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<"m_xDpi = " << m_xDpi << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<
		"Exit: LTKCaptureDevice::setXDPI()"  << endl;

	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: setYDPI
* DESCRIPTION	: sets the y resolution of the digitizer (in dots per inch)
* ARGUMENTS		: yDpiVal - y resolution of the digitizer (in dots per inch)
* RETURNS		: SUCCESS on successful set operation
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

int LTKCaptureDevice::setYDPI (int yDpiVal)
{
	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Enter: LTKCaptureDevice::setYDPI()"  << endl;

    if ( yDpiVal <= 0 )
	{
	    LOG( LTKLogger::LTK_LOGLEVEL_ERR) << 
        "Error: "<<EINVALID_Y_RESOLUTION <<": "<<
		getErrorMessage(EINVALID_Y_RESOLUTION) << 
		"LTKCaptureDevice::setYDPI()"<<endl;

		LTKReturnError(EINVALID_Y_RESOLUTION);
	}
	m_yDpi = yDpiVal;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG)<<"m_yDpi = " << m_yDpi << endl;

	LOG( LTKLogger::LTK_LOGLEVEL_DEBUG) <<
		"Exit: LTKCaptureDevice::setYDPI()"  << endl;

	return SUCCESS;
}

/******************************************************************************
* AUTHOR		: Balaji R.
* DATE			: 23-DEC-2004
* NAME			: ~LTKCaptureDevice
* DESCRIPTION	: destructor
* ARGUMENTS		: 
* RETURNS		: 
* NOTES			:
* CHANGE HISTROY
* Author			Date				Description of change
* ****************************************************************************/

LTKCaptureDevice::~LTKCaptureDevice ()
{
}

