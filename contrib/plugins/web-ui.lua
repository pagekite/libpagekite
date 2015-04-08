-- This registers the function as a socket server named "web-ui"
function web_root(handler)
    handler:respond("text/plain", "ROOT\n")
end
function web_metrics(handler)
    metrics = pklua.manager:get_metrics()
    local names = {}
    for name, val in pairs(metrics) do
        table.insert(names, name)
    end
    table.sort(names)
    local output = ""
    for _, name in ipairs(names) do
        output = output .. name .. ": " .. metrics[name] .. "\n"
    end
    handler:respond("text/plain", output)
end
pklua:add_socket_server("web-ui", "Enable a web-based UI on port %s", 3173,
                        function() -- init
    pklua:add_hook('PK_HOOK_TICK', function(hook_id, iv1, iv2, iv3)
        pklua:log_debug('Tick!')
        return -2
    end)
end,
                        function(sock) -- handler
    local handler = pklua.httpd:handler(sock)
    while handler:get_request() do
        if handler.request.path == "/" then
            web_root(handler)
        elseif handler.request.path == "/metrics.txt" then
            web_metrics(handler)
        else
            handler:respond_with_code(404, "Not found", "text/plain",
                                      "Not found\n")
        end
    end
    handler:close()
end)
