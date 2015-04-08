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
  version = "unknown",
  manager = nil,

  -- Global limits enforced by our simplistic HTTP daemon, to
  -- thwart RAM based DOS attacks (or bugs).
  max_http_line = 10240,
  max_http_headers = 100,
  max_http_upload_bytes = 10 * 1024 * 1024,

  -- Plugin & hook registry!
  plugins = {},
  hooks = {}
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
function pklua:_init_plugins()
  for name, plugin in pairs(self.plugins) do
    if plugin.settings ~= nil and plugin.init ~= nil then
      plugin.init()
      self:log_debug('Initialized plugin '..name)
    end
  end
end

-- These routines have to do with hooks and callbacks -----------------
function pklua:_hook_bridge(hook_id, iv1, pv1, pv2)
  if self.hooks[hook_id] ~= nil then
    return self.hooks[hook_id](hook_id, iv1, pv1, pv2)
  else
    return -1
  end
end
function pklua:add_hook(hook_name, func)
  hook_id = self:get_hook_list()[hook_name]
  if hook_id ~= nil then
    self.hooks[hook_id] = func
    self:_bridge_hook_to_lua(hook_id)
    self:log_debug('Registered lua bridge for hook '..hook_name..'='..hook_id)
  else
    self:log_error('No such hook: '..hook_name)
  end
end


-- These routines have to do with socket_server plugins -----------------
function pklua:add_socket_server(name, about, settings, init, handler)
  self.plugins[name] = {
    plugin_type = 'socket_server',
    init = init,
    about = about,
    settings = tostring(settings),
    callback = handler
  }
end
function pklua:_configure_socket_servers()
  for name, plugin in pairs(self.plugins) do
    if plugin.plugin_type == 'socket_server' and plugin.settings then
      host, port = plugin.settings:match("^(.+):(.+)$")
      if host == nil or port == nil then
        host, port = 'localhost', plugin.settings
      end
      self.manager:_add_socket_server(name, host, tonumber(port))
    end
  end
end
function pklua:_socket_server_accept(name, sock)
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
local httpd = {
  http_headers = "Expires: 0\n",
}
pklua.httpd = httpd
function httpd:handler(sock)
  local ctx = setmetatable({ c = sock }, httpd)
  self.__index = self
  return ctx
end
function httpd:get_headers()
  line, err = self.c:receive(0, pklua.max_http_line) -- line buffered, bounded
  headers = {}
  while line and line ~= "" and #headers < pklua.max_http_headers do
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
  line, err = self.c:receive(0, pklua.max_http_line+1) -- line buffered
  if line == nil or line == "" then
    return nil
  end
  if #line > pklua.max_http_line then
    self:respond_with_code(414, 'Error', 'text/plain', 'URI too long\n')
    return nil
  end
  method, path = line:match("^(%a+)%s+(%S-)%s+HTTP/%S+$")
  if method == nil then
    pklua:log_debug('Invalid HTTP request: '..line)
    self:respond_with_code(400, 'Error', 'text/plain', 'Bad request\n')
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
    if headers['content-length'] ~= nil then
      length = tonumber(headers['content-length'])
      expect100 = headers['expect']
      if length ~= nil and length <= pklua.max_http_upload_bytes then
        if length > 0 then
          if expect100 ~= nil and expect100:match("^100") then
            self.c:send('HTTP/1.1 100 Continue\n\n')
          end
          self.request.data = self.c:receive(length)
          if #self.request.data ~= length then
            self:respond_with_code(400, 'Error', 'text/plain', 'Bad request\n')
            return nil
          end
        else
          self.request.data = ''
        end
      else
        if expect100 then
          self:respond_with_code(417, 'Error', 'text/plain', 'Too much data\n')
        else
          self:respond_with_code(418, 'Uhm', 'text/plain', 'I\'m a teapot\n')
        end
        return nil
      end
    end
    -- FIXME: Handle chunked stuff as well?
    return self.request
  end
end
function httpd:respond_with_code(code, msg, mimetype, body)
  self.c:send(string.format([[HTTP/1.1 %d %s
Server: PageKite-Lua/%s
Content-Type: %s
Content-Length: %d
%sConnection: close

]], code, msg, pklua.version, mimetype, #body, self.http_headers))
  self.c:send(body)
  self.c:close()
end
function httpd:respond(mimetype, body)
  return self:respond_with_code(200, "OK", mimetype, body)
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
