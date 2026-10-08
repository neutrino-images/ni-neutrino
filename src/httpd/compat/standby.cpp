//=============================================================================
// See standby.h.
//=============================================================================

#include "httpd/compat/standby.h"

#include <coreapi/channels.h>
#include <coreapi/system.h>
#include <neutrinoMessages.h>

namespace httpd
{
namespace compat
{

namespace
{

// An unsettled box is not in standby, which is what the raw mode read said
// before it had a status of its own to say it with.
bool isInStandby()
{
	coreapi::Result<int> m = coreapi::channels::mode();
	return m.ok() && m.value() == NeutrinoModes::mode_standby;
}

} // namespace

void answerStandby(CyhookHandler &hh)
{
	if (hh.ParamList.empty())
	{
		if (isInStandby())
			hh.WriteLn("on");
		else
			hh.WriteLn("off");
		return;
	}

	const bool cec = hh.ParamList["cec"] != "off";

	// Answered once, after the command went out, so that one that did not is
	// reported here as it is everywhere else rather than with ok.
	bool sent = true;
	if (hh.ParamList["1"] == "on")
	{
		if (!isInStandby())
			sent = coreapi::system::standby(true, cec).ok();
	}
	else if (hh.ParamList["1"] == "off")
	{
		if (isInStandby())
			sent = coreapi::system::standby(false, cec).ok();
	}
	else
		sent = false;

	if (sent)
		hh.SendOk();
	else
		hh.SendError();
}

} // namespace compat
} // namespace httpd
