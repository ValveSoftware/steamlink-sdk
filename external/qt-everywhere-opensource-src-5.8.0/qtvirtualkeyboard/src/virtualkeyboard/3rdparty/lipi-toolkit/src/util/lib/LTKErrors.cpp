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
 * $LastChangedDate: 2009-04-17 11:23:39 +0530 (Fri, 17 Apr 2009) $
 * $Revision: 765 $
 * $Author: mnab $
 *
 ************************************************************************/

/************************************************************************
 * FILE DESCR: Definition of getError function.
 *
 * CONTENTS:
 *	getError
 *
 * AUTHOR:  VijayaKumara M.
 *
 * DATE:
 * CHANGE HISTORY:
 * Author		Date			Description of change
 ************************************************************************/
#include "LTKErrors.h"
#include "LTKMacros.h"
#include "LTKErrorsList.h"
#include "stdio.h"

int errorCode;

// Error list ( In the same oreder as in LTKErrorList.h file in "include" dir.
static map<int,string> errors;

static void initErrorCode()
{
	errors.clear();
	errors[EINK_FILE_OPEN] = "Unable to open ink file";				/* 100 - EINK_FILE_OPEN */
	errors[ECONFIG_FILE_OPEN] = "Unable to open configuration file";	/* 101 - ECONFIG_FILE_OPEN */
	errors[EHEADER_INFO_FILE_OPEN] = "Unable to open model header information file";								/* 102 - EHEADER_INFO_FILE_OPEN_ERR */
	errors[EMODEL_DATA_FILE_OPEN] = "Unable to open model data file";		/* 103 - EMODEL_DATA_FILE_OPEN_ERR */
	errors[ETRAINLIST_FILE_OPEN] = "Unable to open train list file";		/* 104 - ETRAINLIST_FILE_OPEN_ERR	*/
	errors[EMODEL_DATA_FILE_FORMAT] = "Incompatible model data file. The header is not in the desired format.";						/* 105 - EFILE_FORMAT_ERR	*/
	errors[EINVALID_INPUT_FORMAT] = "Model data file is corrupted";			/* 106 - EFILE_CORRUPT_ERR */

	errors[ELOAD_SHAPEREC_DLL] = "Error while loading shape recognition module";		/* 107 - ELOAD_SHAPEREC_DLL_ERR */
	errors[ELOAD_WORDREC_DLL] = "Error while loading word recognition module";		/* 108 - ELOAD_WORDREC_DLL_ERR */
	errors[ELOAD_PREPROC_DLL] = "Error while loading preprocessing module";			/* 109 - ELOAD_PREPROC_DLL_ERR */
	errors[EDLL_FUNC_ADDRESS] = "Exported function not found in module";			/* 110 - EDLL_FUNC_ADDRESS_ERR */
	errors[ECREATE_SHAPEREC] = "Error while creating shape recognizer instance";		/* 111 - ECREATE_SHAPEREC_ERR	*/
	errors[ECREATE_WORDREC] = "Error while creating word recognizer instance";		/* 112 - ECREATE_WORDREC_ERR */
	errors[ECREATE_PREPROC] = "Error while creating preprocessor instance";			/* 113 - ECREATE_PREPROC_ERR */

	errors[ELIPI_ROOT_PATH_NOT_SET] = "Environment variable LIPI_ROOT is not set";			/* 114 - ELIPI_ROOT_PATH_NOT_SET */
	errors[EINVALID_PROJECT_NAME] = "Invalid or no entry for project name";					/* 115 - EINVALID_PROJECT_NAME */
	errors[EINVALID_CONFIG_ENTRY] = "Invalid configuration entry in project.cfg file";		/* 116 - EINVALID_CONFIG_ENTRY */
	errors[ENO_SHAPE_RECOGNIZER] = "No shape recognizer specified in profile.cfg file";	/* 117 - ENO_SHAPE_RECOGNIZER */
	errors[ENO_WORD_RECOGNIZER] = "No word recognizer specified in profile.cfg file";		/* 118 - ENO_WORD_RECOGNIZER */

	errors[EINVALID_NUM_OF_TRACES] = "Invalid number of traces processed";				/* 119 - EINVALID_NUMBER_OF_TRACES */
	errors[EINVALID_NUM_OF_SHAPES] = "Invalid value for number of shapes";				/* 120 - EINVALID_VALUE_NUMOFSHAPES */
	errors[EINVALID_TRACE_DIMENTION] = "Invalid value for trace dimension";				/* 121 - EINVALID_TRACE_DIMENTION */
	errors[EINVALID_NUMEIGENVECTOR] = "Invalid value for eigen vector";					/* 122 - EINVALID_VALUE_NUMEIGENVECTOR */
	errors[EINVALID_FLOAT_SIZE] = "Invalid float size entry in model data File";		/* 123 - EINVALID_FLOAT_SIZE */
	errors[EINCOMPATIBLE_VERSION] = "Incompatible algorithm version";					/* 124 - EINCOMPATIBLE_VERSION_ERR */
	errors[EINVALID_PREPROC_SEQUENCE] = "Wrong preprocessor sequence entry in cfg file";	/* 125 - EPREPROC_SEQUENCE_ERR */

	errors[ENO_TOKEN_FOUND] = "Invalid or no value specified for project name for recognizer";	/* 126 - ENO_TOKEN_FOUND */
	errors[EINVALID_LOGICAL_NAME] = "Invalid or no value specified for logical name for recognizer";	/* 127 - EINVALID_LOGICAL_NAME */

	errors[EINVALID_SEGMENT] = "Invalid segment, boxfield recognizer requires character level segment info";				/* 128 - EINVALID_SEGMENT */
	errors[EINVALID_REC_MODE] = "Unsupported recognizer mode";						/* 129 - EINVALID_REC_MODE */

	errors[EUNSUPPORTED_STATISTICS] = "Unsupported or invalid statistics to be computed";		/* 130 - EUNSUPPORTED_STATISTICS */


	errors[EMAP_NOT_FOUND] = "No function implemented to convert to a unicode string";	/* 131 - EMAP_NOT_FOUND 	*/
	errors[EINVALID_SHAPEID] = "Invalid value for shape id";								/* 132 - EINVALID_VALUE_SHAPEID */

	errors[ENOMAPFOUND_LIPIENGINECFG] = "Cannot map the logical name, no entries in lipiengine.cfg";/* 133 - ENOMAPFOUND_LIPIENGINECFG */
	errors[EINVALID_NUM_OF_POINTS] = "Number of points in the tracegroup is not normalized";		/* 134 - EINVALID_NUM_OF_POINTS */
	errors[EEMPTY_TRACE] = "Empty trace";												/* 135 - EEMPTY_TRACE*/
	errors[EEMPTY_TRACE_GROUP] = "Empty Trace Group"	,										/* 136 -EEMPTY_TRACE_GROUP*/
	errors[ECONFIG_FILE_RANGE] = "The config file variable is not within the correct range"; /* 137 - ECONFIG_FILE_RANGE */


	errors[EINITSHAPE_NONZERO] = "Recognizer requires the Intial shape id to be zero";		/* 138	- EINITSHAPE_NONZERO */
	errors[EINVALID_LINE_LISTFILE] = "Invalid line in the listfile (train or test )";					/* 139	- EINVALID_LINE_LISTFILE  */
	errors[EINVALID_ORDER_LISTFILE] = "Invalid order of shape-ids in the list file ( train )";			/* 140	- EINVALID_ORDER_LISTFILE */
	errors[ENUM_NNS] = "Invalid number of nearest neighbours specified";			/*141 - ENUM_NNS/*/
	errors[EINKFILE_EMPTY] = "Ink file name is empty";									/* 142 - EINKFILE_EMPTY */
	errors[EINKFILE_CORRUPTED] = "Incorrect or corrupted unipen ink file.";					/* 143 - EINKFILE_CORRUPTED */
	errors[EDLL_FUNC_ADDRESS_CREATE] = "Could not map the createPreprocessor function from DLL. "; /*144 EDLL_FUNC_ADDRESS_CREATE */
	errors[EDLL_FUNC_ADDRESS_DELETE] = "Could not map the deletePreprocessor function from DLL. "; /*145 EDLL_FUNC_ADDRESS_DELETE */
	errors[ENO_RESAMPLETRACEGROUP] = "No resampleTraceGroup in preProcSequence entry of cfg file. ";   /*146 ENO_RESAMPLETRACEGROUP */

	errors[EINVALID_SAMPLING_RATE] = "Sampling rate cannot be negative. ";						/*147 EINVALID_SAMPLING_RATE */
	errors[EINVALID_X_RESOLUTION] = "m_xDpi values cannot be negative. ";						/*148 EINVALID_X_RESOLUTION */
	errors[EINVALID_Y_RESOLUTION] = "m_yDpi values cannot be negative. ";						/*149 EINVALID_Y_RESOLUTION */
	errors[EINVALID_LATENCY] = "m_latency cannot be negative. ";							/*150 EINVALID_LATENCY */

	errors[EPOINT_INDEX_OUT_OF_BOUND] = "Point index greater than number of points available. ";	/*151 EPOINT_INDEX_OUT_OF_BOUND */
	errors[ECHANNEL_INDEX_OUT_OF_BOUND] = "Invalid Channel. ";										/*152 ECHANNEL_INDEX_OUT_OF_BOUND */
	errors[ECHANNEL_SIZE_MISMATCH] = "New channel data not as long as the old one. ";			/*153 ECHANNEL_SIZE_MISMATCH */
	errors[ENUM_CHANNELS_MISMATCH] = "Point to be added does not have the same number of channels as the trace. "; /*154 ENUM_CHANNELS_MISMATCH */
	errors[EDUPLICATE_CHANNEL] = "Channel with the new channel name already present. ";		/*155 EDUPLICATE_CHANNEL */
	errors[ECHANNEL_NOT_FOUND] = "Channel not found. ";										/*156 ECHANNEL_NOT_FOUND */
	errors[EZERO_CHANNELS] = "Number of channels cannot be zero. ";						/*157 EZERO_CHANNELS */
	errors[EINVALID_INPUT_STREAM] = "Input stream does not match with number of channels in the trace. "; /*158 EINVALID_INPUT_STREAM */
	errors[ECOMPUTE_DISTANCE_ERROR] = "Error: Cannot find distance for test sample with more than 1 stroke. "; /*159 ECOMPUTE_DISTANCE_ERROR */
	errors[ECOMPARISON_ERROR] = "Error: Cannot compare with train sample having more than 1 stroke. "; /*160 ECOMPARISON_ERROR */
	errors[ETRAIN_TEST_VECTOR_SIZE_MISMATCH] = "Incompatible: train vector and test vector sizes do not match. "; /*161 ETRAIN_TEST_VECTOR_SIZE_MISMATCH */
	errors[EGRAMMER_FILE_NOT_EXIST] = "Grammar file does not exists. ";							/*162 EGRAMMER_FILE_NOT_EXIST */
	errors[EVALUES_NOT_PROVIDED] = "Values for the terminal is not Provided. ";				/*163 EVALUES_NOT_PROVIDED */
	errors[ECONFIG_FILE_FORMAT] = "No productions or terminals identified in the CFG. Please check the CFG format. "; /*164 ECONFIG_FILE_FORMAT */
	errors[ECYCLIC_DEPENDENCY] = "Cyclic dependency exists! Unable to find paths. ";			/*165 ECYCLIC_DEPENDENCY */
	errors[EFILE_OPEN_ERROR] = "Could Not open file : ";                                   /*166 EFILE_OPEN_ERROR*/
	errors[ELOAD_FEATEXT_DLL] = "Error while loading feature extractor module ";			/*167 ELOAD_FEATEXT_DLL*/
	errors[EDLL_FUNC_ADDRESS_CREATE_FEATEXT] = "Could not map the createShapeFeatureExtractor function from DLL ";	/*168 EDLL_FUNC_ADDRESS_CREATE_FEATEXT */
	errors[EDLL_FUNC_ADDRESS_DELETE_FEATEXT] = "Could not map the deleteShapeFeatureExtractor function from DLL ";	/*169 EDLL_FUNC_ADDRESS_DELETE_FEATEXT */
	errors[EFTR_EXTR_NOT_EXIST] = "Feature extractor does not exist ";									/*170 EFTR_EXTR_NOT_EXIST */
	errors[ENO_FTR_EXTR_IN_CFG] = "No Feature Extractor in Config file ";						/*171 ENO_FTR_EXTR_IN_CFG*/
	errors[EFTR_RPRCLASS_NOIMPLEMENTATION] = "No implementation provided ";								/*172 EFTR_RPRCLASS_NOIMPLEMENTATION */
	errors[EINVALID_ORDER_FEATUREFILE] = "Invalid order of shape-ids in the feature file ";			/* 173	- EINVALID_ORDER_FEATUREFILE */
	errors[ENUMSHAPES_NOT_SET] = "NumShapes config variable is  not set in the project.cfg file ";/* 174	- ENUMSHAPES_NOT_SET */
	errors[EUNEQUAL_LENGTH_VECTORS] = "Vectors are of different lengths "; /* 175 - EUNEQUAL_LENGTH_VECTORS */
	errors[EINVALID_LOG_LEVEL] = "Invalid log level "; /* 176 - EINVALID_LOG_LEVEL */
	errors[EPROJ_NOT_DYNAMIC] = "Not allowed to ADD/Delete class for a project with fixed number of shapes";  /*177 EPROJ_NOT_DYNAMIC*/
	errors[EMORPH_FVEC_SIZE_MISMATCH] = "Error: Cannot perform MORPH on features vectors of different sizes";       /*178 EMORPH_FVEC_SIZE_MISMATCH*/
	errors[ESHAPE_RECOCLASS_NOIMPLEMENTATION] = "No implementation provided";										/*179 ESHAPE_RECOCLASS_NOIMPLEMENTATION*/
	errors[ENULL_POINTER] = "Null Pointer Error";				/*180 ENULL_POINTER*/
	errors[EINVALID_X_SCALE_FACTOR] = "Invalid X scale factor. Scale factor must be greater than zero"; /*181 EINVALID_X_SCALE_FACTOR*/
	errors[EINVALID_Y_SCALE_FACTOR] = "Invalid Y scale factor. Scale factor must be greater than zero";  /*182 EINVALID_Y_SCALE_FACTOR*/
	errors[ECONFIG_MDT_MISMATCH] = "Parameter values in config file and MDT file do not match, check log file for more details";  /*183 ECONFIG_MDT_MISMATCH*/
	errors[ENEIGHBOR_INFO_VECTOR_EMPTY] = "Neighbor Info Vector is empty"; /*184 ENEIGHBOR_INFO_VECTOR_EMPTY*/
	errors[ERECO_RESULT_EMPTY] = "Recognize result is empty"; /*185 ERECO_RESULT_EMPTY*/
	errors[ESHAPE_SAMPLE_FEATURES_EMPTY] = "Features of input TraceGroup is empty"; /* 186 ESHAPE_SAMPLE_FEATURES_EMPTY*/
	errors[ENO_TOOLKIT_VERSION] = "Toolkit version missing in the control information"; /* 187 ENO_TOOLKIT_VERSION */
	errors[ETRACE_INDEX_OUT_OF_BOUND] = "Trace index greater than number of traces available. ";	/*188 ETRACE_INDEX_OUT_OF_BOUND */
	errors[EINVALID_CFG_FILE_ENTRY] = "Invalid key=value pair in the config file";	/**189 EINVALID_CFG_FILE_ENTRY*/
	errors[EKEY_NOT_FOUND] = "Key could not be found in the config file"; 	/** 190 EKEY_NOT_FOUND */
	errors[EFEATURE_INDEX_OUT_OF_BOUND] = "feature index out of bounds";  /**191 EFEATURE_INDEX_OUT_OF_BOUND*/
	errors[EINVALID_FILE_HANDLE] = "Invalid file handle"; /**192 EINVALID_FILE_HANDLE*/
	errors[EFEATURE_FILE_OPEN] = "Feature file open error"; /**193 EFEATURE_FILE_OPEN*/
	errors[EFTR_DISTANCE_NOT_DEFINED] = "Distance between the features not defined";	/**194 EFTR_DISTANCE_NOT_DEFINED*/
	errors[EINVALID_CLUSTER_ID] = "Invalid Cluster ID";	/**195 EINVALID_CLUSTER_ID */
	errors[EPROTOTYPE_SET_EMPTY] = "Prototype set is empty";	/**196 EPROTOTYPE_SET_EMPTY */
	errors[ELOG_FILE_NOT_EXIST] = "Log file does not exist"; /**197 ELOG_FILE_NOT_EXIST */
	errors[EDATA_HYPERLINK_VEC_SIZE_MISMATCH] = "Size of the data objects vector and their corresponding hyperlinks vector do not match"; /**198 EDATA_HYPERLINK_VEC_SIZE_MISMATCH*/
	errors[EFILE_CREATION_FAILED] = "File creation failed. Invalid path or no permission."; /**199 EFILE_CREATION_FAILED*/
	errors[EINVALID_NUM_CLUSTERS] = "Invalid number of clusters specified. The number must be greater than or equal to 1 and less than number of data objects."; /**200 EINVALID_NUM_CLUSTERS*/
	errors[ENO_DATA_TO_CLUSTER] = "No elements in the input data vector for clustering."; /**201 ENO_DATA_TO_CLUSTER*/
	errors[EINSUFFICIENT_DATA_FOR_LMETHOD] = "Minimum 6 data objects are required to employ LMethod."; /**202 EINSUFFICIENT_DATA_FOR_LMETHOD*/
	errors[EMODULE_NOT_IN_MEMORY] = "Module index not found in module vector"; /**203 EMODULE_NOT_IN_MEMORY */
	errors[EINVALID_LOG_FILENAME] = "Specified Log filename is empty"; /**204 EINVALID_LOG_FILENAME	*/
	errors[ECREATE_LOGGER] = "Error creating logger"; /**205 ECREATE_LOGGER */
	errors[EINVALID_PROJECT_TYPE] = "Project type in CFG is missing or an invalid value"; /**206 EINVALID_PROJECT_TYPE*/
	errors[EEMPTY_STRING] = "Empty string"; /**207 EEMPTY_STRING*/
	errors[EEMPTY_VECTOR] = "Empty vector"; /**208 EEMPTY_VECTOR*/
	errors[ENON_POSITIVE_NUM] = "Negative or zero value"; /**209 ENON_POSITIVE_NUM*/
	errors[EEMPTY_WORDREC_RESULTS] = "The word recogniton result vector is empty"; /**210 EEMPTY_WORDREC_RESULTS*/
	errors[ENEGATIVE_NUM] = "Negative value";/**211 ENEGATIVE_NUM*/
	errors[EINVALID_CLASS_ID] = "Invalid Class ID"; /**212 EINVALID_CLASS_ID*/
	errors[EINVALID_CONFIDENCE_VALUE] = "Invalid Confidence Value";  /** 213 EINVALID_CONFIDENCE_VALUE*/
	errors[ENO_SHAPE_RECO_PROJECT] = "Shape Recognizer Project name missing in the word recognizer config file."; /**214 ENO_SHAPE_RECO_PROJECT*/
	errors[EINVALID_RECOGNITION_MODE] = "Unsupported recognition mode."; /**215 EINVALID_RECOGNITION_MODE*/
	errors[ELOGGER_LIBRARY_NOT_LOADED] = "Shared library for Logger not loaded";
	errors[ESINGLE_POINT_TRACE] = "Single point trace"; /**217 ESINGLE_POINT_TRACE*/

	errors[EADAPTSCHEME_NOT_SUPPORTED] = "AdaptScheme not supported:"; /**229 EADAPTSCHEME_NOT_SUPPORTED **/
}
/**********************************************************************************
* AUTHOR		: Vijayakumara M
* DATE			: 01-Sept-2005
* NAME			: getError
* DESCRIPTION	: returns the error descriptions from the errors Table.
* ARGUMENTS		: error code.
* RETURNS		: returns pointer to an error description
* NOTES			:
* CHANGE HISTORY
* Author			Date				Description of change
*************************************************************************************/
string getErrorMessage(int errorCode)
{
	initErrorCode();
	string errorDiscrip = errors[errorCode];
	if(errorDiscrip.empty())
	{
		return "Error code is not set";
	}
	
	return errorDiscrip;
}
