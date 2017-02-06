
#ifndef __NEURALNET_H__
#define __NEURALNET_H__


// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the NEURALNET_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// NEURALNET_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef _WIN32
#ifdef NEURALNET_EXPORTS
#define NEURALNET_API __declspec(dllexport)
#else
#define NEURALNET_API __declspec(dllimport)
#endif //#ifdef NEURALNET_EXPORTS
#else
#define NEURALNET_API
#endif	//#ifdef _WIN32

class LTKTraceGroup;
class LTKShapeRecognizer;

#include "LTKInc.h"
#include "LTKTypes.h"

/** @defgroup NeuralNetShapeRecognizer NeuralNetShapeRecognizer
*@brief The NeuralNetShapeRecognizer
*/

/**
* @ingroup NeuralNetShapeRecognizer
* @file NEURALNET.cpp
*/

/**
 *  Crates instance of type NeuralNetShapeRecognizer and returns of type 
 *  LTKShpeRecognizer. (Acts as a Factory Method).
 *
 * @param  none
 *
 * @return  LTKShapeRecognizer - an instance of type LTKShapeRecognizer.
 */
extern "C" NEURALNET_API int createShapeRecognizer(const LTKControlInfo& controlInfo, 
												   LTKShapeRecognizer** pReco );

/**
 * Destroy the instance by taking the address as its argument.
 *
 * @param  obj - Address of LTKShapeRecognizer instance.
 *
 * @return  0 on Success
 */
extern "C" NEURALNET_API int deleteShapeRecognizer(LTKShapeRecognizer *obj);

extern "C" NEURALNET_API int getTraceGroups(LTKShapeRecognizer *obj, int shapeID, int numberOfTraceGroups,
									 vector<LTKTraceGroup> &outTraceGroups);

void unloadDLLs();

#endif //#ifndef __NEURALNET_H__