//=============================================================================
// The shell controlapi.cpp is built against. See hook.h.
//=============================================================================

#include "httpd/compat/hook.h"
#include "httpd/compat/helper.h"

namespace httpd
{
namespace compat
{

//-----------------------------------------------------------------------------
CyhookHandler::CyhookHandler() :
	status(HANDLED_NONE),
	httpStatus(HTTP_NIL),
	ContentLength(0),
	// -1, not 0: hook.h documents this field itself as "-1 dynamic content",
	// the same sentinel yhttpd_core/yhook.cpp's session_init() resets it to
	// on every request. 0 reads as a real modification time to whatever
	// builds the Last-Modified header from it, and the fixed point that
	// value formats to is 00:00:00 GMT, 1 Jan 1970 rather than the moment
	// this handler actually answered.
	LastModified((time_t) -1),
	keep_alive(false),
	cached(false),
	Method(M_UNKNOWN)
{
}

//-----------------------------------------------------------------------------
CyhookHandler::~CyhookHandler()
{
}

//-----------------------------------------------------------------------------
// The old server walks positions "1".."size()" rather than the map's own
// keys, which is the same counting parseParams uses to write them. A keyword
// that never arrived as a positional parameter answers false, not an error.
bool CyhookHandler::ParamList_exist(std::string keyword)
{
	bool exist = false;
	unsigned int s = (unsigned int)ParamList.size();
	for (unsigned int i = 1; i <= s; i++)
	{
		exist = (ParamList[itoa(i)] == keyword);
		if (exist)
			break;
	}
	return exist;
}

//-----------------------------------------------------------------------------
void CyhookHandler::setOutHeader(int _httpStatus, const std::string& _ResponseMimeType)
{
	SetHeader(static_cast<HttpResponseType>(_httpStatus), _ResponseMimeType);
}

} // namespace compat
} // namespace httpd
