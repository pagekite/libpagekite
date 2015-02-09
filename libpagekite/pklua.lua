-- -----------------------------------------------------------------------
-- pklua.lua - Allow libpagekite to be extended using the lua language
--
-- This file is Copyright 2011-2015, The Beanstalks Project ehf.
--
-- This program is free software: you can redistribute it and/or modify it
-- under the terms of  the Apache License 2.0  as published by the  Apache
-- Software Foundation.
--
-- This program  is distributed  in the hope that it  will be useful,  but
-- WITHOUT   ANY   WARRANTY;   without  even   the  implied   warranty  of
-- MERCHANTABILITY  or  FITNESS FOR A PARTICULAR PURPOSE.  See the  Apache
-- License for more details.
--
-- You should have received a copy of  the Apache License  along with this
-- program. If not, see: <http://www.apache.org/licenses/>
--
-- Note: For alternate license terms, see the file COPYING.md.
-- -----------------------------------------------------------------------
local pklua = {
  version = "0.0.1",
  plugins = {}
}
function pklua:_enable_defaults(enable)
  if not enable then
    for name, plugin in pairs(self.plugins) do
      if plugin.settings ~= nil then
        self:log_debug('Disabling plugin '..name)
        plugin.settings = nil
      end
    end 
  end
end
function pklua:_set_setting(plugin_setting)
  name, setting = plugin_setting:match("^(.-)=(.+)$")
  if self.plugins[name] ~= nil then
    self.plugins[name].settings = setting
    self:log_debug('Configured plugin '..name..' = '..setting)
  else
    self:log_error('No such plugin: '..name)
  end
end

-- These routines have to do with socket_server plugins -----------------
function pklua:add_socket_server(name, about, settings, func)
  self.plugins[name] = {
    plugin_type = 'socket_server',
    about = about,
    settings = tostring(settings),
    callback = func
  }
end
function pklua:_configure_socket_servers(pkm)
  for name, plugin in pairs(self.plugins) do
    if plugin.plugin_type == 'socket_server' and plugin.settings then
      host, port = plugin.settings:match("^(.+):(.+)$")
      if host == nil or port == nil then
        host, port = 'localhost', plugin.settings
      end
      pkm:_add_socket_server(name, host, tonumber(port))
    end
  end
end
function pklua:_socket_server_accept(pkm, name, sock)
  -- FIXME: Rewrite this to use coroutines, so our main event loop
  --        can handle this stuff too?
  plugin = self.plugins[name]
  if plugin ~= nil then
    return plugin.callback(sock)
  else
    self:log_error('No such plugin: '..name)
  end
end


-- A super primitive HTTP daemon... -------------------------------------
local httpd = {}
pklua.httpd = httpd
function httpd:handler(sock)
  local ctx = setmetatable({ c = sock }, httpd)
  self.__index = self
  return ctx
end
function httpd:get_headers()
  line, err = self.c:receive()
  headers = {}
  while line and line ~= "" do
    name, value = line:match("^(.-):%s*(.*)$")
    if name ~= nil and value ~= nil then
      headers[name:lower()] = value
      line, err = self.c:receive()
    else
      line = nil
    end
  end
  return headers
end
function httpd:get_request()
  line, err = self.c:receive()
  if line == nil or line == "" then
    return nil
  end
  method, path = line:match("^(%a+)%s+(%S-)%s+HTTP/%S+$")
  if method == nil then
    pklua:log_debug('Invalid HTTP request: '..line)
    return nil
  end
  pklua:log_debug('HTTP request: '..method..' '..path)
  method = method:upper()
  if (method == "GET") or (method == "POST") then
    headers = self:get_headers()
    self.request = {
      method = method,
      path = path,
      headers = headers,
    }
    -- FIXME: if Content-Length: is set, read that much as a body
    -- FIXME: Handle chunked stuff as well?
    return self.request
  end
end
function httpd:respond(mimetype, body)
  self.c:send(string.format([[HTTP/1.1 200 OK
Content-Type: %s
Content-Length: %d
Connection: close

]], mimetype, #body))
  self.c:send(body)
  self.c:close()
end
function httpd:close()
  self.c:close()
end


-- This mocks a socket, for the purposes of testing... ------------------
local fake_socket = { i = io.stdin, o = io.stdout }
function fake_socket:receive() return self.i:read(), nil end
function fake_socket:send(data) return self.o:write(data) end
function fake_socket:close() end
function pklua:test()
  handler = httpd:handler(fake_socket)
  while handler:get_request() do
    handler:respond("text/html",
                    "<html><body><h1>Hello!</h1></body></html>\n") 
  end
  handler:close()
end


return pklua
