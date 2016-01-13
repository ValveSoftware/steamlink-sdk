// Copyright (c) 2014, Intel Corporation
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Intel Corporation nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "PowerGadgetLib.h"
#include <Windows.h>
#include <string>
#include <vector>

using namespace std;

string g_lastError;
HMODULE g_hModule = NULL;

static bool split(const wstring& s, wstring &path)
{
	bool bResult = false;
	vector<wstring> output;

	wstring::size_type prev_pos = 0, pos = 0;

	while((pos = s.find(L';', pos)) != wstring::npos)
	{
		wstring substring( s.substr(prev_pos, pos-prev_pos) );
		if (substring.find(L"Power Gadget 2.") != wstring::npos)
		{
			path = substring;
			bResult = true;
			break;
		}
		prev_pos = ++pos;
	}

	if (!bResult)
	{
		wstring substring(s.substr(prev_pos, pos-prev_pos));

		if (substring.find(L"Power Gadget 2.") != wstring::npos)
		{
			path = substring;
			bResult = true;
		}
	}	

	if (bResult)
	{
		basic_string <char>::size_type pos = path.rfind(L" ");
		wstring version = path.substr(pos+1, path.length());
		double fVer = _wtof(version.c_str());
		if (fVer > 2.6)
			bResult = true;
	}
	
	return bResult;
}

static bool GetLibraryLocation(wstring& strLocation)
{
	TCHAR *pszPath = _wgetenv(L"IPG_Dir");
	if (pszPath == NULL || wcslen(pszPath) == 0)
		return false;

	TCHAR *pszVersion = _wgetenv(L"IPG_Ver");
	if (pszVersion == NULL || wcslen(pszVersion) == 0)
		return false;

	int version = _wtof(pszVersion) * 100;
	if (version >= 270)
	{
#if _M_X64
		strLocation = wstring(pszPath) + L"\\EnergyLib64.dll";	
#else
		strLocation = wstring(pszPath) + L"\\EnergyLib32.dll";
#endif
		return true;
	}
	else
		return false;
}

CIntelPowerGadgetLib::CIntelPowerGadgetLib(void) :
	pInitialize(NULL),
	pGetNumNodes(NULL),
	pGetMsrName(NULL),
	pGetMsrFunc(NULL),
	pGetIAFrequency(NULL),
	pGetTDP(NULL),
	pGetMaxTemperature(NULL),
	pGetTemperature(NULL),
	pReadSample(NULL),
	pGetSysTime(NULL),
	pGetRDTSC(NULL),
	pGetTimeInterval(NULL),
	pGetBaseFrequency(NULL),
	pGetPowerData(NULL),
	pStartLog(NULL),
	pStopLog(NULL),
	pGetNumMsrs(NULL)
{
	wstring strLocation;
	if (GetLibraryLocation(strLocation) == false)
	{
		g_lastError = "Intel Power Gadget 2.7 or higher not found. If unsure, check if the path is in the user's path environment variable";
		return;
	}

	g_hModule = LoadLibrary(strLocation.c_str());
	if (g_hModule == NULL)
	{
		g_lastError = "LoadLibrary failed"; 
		return;
	}
	
    pInitialize = (IPGInitialize) GetProcAddress(g_hModule, "IntelEnergyLibInitialize");
    pGetNumNodes = (IPGGetNumNodes) GetProcAddress(g_hModule, "GetNumNodes");
    pGetMsrName = (IPGGetMsrName) GetProcAddress(g_hModule, "GetMsrName");
    pGetMsrFunc = (IPGGetMsrFunc) GetProcAddress(g_hModule, "GetMsrFunc");
    pGetIAFrequency = (IPGGetIAFrequency) GetProcAddress(g_hModule, "GetIAFrequency");
    pGetTDP = (IPGGetTDP) GetProcAddress(g_hModule, "GetTDP");
    pGetMaxTemperature = (IPGGetMaxTemperature) GetProcAddress(g_hModule, "GetMaxTemperature");
    pGetTemperature = (IPGGetTemperature) GetProcAddress(g_hModule, "GetTemperature");
    pReadSample = (IPGReadSample) GetProcAddress(g_hModule, "ReadSample");
    pGetSysTime = (IPGGetSysTime) GetProcAddress(g_hModule, "GetSysTime");
    pGetRDTSC = (IPGGetRDTSC) GetProcAddress(g_hModule, "GetRDTSC");
    pGetTimeInterval = (IPGGetTimeInterval) GetProcAddress(g_hModule, "GetTimeInterval");
    pGetBaseFrequency = (IPGGetBaseFrequency) GetProcAddress(g_hModule, "GetBaseFrequency");
    pGetPowerData = (IPGGetPowerData) GetProcAddress(g_hModule, "GetPowerData");
    pStartLog = (IPGStartLog) GetProcAddress(g_hModule, "StartLog");
    pStopLog = (IPGStopLog) GetProcAddress(g_hModule, "StopLog");
    pGetNumMsrs = (IPGGetNumMsrs) GetProcAddress(g_hModule, "GetNumMsrs");
}


CIntelPowerGadgetLib::~CIntelPowerGadgetLib(void)
{
	if (g_hModule != NULL)
		FreeLibrary(g_hModule);
}



string CIntelPowerGadgetLib::GetLastError()
{
	return g_lastError;
}

bool CIntelPowerGadgetLib::IntelEnergyLibInitialize(void)
{
	if (pInitialize == NULL)
		return false;
	
	bool bSuccess = pInitialize();
	if (!bSuccess)
	{
		g_lastError = "Initializing the energy library failed";
		return false;
	}
	
	return true;
}


bool CIntelPowerGadgetLib::GetNumNodes(int * nNodes)
{
	return pGetNumNodes(nNodes);
}

bool CIntelPowerGadgetLib::GetNumMsrs(int * nMsrs)
{
	return pGetNumMsrs(nMsrs);
}

bool CIntelPowerGadgetLib::GetMsrName(int iMsr, wchar_t *pszName)
{
	return pGetMsrName(iMsr, pszName);
}

bool CIntelPowerGadgetLib::GetMsrFunc(int iMsr, int *funcID)
{
	return pGetMsrFunc(iMsr, funcID);
}

bool CIntelPowerGadgetLib::GetIAFrequency(int iNode, int *freqInMHz)
{
	return pGetIAFrequency(iNode, freqInMHz);
}

bool CIntelPowerGadgetLib::GetTDP(int iNode, double *TDP)
{
	return pGetTDP(iNode, TDP);
}

bool CIntelPowerGadgetLib::GetMaxTemperature(int iNode, int *degreeC)
{
	return pGetMaxTemperature(iNode, degreeC);
}

bool CIntelPowerGadgetLib::GetTemperature(int iNode, int *degreeC)
{
	return pGetTemperature(iNode, degreeC);
}

bool CIntelPowerGadgetLib::ReadSample()
{
	bool bSuccess = pReadSample();
	if (bSuccess == false)
		g_lastError = "MSR overflowed. You can safely discard this sample";
	return bSuccess;
}

bool CIntelPowerGadgetLib::GetSysTime(SYSTEMTIME *sysTime)
{
	return pGetSysTime(sysTime);
}

bool CIntelPowerGadgetLib::GetRDTSC(UINT64 *TSC)
{
	return pGetRDTSC(TSC);
}

bool CIntelPowerGadgetLib::GetTimeInterval(double *offset)
{
	return pGetTimeInterval(offset);
}

bool CIntelPowerGadgetLib::GetBaseFrequency(int iNode, double *baseFrequency)
{
	return pGetBaseFrequency(iNode, baseFrequency);
}

bool CIntelPowerGadgetLib::GetPowerData(int iNode, int iMSR, double *results, int *nResult)
{
	return pGetPowerData(iNode, iMSR, results, nResult);
}

bool CIntelPowerGadgetLib::StartLog(wchar_t *szFilename)
{
	return pStartLog(szFilename);
}

bool CIntelPowerGadgetLib::StopLog()
{
	return pStopLog();
}
