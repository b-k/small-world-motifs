package.path = package.path .. [[;/usr/local/share/lua/5.1/?.lua;./?.lua]]

local tx = require "pl.tablex"
--local dbg = require "debugger"
local net = {}

net.open = function(self, n)
    --open database number n
    local out = {}
    out.db_number = n
    db_open(n)
    setmetatable(out, self)
    self.__index=self
    return out
end

-- Simple preferential attachment
net.generate_a_graph = function(db, N, lambda)
    db:add_edge(1,2)
    for i=3, N do
        for _=1, draw_exponential(lambda) do
           local parent = db:preferential_draw()
           db:add_edge(parent, i)
end end end

-- Holme and Kim's method: pereferentail attachment plus uplinks to aunts and uncles
net.generate_a_graph_sibs = function(db, N, lambda)
    db:add_edge(1,2)
    for i=3, N do
        for _=1, draw_exponential(lambda) do
           local parent = db:preferential_draw()
           local parent_adjacents = db:get_key(parent, 'p')
           if #parent_adjacents > 0 then
               local adjacent = parent_adjacents[math.random (#parent_adjacents)]
               if adjacent ~= i then db:add_edge(adjacent, i) end
           end
           if parent ~= i then db:add_edge(parent, i) end
end end end

-- Krapinsky's more symmetric in/outdegrees
net.generate_a_graph_sym = function(db, N, lambda)
    db:add_edge(1,2)
    for i=3, N do
        u = draw_uniform(1)
        if u < lambda then
           local parent = db:preferential_draw()
           db:add_edge(parent, i)
       else
           p =  db:preferential_draw()
           c =  db:preferential_draw(1)
           db:add_edge(c, p)
end end end

net.generate_a_graph_sym_sibs = function(db, N, lambda, hk_mu, mu)
    u = draw_uniform(1)
    u2 = draw_uniform(1)
    db:add_edge(1,2)
    for i=3, N do
        for _=1, draw_exponential(lambda) do
           local parent = db:preferential_draw()
           if u < mu then --Krapivsky, another up-link
               c =  db:preferential_draw(1)
               db:add_edge(c, parent)
           end
           if u2 < hk_mu then --H&K, another down-link
               local parent_adjacents = db:get_key(parent, 'p')
               if #parent_adjacents > 0 then
                   local adjacent = parent_adjacents[math.random (#parent_adjacents)]
                   if adjacent ~= i then db:add_edge(adjacent, i) end
               end
           end
           db:add_edge(parent, i)
end end end

net.generate_a_graph_sym_sibs_flipped = function(db, N, lambda, hk_mu, mu)
    u = draw_uniform(1)
    u2 = draw_uniform(1)
    db:add_edge(2,1)
    for i=3, N do
        for _=1, draw_exponential(lambda) do
           local carent = db:preferential_draw(1)
           if u < mu then --Krapivsky, another up-link
               p =  db:preferential_draw(1)
               db:add_edge(carent, p)
           end
           if u2 < hk_mu then --H&K, another down-link
               local carent_adjacents = db:get_key(carent, 'c')
               if #carent_adjacents > 0 then
                   local adjacent = carent_adjacents[math.random (#carent_adjacents)]
                   if adjacent ~= i then db:add_edge(i, adjacent) end
               end
           end
           db:add_edge(i, carent)
end end end

net.preferential_draw = function(db) return preferential_draw(db.db_number) end

net.db_dump_1d = function(db, direction) --prints
    local ok=db:get_all_keys(direction)
    for i =1, #ok do
        io.write(ok[i] .. "|")
        local vals = db:get_key(ok[i], direction)
        for j =1, #vals do io.write (vals[j] .. "\t") end
        print()
    end
end

net.degree_dump = function(db, direction) --returns a table for you to print
    local outp ={}
    local pc= db:get_all_keys(direction)
    for _,v in pairs(pc) do
        local ct = tx.size(db:get_key(v, direction))
        if outp[ct]==nil then outp[ct]=0 end
        outp[ct] = outp[ct]+1
    end
    return outp
end

net.db_dump = function(db)
    print("p→c→→→→→")
    net.db_dump_1d(db, 'p')
    print("c→p→→→→→")
    net.db_dump_1d(db, 'c')
end

net.read_csv = function(db, path)
  for line in io.lines(path) do
    local L,R = line:match("%s*(.*)|%s*(.*)")
    db.add_edge(db, L, R)
  end
end

net.close = function(fromdb) db_close(fromdb.db_number) end

net.cut_graph = function(db, p, do_color) cut_graph(db.db_number, p, do_color) return db end

net.find = function(db, node, listkey, p_or_c) return db_find(db.db_number, node, listkey, p_or_c) end
net.get_degree = function(db, node, p_or_c) return get_degree(db.db_number, node, p_or_c) end
net.get_key = function(db, k, direction) return get_key(db.db_number, k, direction) end
net.get_all_keys = function(db) return get_all_keys(db.db_number) end
net.rm_node = function(db, node) return db_rm_node(db.db_number, node) end
net.add_edge = function(db, from, to) add_edge(db.db_number, from, to) end
net.rm_edge  = function(db, from, to)  rm_edge(db.db_number, from, to) end
net.declare_dual  = function(db, db2, db_ref, size) declare_dual(db.db_number, db2.db_number, db_ref.db_number, size) end

net.get_subgraphs = function(final, tr) get_motifs(final.db_number, tr.db.db_number, tr.size) end

net.cp = function(fromdb, to)
    local outdir = get_outdir() --this was argv[1] on the C side.
    db_close(fromdb.db_number)
    os.execute([[cp ]] .. outdir .. [[/dbcp.]]..fromdb.db_number .." ".. outdir .. [[/dbcp.]] ..to)
    os.execute([[cp ]] .. outdir .. [[/dbpc.]]..fromdb.db_number .." ".. outdir .. [[/dbpc.]] ..to)
    os.execute([[cp ]] .. outdir .. [[/dbcl.]]..fromdb.db_number .." ".. outdir .. [[/dbcl.]] ..to)
    db_open(fromdb.db_number)
    return net:open(to)
end

return net
