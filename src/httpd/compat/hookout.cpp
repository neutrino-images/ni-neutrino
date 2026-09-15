//=============================================================================
// The old server's answer writer. See hookout.h.
//=============================================================================
// C
#include <cstdarg>
#include <cstdio>
#include <cstring>

// UTF8 convert
#include <system/helpers.h>
#include <xmltree/xmlinterface.h>

#include "httpd/compat/hookout.h"
#include "httpd/compat/helper.h"

namespace httpd
{
namespace compat
{

//=============================================================================
// Constructor & Ceconstructor
//=============================================================================
Serializer::Serializer()
{
	_outIndent = 0;
	outType = plain;
	nonPair = false;
	// the hook handler starts its status at HTTP_NIL, which is this
	outHttpStatus = -1;
}

Serializer::~Serializer()
{
}

//-----------------------------------------------------------------------------
void Serializer::reset()
{
	yresult.clear();
	_outIndent = 0;
	outType = plain;
	nonPair = false;
	outHttpStatus = -1;
	outMimeType.clear();
	// ParamList is the request and not the answer, so a reset leaves it alone
}

//-----------------------------------------------------------------------------
void Serializer::setOutHeader(int _httpStatus, const std::string& _ResponseMimeType)
{
	outHttpStatus = _httpStatus;
	outMimeType = _ResponseMimeType;
}

//=============================================================================
// Output helpers
//=============================================================================
//-----------------------------------------------------------------------------
void Serializer::SendHTMLHeader(const std::string& Titel) {
	WriteLn("<html>\n<head><title>" + Titel + "</title>\n");
	WriteLn("<meta http-equiv=\"cache-control\" content=\"no-cache\" />");
	WriteLn("<meta http-equiv=\"expires\" content=\"0\" />\n</head>\n<body>\n");
}
//-----------------------------------------------------------------------------
void Serializer::SendHTMLFooter(void) {
	WriteLn("</body>\n</html>\n\n");
}

//-----------------------------------------------------------------------------
#define OUTBUFSIZE 4096
void Serializer::printf(const char *fmt, ...) {
	char outbuf[OUTBUFSIZE];
	memset(outbuf,0, OUTBUFSIZE);
	va_list arglist;
	va_start(arglist, fmt);
	vsnprintf(outbuf, OUTBUFSIZE, fmt, arglist);
	va_end(arglist);
	Write(outbuf);
}
//-----------------------------------------------------------------------------
TOutType Serializer::getOutType() {
	TOutType _outType = plain;
	if(!(ParamList.empty())) {
		if ((ParamList.find("format") != ParamList.end() && ParamList["format"] == "json")
				|| (ParamList.find("json") != ParamList.end() && !(ParamList["json"].empty())) )
			_outType = json;
		else if ((ParamList.find("format") != ParamList.end() && ParamList["format"] == "xml")
				|| (ParamList.find("xml") != ParamList.end() && !(ParamList["xml"].empty())) )
			_outType = xml;
	}
	return _outType;
}
//-----------------------------------------------------------------------------
TOutType Serializer::outStart(bool single) {
	// for compatibility
	nonPair = single;
	// get outType
	outType = getOutType();
	// set response header
	if (outType == xml)
		setOutHeader(httpOk, "text/xml; charset=UTF-8");
	else
		setOutHeader(httpOk, "text/plain; charset=UTF-8");

	return outType;
}

//-----------------------------------------------------------------------------
std::string Serializer::outIndent() {
	return "";
}

//-----------------------------------------------------------------------------
std::string Serializer::outSingle(std::string _content) {
	return _content + "\n";
}

//-----------------------------------------------------------------------------
std::string Serializer::outPair(std::string _key, std::string _content, bool _next) {
	std::string result = "", _key_close = "", tmp;
	ySplitString(_key, " ", _key_close, tmp);
	switch (outType) {
	case xml:
		result = outIndent() + "<" + _key + ">" + _content + "</" + _key_close + ">";
		break;
	case json:
		replace(_content, "\"", "\'");
		result = outIndent() + "\"" + _key + "\": \"" + _content + "\"";
		if(_next)
			result += ",";
		break;
	default:
		if (nonPair)
			result = _content;
		else
			result = _key + "=" + _content;
		break;
	}
	return result + "\n";
}

//-----------------------------------------------------------------------------
std::string Serializer::outArray(std::string _key, std::string _content, bool _next) {
	std::string result = "", _key_close = "", tmp;
	ySplitString(_key, " ", _key_close, tmp);
	switch (outType) {
	case xml:
		//TODO: xml check and DESC check
		result = outIndent() + "<" + _key + ">\n" + _content + "</" + _key_close + ">";
		result += "\n";
		break;
	case json:
		//TODO: json check
		result = outIndent() + "\"" + _key + "\": [" + _content + "]";
		if(_next)
			result += ",";
		result += "\n";
		break;
	default:
		result = _content;
		break;
	}
	return result;
}

//-----------------------------------------------------------------------------
std::string Serializer::outArrayItem(std::string _key, std::string _content, bool _next) {
	std::string result = "", _key_close = "", tmp;
	ySplitString(_key, " ", _key_close, tmp);
	switch (outType) {
	case xml:
		//TODO: xml check and DESC check
		result = outIndent() + "<" + _key + ">\n" + _content + "</" + _key_close + ">";
		result += "\n";
		break;
	case json:
		//TODO: json check
		result = outIndent() + "{" + _content + "}";
		if(_next)
			result += ",";
		result += "\n";
		break;
	default:
		result = _content;
		break;
	}
	return result;
}
//-----------------------------------------------------------------------------
std::string Serializer::outObject(std::string _key, std::string _content, bool _next) {
	std::string result = "", _key_close = "", tmp;
	ySplitString(_key, " ", _key_close, tmp);
	switch (outType) {
	case xml:
		//TODO: xml check and DESC check
		result = outIndent() + "<" + _key + ">\n" + _content + "</" + _key_close + ">";
		result += "\n";
		break;
	case json:
		//TODO: json check
		result = outIndent() + "\"" + _key + "\": {" + _content + "}";
		if(_next)
			result += ",";
		result += "\n";
		break;
	default:
		result = _content;
		break;
	}
	return result;
}

//-----------------------------------------------------------------------------
std::string Serializer::outValue(std::string _content, bool _xml_cdata) {
	std::string result = "";
	switch (outType) {
	case xml:
		if (_xml_cdata)
		{
			result = "<![CDATA[" + _content + "]]>";;
		}
		else
		{
			result = convert_UTF8_To_UTF8_XML(utf8_check_is_valid(_content) ? _content.c_str() : iso_8859_1_to_utf8(_content).c_str());
		}
		break;
	case json:
		result = json_convert_string(_content);
		break;

	default:
		result = _content;
		break;
	}
	return result;
}

std::string Serializer::outNext() {
	if(outType == json)
		return ",";
	else
		return "";
}

//-----------------------------------------------------------------------------
void Serializer::SendOk() {
	std::string result = "";
	switch (outType) {
	case xml:
		result = "<success>true</success>";
		break;
	case json:
		result = "{\"success\": \"true\"}";
		break;
	default:
		result = "ok";
		break;
	}
	Write(result);
}
//-----------------------------------------------------------------------------
void Serializer::SendError(std::string error) {
	std::string result = "";
	switch (outType) {
	case xml:
		if (error.empty())
			result = "<success>false</success>";
		else
			result = "<success>false<error>" + error + "</error></success>";
		break;
	case json:
		if (error.empty())
			result = "{\"success\": \"false\"}";
		else
			result = "{\"success\": \"false\", \"error\":{\"msg\": \"" + error + "\"}}";
		break;
	default:
		if (error.empty())
			result = "error";
		else
			result = "error=" + error;
		break;
	}
	Write(result);
}
//-----------------------------------------------------------------------------
void Serializer::SendResult(std::string _content) {
	std::string result = "";
	switch (outType) {
	case xml:
		result = _content;
		break;
	case json:
		result = json_out_success(_content);
		break;
	default:
		result = _content;
		break;
	}
	WriteLn(result);
}

} // namespace compat
} // namespace httpd
