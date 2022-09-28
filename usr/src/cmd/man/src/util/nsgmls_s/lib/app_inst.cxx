#pragma ident	"@(#)instmac.m4	1.2	97/04/24 SMI"
#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif


#ifdef SP_NAMESPACE
}
#endif
#pragma ident	"@(#)app_inst.m4	1.2	97/04/24 SMI"
// Copyright (c) 1994 James Clark
// See the file COPYING for copying permission.

#include "splib.h"

#ifdef SP_MANUAL_INST

#define SP_DEFINE_TEMPLATES
#include "Vector.h"
#include "Owner.h"
#include "Options.h"
#undef SP_DEFINE_TEMPLATES

#include <stddef.h>
#include "CodingSystem.h"
#include "CmdLineApp.h"
#include "Event.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

#ifdef __DECCXX
#pragma define_template Vector<const CmdLineApp::AppChar *>
#else
#ifdef SP_ANSI_CLASS_INST
template class Vector<const CmdLineApp::AppChar *>;
#else
typedef Vector<const CmdLineApp::AppChar *> Dummy_0;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Owner<Encoder>
#else
#ifdef SP_ANSI_CLASS_INST
template class Owner<Encoder>;
#else
typedef Owner<Encoder> Dummy_1;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Options<CmdLineApp::AppChar>
#else
#ifdef SP_ANSI_CLASS_INST
template class Options<CmdLineApp::AppChar>;
#else
typedef Options<CmdLineApp::AppChar> Dummy_2;
#endif
#endif
#ifdef __DECCXX
#pragma define_template Owner<EventHandler>
#else
#ifdef SP_ANSI_CLASS_INST
template class Owner<EventHandler>;
#else
typedef Owner<EventHandler> Dummy_3;
#endif
#endif

#ifdef SP_NAMESPACE
}
#endif

#endif /* SP_MANUAL_INST */
