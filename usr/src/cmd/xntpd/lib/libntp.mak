# Microsoft Visual C++ Generated NMAKE File, Format Version 2.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

!IF "$(CFG)" == ""
CFG=Win32 Debug
!MESSAGE No configuration specified.  Defaulting to Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "Win32 Release" && "$(CFG)" != "Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE on this makefile
!MESSAGE by defining the macro CFG on the command line.  For example:
!MESSAGE 
!MESSAGE NMAKE /f "libntp.mak" CFG="Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

################################################################################
# Begin Project
CPP=cl.exe

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

ALL : $(OUTDIR)/libntp.lib $(OUTDIR)/libntp.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

# ADD BASE CPP /nologo /ML /W3 /GX /YX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /FR /c
# ADD CPP /nologo /MT /W3 /GX /YX /O2 /I "..\include" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "SYS_WINNT" /D "DES" /D "MD5" /D "NTP_LITTLE_ENDIAN" /D "DEBUG" /D "SYSLOG_FILE" /D "__STDC__" /FR /c
CPP_PROJ=/nologo /MT /W3 /GX /YX /O2 /I "..\include" /D "NDEBUG" /D "WIN32" /D\
 "_WINDOWS" /D "SYS_WINNT" /D "DES" /D "MD5" /D "NTP_LITTLE_ENDIAN" /D "DEBUG"\
 /D "SYSLOG_FILE" /D "__STDC__" /FR$(INTDIR)/ /Fp$(OUTDIR)/"libntp.pch"\
 /Fo$(INTDIR)/ /c 
CPP_OBJS=.\WinRel/
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"libntp.bsc" 
BSC32_SBRS= \
	$(INTDIR)/mfptoa.sbr \
	$(INTDIR)/lib_strbuf.sbr \
	$(INTDIR)/decodenetnum.sbr \
	$(INTDIR)/gettstamp.sbr \
	$(INTDIR)/clocktime.sbr \
	$(INTDIR)/uinttoa.sbr \
	$(INTDIR)/numtoa.sbr \
	$(INTDIR)/octtoint.sbr \
	$(INTDIR)/inttoa.sbr \
	$(INTDIR)/mstolfp.sbr \
	$(INTDIR)/authdecrypt.sbr \
	$(INTDIR)/tstotv.sbr \
	$(INTDIR)/a_md512crypt.sbr \
	$(INTDIR)/modetoa.sbr \
	$(INTDIR)/atouint.sbr \
	$(INTDIR)/getopt.sbr \
	$(INTDIR)/uglydate.sbr \
	$(INTDIR)/machines.sbr \
	$(INTDIR)/dolfptoa.sbr \
	$(INTDIR)/a_md5decrypt.sbr \
	$(INTDIR)/mexit.sbr \
	$(INTDIR)/syssignal.sbr \
	$(INTDIR)/msutotsf.sbr \
	$(INTDIR)/atoint.sbr \
	$(INTDIR)/auth12crypt.sbr \
	$(INTDIR)/tsftomsu.sbr \
	$(INTDIR)/systime.sbr \
	$(INTDIR)/tvtots.sbr \
	$(INTDIR)/findconfig.sbr \
	$(INTDIR)/authkeys.sbr \
	$(INTDIR)/a_md5encrypt.sbr \
	$(INTDIR)/ranny.sbr \
	$(INTDIR)/tvtoa.sbr \
	$(INTDIR)/refnumtoa.sbr \
	$(INTDIR)/netof.sbr \
	$(INTDIR)/hextoint.sbr \
	$(INTDIR)/caltontp.sbr \
	$(INTDIR)/emalloc.sbr \
	$(INTDIR)/calleapwhen.sbr \
	$(INTDIR)/md5.sbr \
	$(INTDIR)/fptoa.sbr \
	$(INTDIR)/authdes.sbr \
	$(INTDIR)/authusekey.sbr \
	$(INTDIR)/buftvtots.sbr \
	$(INTDIR)/calyearstart.sbr \
	$(INTDIR)/utvtoa.sbr \
	$(INTDIR)/atolfp.sbr \
	$(INTDIR)/msyslog.sbr \
	$(INTDIR)/clocktypes.sbr \
	$(INTDIR)/caljulian.sbr \
	$(INTDIR)/authencrypt.sbr \
	$(INTDIR)/prettydate.sbr \
	$(INTDIR)/hextolfp.sbr \
	$(INTDIR)/fptoms.sbr \
	$(INTDIR)/numtohost.sbr \
	$(INTDIR)/authparity.sbr \
	$(INTDIR)/authreadkeys.sbr \
	$(INTDIR)/dofptoa.sbr \
	$(INTDIR)/mfptoms.sbr \
	$(INTDIR)/statestr.sbr \
	$(INTDIR)/humandate.sbr

$(OUTDIR)/libntp.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LIB32=lib.exe
# ADD BASE LIB32 /NOLOGO
# ADD LIB32 /NOLOGO
LIB32_FLAGS=/NOLOGO /OUT:$(OUTDIR)\"libntp.lib" 
DEF_FLAGS=
DEF_FILE=
LIB32_OBJS= \
	$(INTDIR)/mfptoa.obj \
	$(INTDIR)/lib_strbuf.obj \
	$(INTDIR)/decodenetnum.obj \
	$(INTDIR)/gettstamp.obj \
	$(INTDIR)/clocktime.obj \
	$(INTDIR)/uinttoa.obj \
	$(INTDIR)/numtoa.obj \
	$(INTDIR)/octtoint.obj \
	$(INTDIR)/inttoa.obj \
	$(INTDIR)/mstolfp.obj \
	$(INTDIR)/authdecrypt.obj \
	$(INTDIR)/tstotv.obj \
	$(INTDIR)/a_md512crypt.obj \
	$(INTDIR)/modetoa.obj \
	$(INTDIR)/atouint.obj \
	$(INTDIR)/getopt.obj \
	$(INTDIR)/uglydate.obj \
	$(INTDIR)/machines.obj \
	$(INTDIR)/dolfptoa.obj \
	$(INTDIR)/a_md5decrypt.obj \
	$(INTDIR)/mexit.obj \
	$(INTDIR)/syssignal.obj \
	$(INTDIR)/msutotsf.obj \
	$(INTDIR)/atoint.obj \
	$(INTDIR)/auth12crypt.obj \
	$(INTDIR)/tsftomsu.obj \
	$(INTDIR)/systime.obj \
	$(INTDIR)/tvtots.obj \
	$(INTDIR)/findconfig.obj \
	$(INTDIR)/authkeys.obj \
	$(INTDIR)/a_md5encrypt.obj \
	$(INTDIR)/ranny.obj \
	$(INTDIR)/tvtoa.obj \
	$(INTDIR)/refnumtoa.obj \
	$(INTDIR)/netof.obj \
	$(INTDIR)/hextoint.obj \
	$(INTDIR)/caltontp.obj \
	$(INTDIR)/emalloc.obj \
	$(INTDIR)/calleapwhen.obj \
	$(INTDIR)/md5.obj \
	$(INTDIR)/fptoa.obj \
	$(INTDIR)/authdes.obj \
	$(INTDIR)/authusekey.obj \
	$(INTDIR)/buftvtots.obj \
	$(INTDIR)/calyearstart.obj \
	$(INTDIR)/utvtoa.obj \
	$(INTDIR)/atolfp.obj \
	$(INTDIR)/msyslog.obj \
	$(INTDIR)/clocktypes.obj \
	$(INTDIR)/caljulian.obj \
	$(INTDIR)/authencrypt.obj \
	$(INTDIR)/prettydate.obj \
	$(INTDIR)/hextolfp.obj \
	$(INTDIR)/fptoms.obj \
	$(INTDIR)/numtohost.obj \
	$(INTDIR)/authparity.obj \
	$(INTDIR)/authreadkeys.obj \
	$(INTDIR)/dofptoa.obj \
	$(INTDIR)/mfptoms.obj \
	$(INTDIR)/statestr.obj \
	$(INTDIR)/humandate.obj

$(OUTDIR)/libntp.lib : $(OUTDIR)  $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
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

ALL : $(OUTDIR)/libntp.lib $(OUTDIR)/libntp.bsc

$(OUTDIR) : 
    if not exist $(OUTDIR)/nul mkdir $(OUTDIR)

# ADD BASE CPP /nologo /ML /W3 /GX /Z7 /YX /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /c
# ADD CPP /nologo /MT /W3 /GX /Z7 /YX /Od /I "..\include" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "SYS_WINNT" /D "DES" /D "MD5" /D "NTP_LITTLE_ENDIAN" /D "DEBUG" /D "SYSLOG_FILE" /D "__STDC__" /FR /c
CPP_PROJ=/nologo /MT /W3 /GX /Z7 /YX /Od /I "..\include" /D "_DEBUG" /D "WIN32"\
 /D "_WINDOWS" /D "SYS_WINNT" /D "DES" /D "MD5" /D "NTP_LITTLE_ENDIAN" /D\
 "DEBUG" /D "SYSLOG_FILE" /D "__STDC__" /FR$(INTDIR)/ /Fp$(OUTDIR)/"libntp.pch"\
 /Fo$(INTDIR)/ /c 
CPP_OBJS=.\WinDebug/
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
BSC32_FLAGS=/nologo /o$(OUTDIR)/"libntp.bsc" 
BSC32_SBRS= \
	$(INTDIR)/mfptoa.sbr \
	$(INTDIR)/lib_strbuf.sbr \
	$(INTDIR)/decodenetnum.sbr \
	$(INTDIR)/gettstamp.sbr \
	$(INTDIR)/clocktime.sbr \
	$(INTDIR)/uinttoa.sbr \
	$(INTDIR)/numtoa.sbr \
	$(INTDIR)/octtoint.sbr \
	$(INTDIR)/inttoa.sbr \
	$(INTDIR)/mstolfp.sbr \
	$(INTDIR)/authdecrypt.sbr \
	$(INTDIR)/tstotv.sbr \
	$(INTDIR)/a_md512crypt.sbr \
	$(INTDIR)/modetoa.sbr \
	$(INTDIR)/atouint.sbr \
	$(INTDIR)/getopt.sbr \
	$(INTDIR)/uglydate.sbr \
	$(INTDIR)/machines.sbr \
	$(INTDIR)/dolfptoa.sbr \
	$(INTDIR)/a_md5decrypt.sbr \
	$(INTDIR)/mexit.sbr \
	$(INTDIR)/syssignal.sbr \
	$(INTDIR)/msutotsf.sbr \
	$(INTDIR)/atoint.sbr \
	$(INTDIR)/auth12crypt.sbr \
	$(INTDIR)/tsftomsu.sbr \
	$(INTDIR)/systime.sbr \
	$(INTDIR)/tvtots.sbr \
	$(INTDIR)/findconfig.sbr \
	$(INTDIR)/authkeys.sbr \
	$(INTDIR)/a_md5encrypt.sbr \
	$(INTDIR)/ranny.sbr \
	$(INTDIR)/tvtoa.sbr \
	$(INTDIR)/refnumtoa.sbr \
	$(INTDIR)/netof.sbr \
	$(INTDIR)/hextoint.sbr \
	$(INTDIR)/caltontp.sbr \
	$(INTDIR)/emalloc.sbr \
	$(INTDIR)/calleapwhen.sbr \
	$(INTDIR)/md5.sbr \
	$(INTDIR)/fptoa.sbr \
	$(INTDIR)/authdes.sbr \
	$(INTDIR)/authusekey.sbr \
	$(INTDIR)/buftvtots.sbr \
	$(INTDIR)/calyearstart.sbr \
	$(INTDIR)/utvtoa.sbr \
	$(INTDIR)/atolfp.sbr \
	$(INTDIR)/msyslog.sbr \
	$(INTDIR)/clocktypes.sbr \
	$(INTDIR)/caljulian.sbr \
	$(INTDIR)/authencrypt.sbr \
	$(INTDIR)/prettydate.sbr \
	$(INTDIR)/hextolfp.sbr \
	$(INTDIR)/fptoms.sbr \
	$(INTDIR)/numtohost.sbr \
	$(INTDIR)/authparity.sbr \
	$(INTDIR)/authreadkeys.sbr \
	$(INTDIR)/dofptoa.sbr \
	$(INTDIR)/mfptoms.sbr \
	$(INTDIR)/statestr.sbr \
	$(INTDIR)/humandate.sbr

$(OUTDIR)/libntp.bsc : $(OUTDIR)  $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LIB32=lib.exe
# ADD BASE LIB32 /NOLOGO
# ADD LIB32 /NOLOGO
LIB32_FLAGS=/NOLOGO /OUT:$(OUTDIR)\"libntp.lib" 
DEF_FLAGS=
DEF_FILE=
LIB32_OBJS= \
	$(INTDIR)/mfptoa.obj \
	$(INTDIR)/lib_strbuf.obj \
	$(INTDIR)/decodenetnum.obj \
	$(INTDIR)/gettstamp.obj \
	$(INTDIR)/clocktime.obj \
	$(INTDIR)/uinttoa.obj \
	$(INTDIR)/numtoa.obj \
	$(INTDIR)/octtoint.obj \
	$(INTDIR)/inttoa.obj \
	$(INTDIR)/mstolfp.obj \
	$(INTDIR)/authdecrypt.obj \
	$(INTDIR)/tstotv.obj \
	$(INTDIR)/a_md512crypt.obj \
	$(INTDIR)/modetoa.obj \
	$(INTDIR)/atouint.obj \
	$(INTDIR)/getopt.obj \
	$(INTDIR)/uglydate.obj \
	$(INTDIR)/machines.obj \
	$(INTDIR)/dolfptoa.obj \
	$(INTDIR)/a_md5decrypt.obj \
	$(INTDIR)/mexit.obj \
	$(INTDIR)/syssignal.obj \
	$(INTDIR)/msutotsf.obj \
	$(INTDIR)/atoint.obj \
	$(INTDIR)/auth12crypt.obj \
	$(INTDIR)/tsftomsu.obj \
	$(INTDIR)/systime.obj \
	$(INTDIR)/tvtots.obj \
	$(INTDIR)/findconfig.obj \
	$(INTDIR)/authkeys.obj \
	$(INTDIR)/a_md5encrypt.obj \
	$(INTDIR)/ranny.obj \
	$(INTDIR)/tvtoa.obj \
	$(INTDIR)/refnumtoa.obj \
	$(INTDIR)/netof.obj \
	$(INTDIR)/hextoint.obj \
	$(INTDIR)/caltontp.obj \
	$(INTDIR)/emalloc.obj \
	$(INTDIR)/calleapwhen.obj \
	$(INTDIR)/md5.obj \
	$(INTDIR)/fptoa.obj \
	$(INTDIR)/authdes.obj \
	$(INTDIR)/authusekey.obj \
	$(INTDIR)/buftvtots.obj \
	$(INTDIR)/calyearstart.obj \
	$(INTDIR)/utvtoa.obj \
	$(INTDIR)/atolfp.obj \
	$(INTDIR)/msyslog.obj \
	$(INTDIR)/clocktypes.obj \
	$(INTDIR)/caljulian.obj \
	$(INTDIR)/authencrypt.obj \
	$(INTDIR)/prettydate.obj \
	$(INTDIR)/hextolfp.obj \
	$(INTDIR)/fptoms.obj \
	$(INTDIR)/numtohost.obj \
	$(INTDIR)/authparity.obj \
	$(INTDIR)/authreadkeys.obj \
	$(INTDIR)/dofptoa.obj \
	$(INTDIR)/mfptoms.obj \
	$(INTDIR)/statestr.obj \
	$(INTDIR)/humandate.obj

$(OUTDIR)/libntp.lib : $(OUTDIR)  $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
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

SOURCE=.\mfptoa.c

$(INTDIR)/mfptoa.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\lib_strbuf.c

$(INTDIR)/lib_strbuf.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\decodenetnum.c

$(INTDIR)/decodenetnum.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\gettstamp.c

$(INTDIR)/gettstamp.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\clocktime.c

$(INTDIR)/clocktime.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\uinttoa.c

$(INTDIR)/uinttoa.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\numtoa.c

$(INTDIR)/numtoa.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\octtoint.c

$(INTDIR)/octtoint.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\inttoa.c

$(INTDIR)/inttoa.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\mstolfp.c

$(INTDIR)/mstolfp.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\authdecrypt.c

$(INTDIR)/authdecrypt.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\tstotv.c

$(INTDIR)/tstotv.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\a_md512crypt.c

$(INTDIR)/a_md512crypt.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\modetoa.c

$(INTDIR)/modetoa.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\atouint.c

$(INTDIR)/atouint.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\getopt.c

$(INTDIR)/getopt.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\uglydate.c

$(INTDIR)/uglydate.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\machines.c

$(INTDIR)/machines.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\dolfptoa.c

$(INTDIR)/dolfptoa.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\a_md5decrypt.c

$(INTDIR)/a_md5decrypt.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\mexit.c

$(INTDIR)/mexit.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\syssignal.c

$(INTDIR)/syssignal.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\msutotsf.c

$(INTDIR)/msutotsf.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\atoint.c

$(INTDIR)/atoint.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\auth12crypt.c

$(INTDIR)/auth12crypt.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\tsftomsu.c

$(INTDIR)/tsftomsu.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\systime.c

$(INTDIR)/systime.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\tvtots.c

$(INTDIR)/tvtots.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\findconfig.c

$(INTDIR)/findconfig.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\authkeys.c

$(INTDIR)/authkeys.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\a_md5encrypt.c

$(INTDIR)/a_md5encrypt.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\ranny.c

$(INTDIR)/ranny.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\tvtoa.c

$(INTDIR)/tvtoa.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\refnumtoa.c

$(INTDIR)/refnumtoa.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\netof.c

$(INTDIR)/netof.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\hextoint.c

$(INTDIR)/hextoint.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\caltontp.c

$(INTDIR)/caltontp.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\emalloc.c

$(INTDIR)/emalloc.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\calleapwhen.c

$(INTDIR)/calleapwhen.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\md5.c

$(INTDIR)/md5.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\fptoa.c

$(INTDIR)/fptoa.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\authdes.c

$(INTDIR)/authdes.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\authusekey.c

$(INTDIR)/authusekey.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\buftvtots.c

$(INTDIR)/buftvtots.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\calyearstart.c

$(INTDIR)/calyearstart.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\utvtoa.c

$(INTDIR)/utvtoa.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\atolfp.c

$(INTDIR)/atolfp.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\msyslog.c

$(INTDIR)/msyslog.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\clocktypes.c

$(INTDIR)/clocktypes.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\caljulian.c

$(INTDIR)/caljulian.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\authencrypt.c

$(INTDIR)/authencrypt.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\prettydate.c

$(INTDIR)/prettydate.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\hextolfp.c

$(INTDIR)/hextolfp.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\fptoms.c

$(INTDIR)/fptoms.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\numtohost.c

$(INTDIR)/numtohost.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\authparity.c

$(INTDIR)/authparity.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\authreadkeys.c

$(INTDIR)/authreadkeys.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\dofptoa.c

$(INTDIR)/dofptoa.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\mfptoms.c

$(INTDIR)/mfptoms.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\statestr.c

$(INTDIR)/statestr.obj :  $(SOURCE)  $(INTDIR)

# End Source File
################################################################################
# Begin Source File

SOURCE=.\humandate.c

$(INTDIR)/humandate.obj :  $(SOURCE)  $(INTDIR)

# End Source File
# End Group
# End Project
################################################################################
