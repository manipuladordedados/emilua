local extract_host, tcp_dial, socket_new = ...
return function(ep, tls_ctx)
    local host = extract_host(ep)
    local sock = tcp_dial(ep)
    sock:set_option('tcp_no_delay', true)
    sock = socket_new(sock, tls_ctx)
    sock:set_verify_mode('peer');
    sock:set_verify_callback('host_name_verification', host);
    sock:set_server_name(host)
    sock:client_handshake()
    return sock
end
