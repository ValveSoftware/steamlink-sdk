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
 * $LastChangedDate: 2008-07-18 15:33:34 +0530 (Fri, 18 Jul 2008) $
 * $Revision: 564 $
 * $Author: sharmnid $
 *
 ************************************************************************/
/************************************************************************
 * FILE DESCR: Contains list of standard type definitions
 *
 * CONTENTS:   
 *
 * AUTHOR:     Balaji R.
 *
 * DATE:       December 23, 2004
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/

#ifndef __LTKTYPES_H
#define __LTKTYPES_H

#include "LTKInc.h"
//class LTKShapeFeature;
class LTKChannel;
class LTKTrace;

typedef vector<bool> boolVector;
typedef vector<int> intVector;
typedef vector<float> floatVector;
typedef vector<double> doubleVector;
typedef vector<string> stringVector;
typedef vector<intVector> int2DVector;
typedef vector< floatVector> float2DVector;
typedef vector<doubleVector> double2DVector;
typedef vector<float2DVector > float3DVector;
typedef map<string, string> stringStringMap;
typedef float (*distPointer)(const vector<float>&,const  vector<float>&);
typedef int (*reduceFunctionPointer)(const vector<vector<float> >&,vector<vector<float> >&);
typedef vector <float> Features;
typedef vector <Features> Character;
typedef map<string,int> stringIntMap;
//typedef double (*FN_PTR_DELETE_SHAPE_FTR_PTR)(LTKShapeFeature*);
typedef vector<LTKChannel> LTKChannelVector;
typedef vector<LTKTrace> LTKTraceVector;
typedef pair<string,string> stringStringPair;
typedef map<int,int> intIntMap;

typedef struct ModuleRefCount
{
	vector<void*> vecRecoHandles;
	void* modHandle;
	int iRefCount;
} MODULEREFCOUNT;


/* @enum ELTKDataType This enumerator stores the data type of a channel.
 *                    All channel values are stored in memory as float.
 */

enum ELTKDataType
{
	DT_BOOL, DT_SHORT, DT_INT, DT_LONG, DT_FLOAT, DT_DOUBLE, DT_NULL
};

enum ELTKTraceGroupStatistics 
{
	TG_MAX, TG_MIN, TG_AVG
};

struct LTKControlInfo
{
	string projectName;
	string profileName;
	string cfgFileName;
	string cfgFilePath;
	string lipiRoot;
	string lipiLib;
	string toolkitVersion;

	// constructor
	LTKControlInfo():
	projectName(""),
	profileName(""),
	cfgFileName(""),
	cfgFilePath(""),
	lipiRoot(""),
	lipiLib(""),
	toolkitVersion("")
	{
	}

};

//Enumerator to indicate corner point of a tracegroup
 enum TGCORNER 
 {
	XMIN_YMIN,
	XMIN_YMAX,
	XMAX_YMIN,
	XMAX_YMAX 
};

#endif	//#ifndef __LTKTYPES_H


