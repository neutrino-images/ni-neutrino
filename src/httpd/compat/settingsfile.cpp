//=============================================================================
// See settingsfile.h.
//=============================================================================

#include <config.h>

#include "httpd/compat/settingsfile.h"

#include "coreapi/settings/settings.h"
#include "coreapi/settings/settingstable.h"

#include <utility>
#include <vector>

namespace httpd
{
namespace compat
{

namespace
{

using coreapi::Descriptor;
using coreapi::ValueType;

bool endsWith(const std::string &s, const char *tail)
{
	const std::string t(tail);
	return s.size() >= t.size() && s.compare(s.size() - t.size(), t.size(), t) == 0;
}

// The file numbers the entries of a list or of records and stores their count beside them.
std::string withoutIndex(const std::string &key)
{
	if (endsWith(key, "_count"))
		return key.substr(0, key.size() - 6);
	const size_t last = key.find_last_not_of("0123456789");
	if (last == std::string::npos || last + 1 == key.size() || key[last] != '_')
		return std::string();
	return key.substr(0, last);
}

const Descriptor *declaredFor(const std::string &key)
{
	if (const Descriptor *d = coreapi::settings::findRow(key))
		return d;

	const std::string base = withoutIndex(key);
	if (base.empty())
		return NULL;
	const Descriptor *d = coreapi::settings::findRow(base);
	if (d != NULL && (d->type == ValueType::List || d->type == ValueType::Records))
		return d;

	// A record's members are filed under the row's stem, each with its own name.
	const Descriptor *t = coreapi::settingsTable();
	const size_t n = coreapi::settingsTableCount();
	for (size_t i = 0; i < n; ++i)
	{
		if (t[i].key == NULL || t[i].type != ValueType::Records)
			continue;
		const std::string row(t[i].key);
		const size_t cut = row.rfind('_');
		if (cut != std::string::npos && cut > 0 && key.compare(0, cut + 1, row, 0, cut + 1) == 0)
			return &t[i];
	}
	return NULL;
}

bool nameLooksSecret(const std::string &key)
{
	static const char *const kAnywhere[] = { "password", "passwd", "pincode", "secret", "token", "_pass_" };
	for (size_t i = 0; i < sizeof(kAnywhere) / sizeof(kAnywhere[0]); ++i)
		if (key.find(kAnywhere[i]) != std::string::npos)
			return true;

	// Only at the end, so keyboard_layout, zappingmode and the passthrough switches stay.
	const std::string base = withoutIndex(key);
	const std::string &name = base.empty() ? key : base;
	static const char *const kAtEnd[] = { "_key", "_dev_id", "_pin", "apikey" };
	for (size_t i = 0; i < sizeof(kAtEnd) / sizeof(kAtEnd[0]); ++i)
		if (endsWith(name, kAtEnd[i]))
			return true;
	return false;
}

/* The file keeps a record member by member, and a secret record is secret only
   in the members its declaration marks; the address, name and user of a remote
   box stay readable beside its password. */
bool recordMemberWithheld(const Descriptor &row, const std::string &key)
{
	const std::string stem(row.key);
	const size_t cut = stem.rfind('_');
	const std::string base = withoutIndex(key);
	const coreapi::FieldExtra *x = row.field.extra;
	if (x != NULL && cut != std::string::npos && base.size() > cut + 1 && base.compare(0, cut + 1, stem, 0, cut + 1) == 0)
	{
		const std::string member = base.substr(cut + 1);
		for (size_t m = 0; m < x->record_field_count; ++m)
			if (member == x->record_fields[m].name)
				return x->record_fields[m].secret;
	}
	return nameLooksSecret(key);
}

// A line to each entry, without the control bytes a pasted list brings and without empty lines.
std::string webListText(const CStringList &params, const char *name)
{
	CStringList::const_iterator p = params.find(name);
	if (p == params.end())
		return std::string();
	std::vector<std::string> entries;
	std::string line;
	const std::string &text = p->second;
	for (size_t i = 0; i <= text.size(); ++i)
	{
		if (i == text.size() || text[i] == '\n')
		{
			if (!line.empty())
				entries.push_back(line);
			line.clear();
		}
		else if (text[i] != '\r' && text[i] != '\t')
			line += text[i];
	}
	return coreapi::settings::listText(entries);
}

} // namespace

bool legacyConfigWithheld(const std::string &config, const std::string &key)
{
	if (config == "neutrino")
	{
		const Descriptor *d = declaredFor(key);
		if (d != NULL && d->type == ValueType::Records && d->secret)
			return recordMemberWithheld(*d, key);
		if (d != NULL)
			return d->secret;
	}
	return nameLooksSecret(key);
}

CStringList legacyConfigReadable(const std::string &config, const CStringList &all)
{
	CStringList out;
	for (CStringList::const_iterator it = all.begin(); it != all.end(); ++it)
		if (!legacyConfigWithheld(config, it->first))
			out.insert(*it);
	return out;
}

void legacyWriteWebLists(const CStringList &params, coreapi::settings::Refusals &failed)
{
	static const char *const kLists[][2] =
	{
		{ "webtv", "webtv_xml" }, { "webradio", "webradio_xml" }, { "xmltv", "xmltv_xml" }
	};
	std::vector<std::pair<std::string, std::string> > members;
	for (size_t i = 0; i < sizeof(kLists) / sizeof(kLists[0]); ++i)
		members.push_back(std::make_pair(std::string(kLists[i][1]), webListText(params, kLists[i][0])));
	coreapi::settings::writeBatch(members, failed);
}

} // namespace compat
} // namespace httpd
