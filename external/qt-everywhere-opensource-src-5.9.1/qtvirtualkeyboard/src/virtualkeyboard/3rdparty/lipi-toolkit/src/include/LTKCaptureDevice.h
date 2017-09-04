/******************************************************************************
* Copyright (c) 2006 Hewlett-Packard Development Company, L.P.
* Permission is hereby granted, free of charge, to any person obtaining a copy 
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights 
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell 
* copies of the Software, and to permit persons to whom the Software is 
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
* SOFTWARE
********************************************************************************/

/******************************************************************************
 * SVN MACROS
 *
 * $LastChangedDate: 2008-07-18 15:00:39 +0530 (Fri, 18 Jul 2008) $
 * $Revision: 561 $
 * $Author: sharmnid $
 *
 ******************************************************************************/
/************************************************************************
 * FILE DESCR: Definition of LTKCaptureDevice which holds the information about
 * the digitizer.
 *
 * CONTENTS:   
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 *****************************************************************************/

#ifndef __LTKCAPTUREDEVICE_H
#define __LTKCAPTUREDEVICE_H

#include "LTKInc.h"

/**
 * @class LTKCaptureDevice
 * <p> This class contains meta-data about hardware that was used to acquire
 * the ink contained in a file. </p>
 */

class LTKCaptureDevice
{
private:

	int m_samplingRate;			//	sampling rate of the device

	int m_xDpi;					//	horizontal direction resolution of the device

	int m_yDpi;					//	vertical direction resolution of the device	

	float m_latency;			//	interval between the time of actual input 
								//  to that of its registration

	bool m_isUniformSamplingRate;	// flag to indicate if the sampling is uniform

public:

    /**
	 * @name Constructors and Destructor
	 */
	// @{

	/**
	 * Default Constructor
	 */

	LTKCaptureDevice();

	/**
	 * This constrcutor takes various paramaters about a digitizer.
	 * @param sRate   The sampling rate of bits
	 * @param uniform Indicates the consistency of sampling rate 
	 * @param lVaue   Device latency (in milliseconds) that applies to all channels
	 * @param xDpi    Dots per inch in horizontal direction.
	 * @param yDpi    Dots per inch in vertical direction.
	 */

	LTKCaptureDevice(int sRate, bool uniform, float lValue, int xDpi, int yDpi);

	/**
	 * Copy constructor
	 */

	LTKCaptureDevice(const LTKCaptureDevice& captureDevice);

	/** Destructor */

	~LTKCaptureDevice();

	// @}

	/** 
	 * @name Assignment operator
	 */
	// @{

	/**
	 * Assignment operator
	 * @param captureDevice The object that has to be copied by assignment
	 *
	 * @return LTKCaptureDevice object
	 */

	LTKCaptureDevice& operator=(const LTKCaptureDevice& captureDevice);
	// @}

	/**
	 * @name Getter Functions
	 */
	// @{

	/**
	 * This function returns the sampling rate measured in samples/sec
	 * @param void
	 *
	 * @return Sampling rate of bits.
	 */

	int getSamplingRate() const;

	/**
	 * This fucntion returns the latency of the real-time channel,
	 * in msec, from physical action to the API time stamp.
	 * @param void
	 * 
	 * @return Basic device latency in milli seconds.
	 */

	float getLatency() const;

	/**
	 * This function returns dots per inch in X direction.
	 * @param void
	 * 
	 * @return X dpi of the device
	 */

	int getXDPI() const;

	/**
	 * This function returns dots per inch in Y direction.
	 * @param void
	 * 
	 * @return Y dpi of the device
	 */

	int getYDPI()  const;

	/**
	 * This function returns a boolean to indicate if the sampling rate is 
	 * regular or irregular
	 * @param void
	 * 
	 * @return If the device samples the points uniformly or non-uniformly
	 */

	bool isUniformSampling()  const;
	// @}

	/**
	 * @name Setter Functions
	 */
	// @{

	/**
	 * This function sets the sampling rate of the device
	 * @param samplingRate Sampling rate of the device
	 *
	 * @return SUCCESS on successful set operation
	 */

	int setSamplingRate(int samplingRate);

	/**
	 * This fucntion returns the latency of the outputs from the device
	 * in msec, from physical action to the API time stamp.
	 * @param latency Latency of the device
	 * 
	 * @return SUCCESS on successful set operation
	 */

	 int setLatency(float latency);

	/**
	 * This function sets the boolean to indicate the consistency of sampling rate.
	 * @param isUniform True if sampling ir regular, False if irregular
	 *
	 * @return SUCCESS on successful set operation
	 */

	void setUniformSampling(bool isUniform);

	/**
	 * This function sets the value for dots per inch to be taken in X direction.
	 * @param xDpi The dots per inch in X direction.
	 *
	 * @return SUCCESS on successful set operation
	 */

	int setXDPI(int xDpi);

	/**
	 * This function sets the value for dots per inch to be taken in Y diretion.
	 * @param yDpi The dots per inch in Y direction.
	 *
	 * @return SUCCESS on successful set operation
	 */

	int setYDPI(int yDpi);
	// @}

};

#endif

//#ifndef __LTKCAPTUREDEVICE_H
//#define __LTKCAPTUREDEVICE_H
	









