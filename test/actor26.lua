if _CONTEXT == 'main' then
    local ch1 = spawn_vm('.')
    local ch2 = spawn_vm('.')

    local a = {}
    ch1:send{ to = ch2, body = { a, a } }
else assert(_CONTEXT == 'worker')
    local inbox = require('inbox')
    local m = inbox:receive()

    if m.to then
        m.to:send{ body = m.body }
    else
        print(type(m.body), #m.body)
        print(type(m.body[1]), #m.body[1])
        print(type(m.body[2]), #m.body[2])
    end
end
