#pragma ident	"@(#)CmdLineAppMessages.h	1.2	97/04/24 SMI"
// This file was automatically generated from CmdLineAppMessages.msg by msggen.pl.
#include "Message.h"

#ifdef SP_NAMESPACE
namespace SP_NAMESPACE {
#endif

struct CmdLineAppMessages {
  // 4000
  static const MessageType1 invalidOptionError;
  // 4001
  static const MessageType1 missingOptionArgError;
  // 4002
  static const MessageType1 usage;
  // 4003
  static const MessageType1 versionInfo;
  // 4004
  static const MessageType1 unknownBctf;
  // 4005
  static const MessageType2 cannotOpenOutputError;
};
const MessageType1 CmdLineAppMessages::invalidOptionError(
MessageType::error,
#ifdef BUILD_LIBSP
MessageFragment::libModule,
#else
MessageFragment::appModule,
#endif
4000
#ifndef SP_NO_MESSAGE_TEXT
,"invalid option %1"
#endif
);
const MessageType1 CmdLineAppMessages::missingOptionArgError(
MessageType::error,
#ifdef BUILD_LIBSP
MessageFragment::libModule,
#else
MessageFragment::appModule,
#endif
4001
#ifndef SP_NO_MESSAGE_TEXT
,"missing argument for option %1"
#endif
);
const MessageType1 CmdLineAppMessages::usage(
MessageType::info,
#ifdef BUILD_LIBSP
MessageFragment::libModule,
#else
MessageFragment::appModule,
#endif
4002
#ifndef SP_NO_MESSAGE_TEXT
,"usage is %1"
#endif
);
const MessageType1 CmdLineAppMessages::versionInfo(
MessageType::info,
#ifdef BUILD_LIBSP
MessageFragment::libModule,
#else
MessageFragment::appModule,
#endif
4003
#ifndef SP_NO_MESSAGE_TEXT
,"SP version %1"
#endif
);
const MessageType1 CmdLineAppMessages::unknownBctf(
MessageType::error,
#ifdef BUILD_LIBSP
MessageFragment::libModule,
#else
MessageFragment::appModule,
#endif
4004
#ifndef SP_NO_MESSAGE_TEXT
,"unknown BCTF %1"
#endif
);
const MessageType2 CmdLineAppMessages::cannotOpenOutputError(
MessageType::error,
#ifdef BUILD_LIBSP
MessageFragment::libModule,
#else
MessageFragment::appModule,
#endif
4005
#ifndef SP_NO_MESSAGE_TEXT
,"cannot open output file %1 (%2)"
#endif
);
#ifdef SP_NAMESPACE
}
#endif
