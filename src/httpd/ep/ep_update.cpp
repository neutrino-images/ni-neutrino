/*
 * ep_update.cpp - routes for installing an image update lying on the box
 *
 * Copyright (C) 2026 NI-Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "httpd/endpoints.h"

#include "httpd/endpoint.h"
#include "httpd/http.h"
#include "httpd/json.h"
#include "httpd/schema.h"
#include "httpd/status.h"

#include "coreapi/update.h"
#include "coreapi/base/errors.h"
#include "coreapi/base/result.h"

#include <cstddef>
#include <string>
#include <utility>

namespace httpd
{

namespace
{

/* Two addresses. One, read and written, to install an image update (.bin)
   that already lies on the box; one, read only, to ask whether the update server
   has a newer image.

   LOCAL AND ONLINE IN THE PATH, because the menu this stands in for has two
   updates, one out of a file on the box and one from a server. Of the second
   only the question is here, whether there is something newer: fetching and
   writing an image is the menu's.

   NO WHOLE IMAGE. Writing a complete system is left to the menu, in front of
   the television.

   NO UPLOAD HERE. How a file reaches the box is the files routes' business;
   this reads one directory and takes a bare name out of it, so there is no
   path a caller writes and nothing here that could be pointed at another
   file. */

const FieldDesc kUpdateFileFields[] = {
	HTTPD_MEMBER("name", FieldType::String,
		"the file's name inside the update directory, which is what POST /api/v1/update/local takes"),
	HTTPD_MEMBER("size", FieldType::UInt, "how large the file is, in bytes"),
	HTTPD_MEMBER("modified", FieldType::Time,
		"when the file was last written, which for a file still being copied to the box is a moment ago"),
};

const Schema kUpdateFileSchema = { "update-file", HTTPD_FIELDS(kUpdateFileFields) };

const FieldDesc kLocalUpdateFields[] = {
	HTTPD_MEMBER("dir", FieldType::String, "the directory the files were looked for in"),
	HTTPD_LIST_OF("items", &kUpdateFileSchema,
		"every image update in the directory, by name, and empty where there is none"),
};

const Schema kLocalUpdateSchema = { "local-update", HTTPD_FIELDS(kLocalUpdateFields) };

Response localUpdate(const Request &)
{
	coreapi::Result<coreapi::update::Local> got = coreapi::update::local();
	if (!got.ok())
		return problemFor(got.error());

	const coreapi::update::Local all = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 64 + 112 * all.files.size());
	j.beginObject();
	j.key("dir");
	j.value(all.dir);
	j.key("items");
	j.beginArray();
	for (size_t i = 0; i < all.files.size(); ++i)
	{
		j.beginObject();
		j.key("name");
		j.value(all.files[i].name);
		j.key("size");
		j.value((unsigned long long) all.files[i].size);
		j.key("modified");
		j.value((long long) all.files[i].modified);
		j.endObject();
	}
	j.endArray();
	j.endObject();
	return out;
}

Response startLocalUpdate(const Request &r)
{
	coreapi::update::Request asked;
	asked.name = r.asString("name");

	coreapi::Result<void> done = coreapi::update::start(asked);
	if (!done.ok())
		return problemFor(done.error());
	// Accepted and not done: what went out is a message to the box's loop.
	return accepted();
}

const FieldDesc kUpdateImageFields[] = {
	HTTPD_MEMBER("release", FieldType::String,
		"the version as the box shows it, e.g. `2026.10`; empty where it could not be read"),
	HTTPD_MEMBER("built", FieldType::Time, "when the image was built; 0 where that could not be read"),
};

const Schema kUpdateImageSchema = { "update-image", HTTPD_FIELDS(kUpdateImageFields) };

const FieldDesc kUpdateFoundFields[] = {
	HTTPD_MEMBER("release", FieldType::String,
		"the version as the box shows it, e.g. `2026.11`; empty where it could not be read"),
	HTTPD_MEMBER("built", FieldType::Time, "when the image was built; 0 where that could not be read"),
	HTTPD_MEMBER("name", FieldType::String, "the name the server's list gives the image"),
};

const Schema kUpdateFoundSchema = { "update-found", HTTPD_FIELDS(kUpdateFoundFields) };

const FieldDesc kOnlineUpdateFields[] = {
	HTTPD_MEMBER("reached", FieldType::Bool,
		"whether the server's list of images was read; false: the server was not reachable, and `newer` says nothing"),
	HTTPD_MEMBER("newer", FieldType::Bool,
		"whether the list holds an image newer than the running one"),
	HTTPD_MEMBER("checked", FieldType::Time, "when the server was asked"),
	HTTPD_OBJECT("running", &kUpdateImageSchema, "the image the box runs"),
	HTTPD_OBJECT_OPTIONAL("found", &kUpdateFoundSchema,
		"the first newer image in the list; only there where `newer` is true"),
};

const Schema kOnlineUpdateSchema = { "online-update", HTTPD_FIELDS(kOnlineUpdateFields) };

void writeImage(Json &j, const coreapi::update::Image &image)
{
	j.key("release");
	j.value(image.release);
	j.key("built");
	j.value((long long) image.built);
}

Response onlineUpdate(const Request &r)
{
	const bool fresh = r.has("fresh") && r.asBool("fresh");
	coreapi::Result<coreapi::update::Online> got = coreapi::update::online(fresh);
	if (!got.ok())
		return problemFor(got.error());

	const coreapi::update::Online on = std::move(got).value();

	Response out = okJson();
	Json j(out.body, 256 + on.name.size());
	j.beginObject();
	j.key("reached");
	j.value(on.reached);
	j.key("newer");
	j.value(on.newer);
	j.key("checked");
	j.value((long long) on.checked);
	j.key("running");
	j.beginObject();
	writeImage(j, on.running);
	j.endObject();
	if (on.newer)
	{
		j.key("found");
		j.beginObject();
		writeImage(j, on.found);
		j.key("name");
		j.value(on.name);
		j.endObject();
	}
	j.endObject();
	return out;
}

const Param kOnlineParams[] = {
	HTTPD_QUERY("fresh", ParamType::Bool,
		"ask the server now even where an answer of the last quarter of an hour is kept, false by default"),
};

const RouteRefusal kOnlineRefusals[] = {
	HTTPD_REFUSES(NotSupported, NoUpdateCheck,
		"nothing on this box can ask the update server"),
};

const Param kStartParams[] = {
	HTTPD_BODY_REQUIRED_TEXT("name",
		"the name field of one file GET /api/v1/update/local lists: a bare file name ending in `.bin`, never a path", 255),
};

const RouteRefusal kStartRefusals[] = {
	HTTPD_REFUSES(InvalidArgument, BadName,
		"that is not the name of an image update"),
	HTTPD_REFUSES(NotFound, NoSuchUpdateFile,
		"the update directory holds no file of that name"),
	HTTPD_REFUSES(Conflict, RecordingRunning,
		"the box is recording, and an update would end that"),
};

const Endpoint kUpdateEndpoints[] = {
	{ Method::Get, "/api/v1/update/local", AuthLevel::Read,
	  "the image updates lying on the box",
	  "Lists every image update in the update directory, `/tmp`: every plain file whose "
	  "name ends in `.bin`, with its size and time of last change. Complete images are "
	  "not listed; they are written from the box's own menu. Read afresh for each "
	  "request; a missing or unreadable directory answers an empty list.\n\n"
	  "**Related:** `POST /api/v1/update/local`.",
	  NULL, 0, &kLocalUpdateSchema, &localUpdate, false,
	  Answers200, HTTPD_NO_REFUSALS },
	/* System, as everything that runs a program as this server's account is,
	   and this one replaces parts of the system the account lives in. */
	{ Method::Post, "/api/v1/update/local", AuthLevel::System,
	  "asks the box to install one of the image updates lying on it",
	  "Hands one file out of `GET /api/v1/update/local` to the box's install script, as "
	  "the local update of the box's menu does. `202` means the request has reached the "
	  "box's event loop, not that anything has been installed: nothing on this interface "
	  "reports how it went, and the television shows nothing either. A box in standby "
	  "stays in standby.\n\n"
	  "**Side effects:** the script unpacks the file in the update directory and puts "
	  "what it carries in place, as the account this server runs as. Whether the box "
	  "restarts afterwards is the update's own decision; an update of this program "
	  "ends it, and this server with it, until the box is up again.\n\n"
	  "**Preconditions:** the box is not recording.\n\n"
	  "**Refusals:**\n"
	  "- `400 bad-name`: `name` is not a bare file name ending in `.bin`.\n"
	  "- `404 no-such-update-file`: the update directory holds no plain file of that "
	  "name. Take names from `GET /api/v1/update/local`.\n"
	  "- `409 recording-running`: the box is recording. End the recording first, see "
	  "`GET /api/v1/recordings`.\n\n"
	  "**Related:** `GET /api/v1/update/local`, `POST /api/v1/system/reboot`.",
	  HTTPD_PARAMS(kStartParams), NULL, &startLocalUpdate, false,
	  Answers202, HTTPD_REFUSALS(kStartRefusals) },
	{ Method::Get, "/api/v1/update/online", AuthLevel::Read,
	  "whether the update server has a newer image",
	  "Asks the update server the box is set to, through the same check the box's menu "
	  "and its background check run: the server's list of images for this box is read and "
	  "searched for one newer than the running image. Nothing beyond the list is fetched "
	  "and nothing is installed; a newer image is installed from the box's own menu.\n\n"
	  "The answer is kept for a quarter of an hour and given again within it, so opening "
	  "a page twice does not ask the server twice; `fresh=true` asks now. Takes as long "
	  "as a ping and the download of a short list, longer where the server does not "
	  "answer.\n\n"
	  "A server that cannot be reached is an answer and not a refusal: `reached` is "
	  "false.\n\n"
	  "**Refusals:**\n"
	  "- `501 no-update-check`: nothing on this box can ask the update server.\n\n"
	  "**Related:** `GET /api/v1/update/local`.",
	  HTTPD_PARAMS(kOnlineParams), &kOnlineUpdateSchema, &onlineUpdate, false,
	  Answers200, HTTPD_REFUSALS(kOnlineRefusals) },
};

} // namespace

extern const RouteTable updateTable = {
	HTTPD_TABLE("update", kUpdateEndpoints)
};

} // namespace httpd
