//=============================================================================
// The old server's answer writer, taken out of the hook handler it used to sit
// in so that the bytes it produces can be held to, and so that the transport
// under it can be replaced without any of them moving.
//
// The fields the output is written to live here and not in whatever derives
// from this: a second declaration of yresult or outType in a derived class
// hides this one, and nothing in the build says so.
//=============================================================================

#ifndef __httpd_compat_hookout_h__
#define __httpd_compat_hookout_h__

#include "httpd/compat/helper.h"

#include <string>

namespace httpd
{
namespace compat
{

//-----------------------------------------------------------------------------
// Output Tyoe.
//-----------------------------------------------------------------------------
typedef enum
{
	plain	= 0,
	html,
	xml,
	json
} TOutType;

class Serializer
{
public:
	Serializer();
	virtual ~Serializer();

	// Output
	std::string	yresult;		// content for response output
	bool		nonPair;

	// Input
	CStringList	ParamList;		// local copy of ParamList (Request)

	// output methods
	void addResult(const std::string& result) 	{yresult += result;}
	void printf(const char *fmt, ...);
	void Write(const std::string& text)			{addResult(text);}
	void WriteLn(const std::string& text)		{addResult(text+"\r\n");}
	void Write(char const *text)				{Write(std::string(text));}
	void WriteLn(char const *text)				{WriteLn(std::string(text));}
	void SendHTMLHeader(const std::string& Titel);
	void SendHTMLFooter(void);
	void SendOk(void);
	void SendError(std::string error = "");
	void SendResult(std::string _content);

	int _outIndent;
	TOutType outType;			// Outputtpe = plain (default)|xml|json
	TOutType outStart(bool single = false);
	TOutType getOutType();
	std::string outIndent();
	std::string outSingle(std::string _content);
	std::string outPair(std::string _key, std::string _content, bool _next);
	std::string outArray(std::string _key, std::string _content, bool _next = false);
	std::string outArrayItem(std::string _key, std::string _content, bool _next);
	std::string outObject(std::string _key,std::string  _content, bool _next = false);
	std::string outValue(std::string _content, bool _xml_cdata = true);
	std::string outNext();

	// Where outStart leaves the response head when nothing is carrying one.
	// A transport overrides setOutHeader and takes the head instead.
	int outHttpStatus;
	std::string outMimeType;

	// For driving this on its own. Nothing in an answer calls these.
	void setOutType(TOutType _outType)	{outType = _outType;}
	const std::string& result() const	{return yresult;}
	void reset();

protected:
	// The status outStart states is always this one; only the content type
	// follows the output type.
	enum { httpOk = 200 };

	// outStart states the response head, which belongs to whatever carries the
	// answer. This one has nothing to carry it, so it writes the head down.
	virtual void setOutHeader(int _httpStatus, const std::string& _ResponseMimeType);
};

} // namespace compat
} // namespace httpd

#endif /* __httpd_compat_hookout_h__ */
