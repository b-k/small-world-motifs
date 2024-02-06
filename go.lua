-- package.path = package.path .. [[;./?.lua]]
package.path = package.path .. [[;/usr/local/share/lua/5.4/?.lua;./?.lua]]

local netlib, load_error = package.loadlib("/home/ldb/@/netlib.so", "luaopen_register")
--local netlib, load_error = package.loadlib("netlib.so", "luaopen_register")
--print(netlib, load_error)

netlib()
set_outdir(arg[1])


local net = require "net"
local tests = require "tests"


local motif_size = 7
local net_size = 20000
math.randomseed(1) --it's the apophenia seed that matters more, tho
rng_init(-1)
local pct_keep = .35

local graphgen = function(number, count, exp)
    local db = net:open(number)
    --db:generate_a_graph(count, exp)
    db:generate_a_graph_sym_sibs_flipped(count, 2.7, .8, .25)
    return db
end

function go (net_in, basedata, msize, top)
   if msize == 1 then return net_in end

   print([[
   -------- Motif search step ]] .. top - msize)
   local n2 = net:open(200+(top-msize))
   net_in:declare_dual(n2, basedata.db, basedata.size)
   if (top > msize) then n2:get_subgraphs(basedata) end
   return go(n2, basedata, msize-1, top)
end

local read_and_cut = function(psv)
    local dbone = net:open(1) --graphgen(1, 300000, 3)
    dbone:read_csv(psv)
    local dbtwo = dbone:cp(2)

    dbone:close()

    print("kid count pre-cut " .. #dbtwo:get_all_keys('c'))
    dbtwo:cut_graph(motif_size, 1) -- 1=do coloring
    print("kid count post-cut " .. #dbtwo:get_all_keys('c'))
    dbtwo:cp(3333):close() --backup
    return dbtwo
end

local load_from_bkup = function(which)
    local dbbkup = net:open(which)
    return dbbkup:cp(2)
end

tests.go()
--local dbone = read_and_cut("tt.psv")

local dbone = graphgen(1, net_size, 3) -- Generational
    dbone:cp(9999):close() -- Generational backup
local dbone = load_from_bkup(9999):cut_graph(pct_keep) -- Loaded

local tr={}
tr.db=dbone:cut_graph(pct_keep)
tr.size=motif_size

local n4 = go(dbone, tr, motif_size, motif_size)
