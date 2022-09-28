# Microsoft Visual C++ Generated NMAKE File, Format Version 2.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

!IF "$(CFG)" == ""
CFG=Win32 Debug
!MESSAGE No configuration specified.  Defaulting to Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "Win32 Release" && "$(CFG)" != "Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "xntpd.mak" CFG="Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

################################################################################
# Begin Project
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "WinRel"
# PROP BASE Intermediate_Dir "WinRel"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "WinRel"
# PROP Intermediate_Dir "WinRel"
OUTDIR=.\WinRel
INTDIR=.\WinRel

ALL : $(OUTDIR)/xntpd.exe $(OUTDIR)/xntpd.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

# ADD BASE CPP /nologo /ML /W3 /GX /YX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /FR /c
# ADD CPP /nologo /MT /W3 /GX /YX /O2 /I "..\include" /D "NDEBUG" /D "WIN32" /D "_CONSOLE" /D "SYS_WINNT" /D "DES" /D "MD5" /D "NTP_LITTLE_ENDIAN" /D "DEBUG" /D "SYSLOG_FILE" /FR /c
CPP_PROJ=/nologo /MT /W3 /GX /YX /O2 /I "..\include" /D "NDEBUG" /D "WIN32" /D\
 "_CONSOLE" /D "SYS_WINNT" /D "DES" /D "MD5" /D "NTP_LITTLE_ENDIAN" /D "DEBUG"\
 /D "SYSLOG_FILE" /FR$(INTDIR)/ /Fp$(OUTDIR)/"xntpd.pch" /Fo$(INTDIR)/ /c 
CPP_OBJS=.\WinRel/
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"xntpd.bsc" 
BSC32_SBRS= \
	$(INTDIR)/refclock_leitch.sbr \
	$(INTDIR)/refclock_moto.sbr \
	$(INTDIR)/ntp_proto.sbr \
	$(INTDIR)/refclock_gpstm.sbr \
	$(INTDIR)/refclock_acts.sbr \
	$(INTDIR)/ntp_leap.sbr \
	$(INTDIR)/refclock_atom.sbr \
	$(INTDIR)/ntp_peer.sbr \
	$(INTDIR)/refclock_irig.sbr \
	$(INTDIR)/ntp_intres.sbr \
	$(INTDIR)/ntp_unixclock.sbr \
	$(INTDIR)/refclock_nmea.sbr \
	$(INTDIR)/ntp_timer.sbr \
	$(INTDIR)/refclock_mx4200.sbr \
	$(INTDIR)/ntp_monitor.sbr \
	$(INTDIR)/refclock_local.sbr \
	$(INTDIR)/refclock_usno.sbr \
	$(INTDIR)/refclock_omega.sbr \
	$(INTDIR)/refclock_ptbacts.sbr \
	$(INTDIR)/ntp_io.sbr \
	$(INTDIR)/refclock_pst.sbr \
	$(INTDIR)/ntp_control.sbr \
	$(INTDIR)/ntp_restrict.sbr \
	$(INTDIR)/refclock_chu.sbr \
	$(INTDIR)/refclock_conf.sbr \
	$(INTDIR)/refclock_trak.sbr \
	$(INTDIR)/ntpd.sbr \
	$(INTDIR)/refclock_wwvb.sbr \
	$(INTDIR)/ntp_config.sbr \
	$(INTDIR)/ntp_request.sbr \
	$(INTDIR)/refclock_tpro.sbr \
	$(INTDIR)/refclock_heath.sbr \
	$(INTDIR)/refclock_goes.sbr \
	$(INTDIR)/refclock_datum.sbr \
	$(INTDIR)/refclock_as2201.sbr \
	$(INTDIR)/refclock_parse.sbr \
	$(INTDIR)/ntp_loopfilter.sbr \
	$(INTDIR)/ntp_refclock.sbr \
	$(INTDIR)/ntp_util.sbr \
	$(INTDIR)/refclock_msfees.sbr \
	$(INTDIR)/ntp_filegen.sbr

$(OUTDIR)/xntpd.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /NOLOGO /SUBSYSTEM:console /MACHINE:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib winmm.lib ..\lib\WinDebug\libntp.lib /NOLOGO /SUBSYSTEM:console /MACHINE:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib wsock32.lib winmm.lib ..\lib\WinDebug\libntp.lib /NOLOGO\
 /SUBSYSTEM:console /INCREMENTAL:no /PDB:$(OUTDIR)/"xntpd.pdb" /MACHINE:I386\
 /OUT:$(OUTDIR)/"xntpd.exe" 
DEF_FILE=
LINK32_OBJS= \
	$(INTDIR)/refclock_leitch.obj \
	$(INTDIR)/refclock_moto.obj \
	$(INTDIR)/ntp_proto.obj \
	$(INTDIR)/refclock_gpstm.obj \
	$(INTDIR)/refclock_acts.obj \
	$(INTDIR)/ntp_leap.obj \
	$(INTDIR)/refclock_atom.obj \
	$(INTDIR)/ntp_peer.obj \
	$(INTDIR)/refclock_irig.obj \
	$(INTDIR)/ntp_intres.obj \
	$(INTDIR)/ntp_unixclock.obj \
	$(INTDIR)/refclock_nmea.obj \
	$(INTDIR)/ntp_timer.obj \
	$(INTDIR)/refclock_mx4200.obj \
	$(INTDIR)/ntp_monitor.obj \
	$(INTDIR)/refclock_local.obj \
	$(INTDIR)/refclock_usno.obj \
	$(INTDIR)/refclock_omega.obj \
	$(INTDIR)/refclock_ptbacts.obj \
	$(INTDIR)/ntp_io.obj \
	$(INTDIR)/refclock_pst.obj \
	$(INTDIR)/ntp_control.obj \
	$(INTDIR)/ntp_restrict.obj \
	$(INTDIR)/refclock_chu.obj \
	$(INTDIR)/refclock_conf.obj \
	$(INTDIR)/refclock_trak.obj \
	$(INTDIR)/ntpd.obj \
	$(INTDIR)/refclock_wwvb.obj \
	$(INTDIR)/ntp_config.obj \
	$(INTDIR)/ntp_request.obj \
	$(INTDIR)/refclock_tpro.obj \
	$(INTDIR)/refclock_heath.obj \
	$(INTDIR)/refclock_goes.obj \
	$(INTDIR)/refclock_datum.obj \
	$(INTDIR)/refclock_as2201.obj \
	$(INTDIR)/refclock_parse.obj \
	$(INTDIR)/ntp_loopfilter.obj \
	$(INTDIR)/ntp_refclock.obj \
	$(INTDIR)/ntp_util.obj \
	$(INTDIR)/refclock_msfees.obj \
	$(INTDIR)/ntp_filegen.obj

$(OUTDIR)/xntpd.exe : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "WinDebug"
# PROP BASE Intermediate_Dir "WinDebug"
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "WinDebug"
# PROP Intermediate_Dir "WinDebug"
OUTDIR=.\WinDebug
INTDIR=.\WinDebug

ALL : $(OUTDIR)/xntpd.exe $(OUTDIR)/xntpd.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

# ADD BASE CPP /nologo /ML /W3 /GX /Zi /YX /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /FR /c
# ADD CPP /nologo /MT /W3 /GX /Zi /YX /Od /I "..\include" /D "_DEBUG" /D "WIN32" /D "_CONSOLE" /D "SYS_WINNT" /D "DES" /D "MD5" /D "NTP_LITTLE_ENDIAN" /D "DEBUG" /D "SYSLOG_FILE" /FR /c
CPP_PROJ=/nologo /MT /W3 /GX /Zi /YX /Od /I "..\include" /D "_DEBUG" /D "WIN32"\
 /D "_CONSOLE" /D "SYS_WINNT" /D "DES" /D "MD5" /D "NTP_LITTLE_ENDIAN" /D\
 "DEBUG" /D "SYSLOG_FILE" /FR$(INTDIR)/ /Fp$(OUTDIR)/"xntpd.pch" /Fo$(INTDIR)/\
 /Fd$(OUTDIR)/"xntpd.pdb" /c 
CPP_OBJS=.\WinDebug/
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"xntpd.bsc" 
BSC32_SBRS= \
	$(INTDIR)/refclock_leitch.sbr \
	$(INTDIR)/refclock_moto.sbr \
	$(INTDIR)/ntp_proto.sbr \
	$(INTDIR)/refclock_gpstm.sbr \
	$(INTDIR)/refclock_acts.sbr \
	$(INTDIR)/ntp_leap.sbr \
	$(INTDIR)/refclock_atom.sbr \
	$(INTDIR)/ntp_peer.sbr \
	$(INTDIR)/refclock_irig.sbr \
	$(INTDIR)/ntp_intres.sbr \
	$(INTDIR)/ntp_unixclock.sbr \
	$(INTDIR)/refclock_nmea.sbr \
	$(INTDIR)/ntp_timer.sbr \
	$(INTDIR)/refclock_mx4200.sbr \
	$(INTDIR)/ntp_monitor.sbr \
	$(INTDIR)/refclock_local.sbr \
	$(INTDIR)/refclock_usno.sbr \
	$(INTDIR)/refclock_omega.sbr \
	$(INTDIR)/refclock_ptbacts.sbr \
	$(INTDIR)/ntp_io.sbr \
	$(INTDIR)/refclock_pst.sbr \
	$(INTDIR)/ntp_control.sbr \
	$(INTDIR)/ntp_restrict.sbr \
	$(INTDIR)/refclock_chu.sbr \
	$(INTDIR)/refclock_conf.sbr \
	$(INTDIR)/refclock_trak.sbr \
	$(INTDIR)/ntpd.sbr \
	$(INTDIR)/refclock_wwvb.sbr \
	$(INTDIR)/ntp_config.sbr \
	$(INTDIR)/ntp_request.sbr \
	$(INTDIR)/refclock_tpro.sbr \
	$(INTDIR)/refclock_heath.sbr \
	$(INTDIR)/refclock_goes.sbr \
	$(INTDIR)/refclock_datum.sbr \
	$(INTDIR)/refclock_as2201.sbr \
	$(INTDIR)/refclock_parse.sbr \
	$(INTDIR)/ntp_loopfilter.sbr \
	$(INTDIR)/ntp_refclock.sbr \
	$(INTDIR)/ntp_util.sbr \
	$(INTDIR)/refclock_msfees.sbr \
	$(INTDIR)/ntp_filegen.sbr

$(OUTDIR)/xntpd.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /NOLOGO /SUBSYSTEM:console /DEBUG /MACHINE:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib winmm.lib ..\lib\WinDebug\libntp.lib /NOLOGO /SUBSYSTEM:console /DEBUG /MACHINE:I386
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib\
 advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib\
 odbccp32.lib wsock32.lib winmm.lib ..\lib\WinDebug\libntp.lib /NOLOGO\
 /SUBSYSTEM:console /INCREMENTAL:yes /PDB:$(OUTDIR)/"xntpd.pdb" /DEBUG\
 /MACHINE:I386 /OUT:$(OUTDIR)/"xntpd.exe" 
DEF_FILE=
LINK32_OBJS= \
	$(INTDIR)/refclock_leitch.obj \
	$(INTDIR)/refclock_moto.obj \
	$(INTDIR)/ntp_proto.obj \
	$(INTDIR)/refclock_gpstm.obj \
	$(INTDIR)/refclock_acts.obj \
	$(INTDIR)/ntp_leap.obj \
	$(INTDIR)/refclock_atom.obj \
	$(INTDIR)/ntp_peer.obj \
	$(INTDIR)/refclock_irig.obj \
	$(INTDIR)/ntp_intres.obj \
	$(INTDIR)/ntp_unixclock.obj \
	$(INTDIR)/refclock_nmea.obj \
	$(INTDIR)/ntp_timer.obj \
	$(INTDIR)/refclock_mx4200.obj \
	$(INTDIR)/ntp_monitor.obj \
	$(INTDIR)/refclock_local.obj \
	$(INTDIR)/refclock_usno.obj \
	$(INTDIR)/refclock_omega.obj \
	$(INTDIR)/refclock_ptbacts.obj \
	$(INTDIR)/ntp_io.obj \
	$(INTDIR)/refclock_pst.obj \
	$(INTDIR)/ntp_control.obj \
	$(INTDIR)/ntp_restrict.obj \
	$(INTDIR)/refclock_chu.obj \
	$(INTDIR)/refclock_conf.obj \
	$(INTDIR)/refclock_trak.obj \
	$(INTDIR)/ntpd.obj \
	$(INTDIR)/refclock_wwvb.obj \
	$(INTDIR)/ntp_config.obj \
	$(INTDIR)/ntp_request.obj \
	$(INTDIR)/refclock_tpro.obj \
	$(INTDIR)/refclock_heath.obj \
	$(INTDIR)/refclock_goes.obj \
	$(INTDIR)/refclock_datum.obj \
	$(INTDIR)/refclock_as2201.obj \
	$(INTDIR)/refclock_parse.obj \
	$(INTDIR)/ntp_loopfilter.obj \
	$(INTDIR)/ntp_refclock.obj \
	$(INTDIR)/ntp_util.obj \
	$(INTDIR)/refclock_msfees.obj \
	$(INTDIR)/ntp_filegen.obj

$(OUTDIR)/xntpd.exe : $(OUTDIR)  $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cpp{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

.cxx{$(CPP_OBJS)}.obj:
   $(CPP) $(CPP_PROJ) $<  

################################################################################
# Begin Group "Source Files"

################################################################################
# Begin Source File

SOURCE=.\refclock_leitch.c

$(INTDIR)/refclock_leitch.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_moto.c

$(INTDIR)/refclock_moto.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ntp_proto.c

$(INTDIR)/ntp_proto.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_gpstm.c

$(INTDIR)/refclock_gpstm.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_acts.c

$(INTDIR)/refclock_acts.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ntp_leap.c

$(INTDIR)/ntp_leap.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_atom.c

$(INTDIR)/refclock_atom.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ntp_peer.c

$(INTDIR)/ntp_peer.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_irig.c

$(INTDIR)/refclock_irig.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ntp_intres.c

$(INTDIR)/ntp_intres.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ntp_unixclock.c

$(INTDIR)/ntp_unixclock.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_nmea.c

$(INTDIR)/refclock_nmea.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ntp_timer.c

$(INTDIR)/ntp_timer.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_mx4200.c

$(INTDIR)/refclock_mx4200.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ntp_monitor.c

$(INTDIR)/ntp_monitor.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_local.c

$(INTDIR)/refclock_local.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_usno.c

$(INTDIR)/refclock_usno.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_omega.c

$(INTDIR)/refclock_omega.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_ptbacts.c

$(INTDIR)/refclock_ptbacts.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ntp_io.c

$(INTDIR)/ntp_io.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_pst.c

$(INTDIR)/refclock_pst.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ntp_control.c

$(INTDIR)/ntp_control.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ntp_restrict.c

$(INTDIR)/ntp_restrict.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_chu.c

$(INTDIR)/refclock_chu.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_conf.c

$(INTDIR)/refclock_conf.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_trak.c

$(INTDIR)/refclock_trak.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ntpd.c

$(INTDIR)/ntpd.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_wwvb.c

$(INTDIR)/refclock_wwvb.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ntp_config.c

$(INTDIR)/ntp_config.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ntp_request.c

$(INTDIR)/ntp_request.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_tpro.c

$(INTDIR)/refclock_tpro.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_heath.c

$(INTDIR)/refclock_heath.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_goes.c

$(INTDIR)/refclock_goes.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_datum.c

$(INTDIR)/refclock_datum.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_as2201.c

$(INTDIR)/refclock_as2201.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_parse.c

$(INTDIR)/refclock_parse.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ntp_loopfilter.c

$(INTDIR)/ntp_loopfilter.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ntp_refclock.c

$(INTDIR)/ntp_refclock.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ntp_util.c

$(INTDIR)/ntp_util.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refclock_msfees.c

$(INTDIR)/refclock_msfees.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ntp_filegen.c

$(INTDIR)/ntp_filegen.obj :  $(SOURCE)  $(INTDIR)

# End Source File
# End Group
# End Project
################################################################################
