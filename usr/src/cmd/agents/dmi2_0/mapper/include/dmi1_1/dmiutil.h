/* Copyright 08/05/96 Sun Microsystems, Inc. All Rights Reserved.
*/
#pragma ident  "@(#)dmiutil.h	1.1 96/08/05 Sun Microsystems"

DmiLibInstallData_t *mifInstall(char         *filename,
                                char         *mifDir, 
                                DMI_FUNC_OUT localEventHandler);
void IssueCiReg(unsigned long Handle, unsigned long Component,
                DMI_FUNC_OUT accessFunc, DMI_FUNC0_OUT cancelFunc);
