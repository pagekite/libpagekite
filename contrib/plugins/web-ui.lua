-- This registers the function as a socket server named "web-ui"
function web_root(handler)
    handler:respond("text/plain", "ROOT\n")
end
function web_vars(handler)
    vars = pklua.manager:get_vars()
    local names = {}
    for name, val in pairs(vars) do
        table.insert(names, name)
    end
    table.sort(names)
    local output = ""
    for _, name in ipairs(names) do
        output = output .. name .. ": " .. vars[name] .. "\n"
    end
    handler:respond("text/plain", output)
end
pklua:add_socket_server("web-ui", "Enable a web-based UI on port %s", 3173,
                        function(sock)
    local handler = pklua.httpd:handler(sock)
    while handler:get_request() do
        if handler.request.path == "/" then
            web_root(handler)
        elseif handler.request.path == "/vars.txt" then
            web_vars(handler)
        else
            handler:respond_with_code(404, "Not found", "text/plain",
                                      "Not found\n")
        end
    end
    handler:close()
end)
