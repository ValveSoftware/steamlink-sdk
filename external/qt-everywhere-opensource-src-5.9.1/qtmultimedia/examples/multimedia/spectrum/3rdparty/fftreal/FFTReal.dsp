# Microsoft Developer Studio Project File - Name="FFTReal" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=FFTReal - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "FFTReal.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "FFTReal.mak" CFG="FFTReal - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "FFTReal - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "FFTReal - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "FFTReal - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /W3 /GR /GX /O2 /Ob2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD BASE RSC /l 0x40c /d "NDEBUG"
# ADD RSC /l 0x40c /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "FFTReal - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /G6 /MTd /W3 /Gm /GR /GX /Zi /Od /Gf /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FR /YX /FD /c
# ADD BASE RSC /l 0x40c /d "_DEBUG"
# ADD RSC /l 0x40c /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "FFTReal - Win32 Release"
# Name "FFTReal - Win32 Debug"
# Begin Group "Library"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\Array.h
# End Source File
# Begin Source File

SOURCE=.\Array.hpp
# End Source File
# Begin Source File

SOURCE=.\def.h
# End Source File
# Begin Source File

SOURCE=.\DynArray.h
# End Source File
# Begin Source File

SOURCE=.\DynArray.hpp
# End Source File
# Begin Source File

SOURCE=.\FFTReal.h
# End Source File
# Begin Source File

SOURCE=.\FFTReal.hpp
# End Source File
# Begin Source File

SOURCE=.\FFTRealFixLen.h
# End Source File
# Begin Source File

SOURCE=.\FFTRealFixLen.hpp
# End Source File
# Begin Source File

SOURCE=.\FFTRealFixLenParam.h
# End Source File
# Begin Source File

SOURCE=.\FFTRealPassDirect.h
# End Source File
# Begin Source File

SOURCE=.\FFTRealPassDirect.hpp
# End Source File
# Begin Source File

SOURCE=.\FFTRealPassInverse.h
# End Source File
# Begin Source File

SOURCE=.\FFTRealPassInverse.hpp
# End Source File
# Begin Source File

SOURCE=.\FFTRealSelect.h
# End Source File
# Begin Source File

SOURCE=.\FFTRealSelect.hpp
# End Source File
# Begin Source File

SOURCE=.\FFTRealUseTrigo.h
# End Source File
# Begin Source File

SOURCE=.\FFTRealUseTrigo.hpp
# End Source File
# Begin Source File

SOURCE=.\OscSinCos.h
# End Source File
# Begin Source File

SOURCE=.\OscSinCos.hpp
# End Source File
# End Group
# Begin Group "Test"

# PROP Default_Filter ""
# Begin Group "stopwatch"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\stopwatch\ClockCycleCounter.cpp
# End Source File
# Begin Source File

SOURCE=.\stopwatch\ClockCycleCounter.h
# End Source File
# Begin Source File

SOURCE=.\stopwatch\ClockCycleCounter.hpp
# End Source File
# Begin Source File

SOURCE=.\stopwatch\def.h
# End Source File
# Begin Source File

SOURCE=.\stopwatch\fnc.h
# End Source File
# Begin Source File

SOURCE=.\stopwatch\fnc.hpp
# End Source File
# Begin Source File

SOURCE=.\stopwatch\Int64.h
# End Source File
# Begin Source File

SOURCE=.\stopwatch\StopWatch.cpp
# End Source File
# Begin Source File

SOURCE=.\stopwatch\StopWatch.h
# End Source File
# Begin Source File

SOURCE=.\stopwatch\StopWatch.hpp
# End Source File
# End Group
# Begin Source File

SOURCE=.\test.cpp
# End Source File
# Begin Source File

SOURCE=.\test_fnc.h
# End Source File
# Begin Source File

SOURCE=.\test_fnc.hpp
# End Source File
# Begin Source File

SOURCE=.\test_settings.h
# End Source File
# Begin Source File

SOURCE=.\TestAccuracy.h
# End Source File
# Begin Source File

SOURCE=.\TestAccuracy.hpp
# End Source File
# Begin Source File

SOURCE=.\TestHelperFixLen.h
# End Source File
# Begin Source File

SOURCE=.\TestHelperFixLen.hpp
# End Source File
# Begin Source File

SOURCE=.\TestHelperNormal.h
# End Source File
# Begin Source File

SOURCE=.\TestHelperNormal.hpp
# End Source File
# Begin Source File

SOURCE=.\TestSpeed.h
# End Source File
# Begin Source File

SOURCE=.\TestSpeed.hpp
# End Source File
# Begin Source File

SOURCE=.\TestWhiteNoiseGen.h
# End Source File
# Begin Source File

SOURCE=.\TestWhiteNoiseGen.hpp
# End Source File
# End Group
# End Target
# End Project
