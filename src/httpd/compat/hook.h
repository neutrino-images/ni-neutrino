//=============================================================================
// The shell controlapi.cpp is built against, so that file can move into this
// tree unchanged. Everything the Serializer base class already owns is not
// declared again here: ParamList, yresult, outType, nonPair, the out* family,
// Write, WriteLn, printf, addResult, SendHTMLHeader, SendHTMLFooter, SendOk,
// SendError and SendResult all live on that class. A second declaration of any
// of them in this one would hide the base member, and nothing in the build
// says so.
//=============================================================================

#ifndef __httpd_compat_hook_h__
#define __httpd_compat_hook_h__

#include "httpd/compat/hookout.h"

#include <ctime>
#include <string>
#include <sys/types.h>

namespace httpd
{
namespace compat
{

//-----------------------------------------------------------------------------
// Hook handling result. The old server calls this THandleStatus
// (yhttpd_core/yhook.h); controlapi.cpp compares hh->status against these
// names, so the numbers below are not free to move even though nothing here
// hands them to a wire.
//-----------------------------------------------------------------------------
typedef enum
{
	HANDLED_NONE			= 0,	// Init
	HANDLED_READY,				// Handled
	HANDLED_ABORT,				// Abort Connection Fatal
	HANDLED_ERROR,				// Have Error like missing Parameter
	HANDLED_NOT_IMPLEMENTED,	// URL should be handled but not implemented
	HANDLED_REDIRECTION,		// Set new URL and send HTTPD Object Moved
	HANDLED_SENDFILE,			// Set new URL and Send File
	HANDLED_REWRITE,			// Set new URL and call Hooks again
	HANDLED_CONTINUE,			// handled but go on
} THandleStatus;

//-----------------------------------------------------------------------------
// HTTP status codes. Equivalent to yhttpd_core/ytypes_globals.h's
// HttpResponseType, not a redefinition of it: controlapi.cpp compares
// hh->httpStatus against HTTP_OK and HTTP_NOT_FOUND by value, so these are the
// wire's numbers and stay pinned to them.
//-----------------------------------------------------------------------------
typedef enum
{
	HTTP_NIL				= -1,
	HTTP_CONTINUE				= 100,
	HTTP_SWITCHING_PROTOCOLS		= 101,
	HTTP_OK					= 200,
	HTTP_CREATED				= 201,
	HTTP_ACCEPTED				= 202,
	HTTP_NON_AUTHORITATIVE_INFO		= 203,
	HTTP_NO_CONTENT				= 204,
	HTTP_RESET_CONTENT			= 205,
	HTTP_PARTIAL_CONTENT			= 206,
	HTTP_MULTIBLE_CHOICES			= 300,
	HTTP_MOVED_PERMANENTLY			= 301,
	HTTP_MOVED_TEMPORARILY			= 302,
	HTTP_SEE_OTHER				= 303,
	HTTP_NOT_MODIFIED			= 304,
	HTTP_USE_PROXY				= 305,
	HTTP_TEMPORARY_REDIRECT			= 307,
	HTTP_BAD_REQUEST			= 400,
	HTTP_UNAUTHORIZED			= 401,
	HTTP_PAYMENT_REQUIRED			= 402,
	HTTP_FORBIDDEN				= 403,
	HTTP_NOT_FOUND				= 404,
	HTTP_METHOD_NOT_ALLOWED			= 405,
	HTTP_NOT_ACCEPTABLE			= 406,
	HTTP_PROXY_AUTHENTICATION_REQUIRED	= 407,
	HTTP_REQUEST_TIMEOUT			= 408,
	HTTP_CONFLICT				= 409,
	HTTP_GONE				= 410,
	HTTP_LENGTH_REQUIRED			= 411,
	HTTP_PRECONDITION_FAILED		= 412,
	HTTP_REQUEST_ENTITY_TOO_LARGE		= 413,
	HTTP_REQUEST_URI_TOO_LARGE		= 414,
	HTTP_UNSUPPORTED_MEDIA_TYPE		= 415,
	HTTP_REQUEST_RANGE_NOT_SATISFIABLE	= 416,
	HTTP_EXPECTAION_FAILED			= 417,
	HTTP_INTERNAL_SERVER_ERROR		= 500,
	HTTP_NOT_IMPLEMENTED			= 501,
	HTTP_BAD_GATEWAY			= 502,
	HTTP_SERVICE_UNAVAILABLE		= 503,
	HTTP_GATEWAY_TIMEOUT			= 504,
	HTTP_HTTP_VERSION_NOT_SUPPORTED		= 505,
} HttpResponseType;

//-----------------------------------------------------------------------------
// HTTP request methods. Equivalent to ytypes_globals.h's THttp_Method:
// controlapi.cpp compares hh->Method == M_HEAD by value.
//-----------------------------------------------------------------------------
typedef enum
{
	M_UNKNOWN	= 0,
	M_POST		= 1,
	M_GET,
	M_HEAD,
	M_PUT,
	M_DELETE,
	M_TRACE
} THttp_Method;

//-----------------------------------------------------------------------------
// The input and output a handler in controlapi.cpp sees, on top of what
// Serializer already carries.
//-----------------------------------------------------------------------------
class CyhookHandler : public Serializer
{
public:
	CyhookHandler();
	~CyhookHandler();

	// Output
	THandleStatus		status;			// status of Hook handling
	HttpResponseType	httpStatus;		// http-status code for response
	std::string		ResponseMimeType;	// mime-type for response
	std::string		NewURL;			// new URL for Redirection
	std::string		Sendfile;		// Path & Name (local os style) of file to send
	off_t			ContentLength;		// Length of Response Body
	time_t			LastModified;		// Last Modified Time of Item to send / -1 dynamic content
	bool			keep_alive;
	bool			cached;			// cached by mod_cache

	// Input
	CStringList		UrlData;		// local copy of UrlData (Request)
	CStringList		HeaderList;		// local copy of HeaderList (Request)
	CStringList		WebserverConfigList;	// Reference (writable) to ConfigList
	THttp_Method		Method;			// HTTP Method (requested)

	// The one-argument form is Serializer's; without this the two-argument
	// override below hides it, and hh->addResult(s) from anywhere else in the
	// tree stops compiling instead of warning.
	using Serializer::addResult;
	void addResult(const std::string& result, THandleStatus _status)
		{yresult += result; status = _status;}

	void SendFile(const std::string& url)		{NewURL = url; status = HANDLED_SENDFILE;}
	void SendRedirect(const std::string& url)	{httpStatus=HTTP_MOVED_TEMPORARILY; NewURL = url; status = HANDLED_REDIRECTION;}
	void SendRewrite(const std::string& url)	{NewURL = url; status = HANDLED_REWRITE;}

	void SetHeader(HttpResponseType _httpStatus, std::string _ResponseMimeType)
		{httpStatus = _httpStatus; ResponseMimeType = _ResponseMimeType;}
	void SetHeader(HttpResponseType _httpStatus, std::string _ResponseMimeType, THandleStatus _status)
		{httpStatus = _httpStatus; ResponseMimeType = _ResponseMimeType; status = _status;}
	void SetError(HttpResponseType responseType)
		{SetHeader(responseType, "text/html");}
	void SetError(HttpResponseType responseType, THandleStatus _status)
		{SetError(responseType); status = _status;}

	off_t GetContentLength()
		{return (status==HANDLED_SENDFILE)?ContentLength : (off_t)yresult.length();}

	bool ParamList_exist(std::string keyword);

protected:
	// outStart (hookout.cpp) states the response head through this hook
	// rather than through the outHttpStatus/outMimeType fields Serializer
	// falls back to on its own. Forgetting this override compiles clean and
	// runs clean: outStart still runs, it just states the head nowhere, and
	// every answer goes out without one. See the case that holds it.
	virtual void setOutHeader(int _httpStatus, const std::string& _ResponseMimeType);
};

} // namespace compat
} // namespace httpd

#endif /* __httpd_compat_hook_h__ */
