-- This registers the function as a socket server named "web-ui"
pklua:add_socket_server("web-ui", "Enable a web-based UI on port %s", 3173,
                        function(sock)
    handler = pklua.httpd:handler(sock)
    while handler:get_request() do
        handler:respond("text/html",
                        "<html><body><h1>Hello!</h1></body></html>\n") 
    end
    handler:close()
end)
