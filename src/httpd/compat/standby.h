//=============================================================================
// /control/standby, all but the wake of the channel daemon, which needs the
// running program and stays in CControlAPI::StandbyCGI. Apart so that a host
// test can drive what the legacy surface answers and sends.
//=============================================================================

#ifndef __httpd_compat_standby_h__
#define __httpd_compat_standby_h__

#include "httpd/compat/hook.h"

namespace httpd
{
namespace compat
{

/* No parameter: "on" or "off" for the standby state. "on" or "off" in "1": the
   change, sent only where the box is not already in that state, and with cec=off
   one that leaves the television alone. ok once sent, error where it was not. */
void answerStandby(CyhookHandler &hh);

} // namespace compat
} // namespace httpd

#endif /* __httpd_compat_standby_h__ */
