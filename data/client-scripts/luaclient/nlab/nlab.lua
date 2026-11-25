#!/usr/bin/luaclient

--[[
	nlab - Neutrino Lua API bridge

	Copyright (C) 2021 Sven Hoefer <svenhoefer@svenhoefer.com>
]]

nlab = "Neutrino Lua API bridge"
version = "0.03"

a = {}
r = ""

if arg[2] == nil then
	r = nlab .. " v" .. version
end

for i, v in ipairs(arg) do
	a[i] = v:lower()
end

if a[2] == "get" then
	-- APIVERSION: see src/gui/lua/luainstance.cpp
	-- DIR and SCREEN: see src/scriptapi/script_constants.cpp

	if a[3] == "apiversion" then
		r = APIVERSION.MAJOR .. "." .. APIVERSION.MINOR
	elseif a[3] == "apiversion.major" then
		r = APIVERSION.MAJOR
	elseif a[3] == "apiversion.minor" then
		r = APIVERSION.MINOR
	elseif a[3] ~= nil then
		-- dir.<name> and screen.<name> answer any entry of DIR and SCREEN,
		-- so a directory added there needs no line here
		for prefix, tbl in pairs({ ["dir."] = DIR, ["screen."] = SCREEN }) do
			if a[3]:sub(1, #prefix) == prefix then
				r = tbl[a[3]:sub(#prefix + 1):upper()]
			end
		end
	end

elseif a[2] == "misc" then
	-- see src/gui/lua/lua_misc.cpp

	misc = misc.new()
	if a[3] == "postmsg.standby_on" then
		r = misc:postMsg(POSTMSG.STANDBY_ON)
	elseif a[3] == "getvolume" then
		r = misc:getVolume()
	elseif a[3] == "setvolume" and a[4] ~= nil then
		r = misc:setVolume(a[4])
	elseif a[3] == "ismuted" then
		r = misc:isMuted()
	elseif a[3] == "getrevision" then
		r = misc:GetRevision()
	end
	misc = nil

elseif a[2] == "video" then
	-- see src/gui/lua/lua_video.cpp

	video = video.new()
	if a[3] == "showpicture" and a[4] ~= nil then
		video:zapitStopPlayBack()
		r = video:ShowPicture(arg[4])
	elseif a[3] == "stoppicture" then
		video:zapitStopPlayBack(false)
		r = video:StopPicture()
	end

end

if r ~= nil and r ~= "" then
	return tostring(r) .. "\n"
end
