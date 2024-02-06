--package.path = package.path .. [[;./?.lua]]
local net = require "net"
local tx = require "pl.tablex"
--local dbg = require "debugger"


local check_list = function(want, have)
    if(tx.size(want) ~= tx.size(have)) then error("size issue: want is " .. tx.size(want).."; have is "..tx.size(have),2) end
    for k,v in pairs(want) do
        if have[k] == nil then error("Missing "..k, 2) end
    --    if have[k] ~= want[k] then error(have[k] .. " ‡ ".. want[k], 2) end
        assert( have[k] == want[k])
    end
    return true
end

local check_list_keys = function(want, have)
    if(tx.size(want) ~= tx.size(have)) then error("size issue",2) end
    for k,_ in pairs(want) do if have[k] == nil then error("Missing "..k, 2) end end
    return true
end

local reset_tnet = function()
    --db_close(10)
    local outdir = get_outdir()
    os.execute([[rm -f ]] .. outdir .. [[/dbcp.10 ]] ..outdir.. [[/dbcp.10]])
    return net:open(10)
end

local onechain = function()
    local tnet = reset_tnet()
-- 1→2→3
    tnet:add_edge(1,2)
    tnet:add_edge(2,3)

-- Now add 2→4
    tnet:add_edge(2,4)
    assert(tnet:get_degree(2, 'p') == 2)
    --print("xx")
-- rm 4
    tnet:rm_node(4)

    --for k,_ in pairs(a:get_aura(tnet)) do print(k) end
    assert(tnet:get_degree(1, 'p') == 1)
    assert(tnet:get_degree(2, 'p') == 1)
    assert(tnet:get_degree(3, 'c') == 1)

    tnet:add_edge(2,4)
    assert(tnet:get_degree(2, 'p') == 2)
    --status: 1 has one child zero parents
    --2: two kids, one parent
    --3: zero kids, one parent
    --4: zero kids, one parent

    local pdegree = tnet:degree_dump("p")
    local cdegree = tnet:degree_dump("c")
    --for k, v in pairs(pdegree) do print("p", k, v) end
    --for k, v in pairs(cdegree) do print("c", k, v) end

    --dbg.pp(pdegree)
    --dbg.pp(cdegree)
    check_list({[0]=1, [1]=3}, cdegree)
    check_list({[0]=2, [1]=1, [2]=1}, pdegree)
end

local tests = {
go = function()
    onechain()
    print("✓ Pretest ok")
end}

return tests
