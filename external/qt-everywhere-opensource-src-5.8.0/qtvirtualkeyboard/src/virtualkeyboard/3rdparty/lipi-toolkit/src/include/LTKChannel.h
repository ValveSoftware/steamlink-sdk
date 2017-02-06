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
 * $LastChangedDate: 2008-07-10 15:23:21 +0530 (Thu, 10 Jul 2008) $
 * $Revision: 556 $
 * $Author: sharmnid $
 *
 ************************************************************************/

/************************************************************************
 * FILE DESCR: Definition of LTKChannel which has the description of a particular 
			 	input stream like x-coordinate stream, y-coordinate stream, 
				time, pressure etc.
 *
 * CONTENTS:   
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef __LTKCHANNEL_H
#define __LTKCHANNEL_H

#include "LTKTypes.h"

/**
* @ingroup Common_Classes
*/

/** @brief Stores information about a channel,whose value is captured by a digitizer. 
 * @class LTKChannel 
 * <p> Channels captured by a digitizer can have different data types. The memory storage for the
 * data types of these channels is thus different.
 * LIPI Toolkit requires coordinates to have "float" data type to have same memory allocation
 * for coordinate values. </p>
 *
 * <p> In order to retain the information about a channel,name, datatype, and a boolean (to indicate
 * if the channel is Regular or Intermittent) are stored in this class. </p>
 *
 * <p> LTKChannel class object has to be created prior to any request for recognition. These
 * objects are stored in LTKTraceFormat class. </p>
 * 
 * @see LTKTraceFormat
 * 
 */


class LTKChannel
{
private:
	
	string m_channelName;			//	logical name of the channel

	ELTKDataType m_channelType;		//	data type of values obtained from channel

	bool m_isRegularChannel;		//	flag to indicate if a value for the channel 
									//  is available at every sampling instance

public:
 
	/** @name Constructors and Destructor */
	
	//@{

	/**
	 * Default Constructor.
	 */
	
	LTKChannel();


	LTKChannel(const string& channelName);
   	
	/**
	 * This constructor initializes the members of the class
	 * @param channelName  Name of the channel
	 * @param ELTKDataType Data type of the channel is stored. This is stored only for information.
	 *                     Internally all channel values are stored as float.
	 * @param isRegular    Boolean variable that indicates if the channel is Regular or Intermittent.
	 * 
	 */

 	LTKChannel(const string& channelName, ELTKDataType channelType, bool isRegular);	

	/** 
	 * Copy Constructor
	 */
	
	LTKChannel(const LTKChannel& channel);
	
	/**
	 * Destructor
	 */
	
	~LTKChannel();
	//@}
	
	/**
	  * @name Assignment operator 
	  */
	//@{
	
	/**
	 * Assignment operator
	 * @param channelObj The object to be copied by assignment
	 *
	 * @return LTKChannel object
	 */
	
	LTKChannel& operator=(const LTKChannel& channel);
	//@}
	
	/**
	 * @name Getter Functions
	 */
	//@{ 
	
	/**
	 * This method returns data type of a channel
	 * @param void
	 *
	 * @return enum data type of ELTKDataType .
	 */
	
	ELTKDataType getChannelType() const;
	
	/**
	 * This method returns name of the channel.
	 * @param void
	 *
	 * @return name of the channel.
	 */

    string getChannelName() const;
	
	/**
	 * This method returns a boolen to indicate if the channel is regular or intermittent.
	 * @param void
	 *
	 * @return True if channel is Regular, False if channel is Intermittent.
	 */
	
	bool isRegularChannel() const;
	//@}
	
	/**
	 * @name Setter Functions
	 */
	//@{
	
	/**
	 * This method sets the type of a channel
	 * @param channelType The channel type passed should belong to ELTKDataType.
	 *
	 * @return SUCCESS on successful set operation
	 */
	
	void setChannelType(ELTKDataType channelType);
	
	/**
	 * This method sets the channel name
	 * @param channelName The name of the channel which a digitizer can capture.
	 *
	 * @return SUCCESS on successful set operation
	 */
	
	int setChannelName(const string& channelName);
	
	/**
	 * This method sets a boolean to indicate if the channel is Regular or Intermittent.
	 * @param isRegular boolean value to indicate if the channel is Regular or Intermittent.
	 *
	 * @return SUCCESS on successful set operation
	 */
	
	void setRegularChannel(bool isRegular);
	//@}

};

#endif
    
//#ifndef __LTKCHANNEL_H
//#define __LTKCHANNEL_H




	 



