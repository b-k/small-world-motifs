db = arg[1].."/graphs.db"

sh = function(doit) os.execute(doit) end --save some typing
Qp = function(query) 
    print([[sqlite3 --header ]] .. db .. [[ "]].. query ..[["]])
    sh([[sqlite3 --header ]] .. db .. [[ "]].. query ..[["]]) end
Q = function(query) sh([[sqlite3 --header ]]  .. db .. [[ "]].. query ..[["]]) end


init = function(dir)
    --sh([[apop_text_to_db -d"|" -ea -nc -N "tag,node" ]]..dir..[[/out.nodes.psv nodes ]]..db)
   sh([[apop_text_to_db -d"|" -ea -nc -N "tag,subgraph" ]]..dir..[[/out.graph.psv graphs ]]..db)
end

local mean="sum(ct)/count(*)"

get_subgraph_frequencies = function()
    Qp([[ select count(*) ct, subgraph, length(subgraph) L from graphs group by subgraph having ct>10 order by L, ct
    ]])
end

local readin = function(dir)
    sh([[
for i in {0..5}; do
    #apop_text_to_db -d"|" -ea -nc -N "tag,node" ]] .. dir .. [[/out.nodes.psv nodes ]] .. dir .. [[/graphs.db
    apop_text_to_db -d"|" -ea -nc -N "tag,subgraph" ]] .. dir .. [[/out.graph.psv graphs ]] .. dir .. [[/graphs.db
done
    ]])

--normalize to frequency within size class

if dir2 ~= "nope" then
    sh([[ sqlite3 -header ]] .. dir .. [[/graphs.db "
    attach ']] .. dir2 ..[[/graphs2.db' as g2;
    with ct_tab as (select count(*) ct1, subgraph s1, length(subgraph) L from graphs group by subgraph),
        ct_tab2 as (select count(*) ct2, subgraph s2, length(subgraph) L from g2.graphs group by subgraph),
        freq as (select count(*) ct, length(subgraph) L from graphs group by L),
        freq2 as (select count(*) ct, length(subgraph) L from g2.graphs group by L)

    select  (ct1*freq2.ct)/(ct2*freq.ct + 0.0) ratio, s1, ct1, ct2, freq.ct, freq2.ct
        from ct_tab join ct_tab2 on s1==s2
        join freq on freq.L==ct_tab.L
        join freq2 on freq.L==freq2.L
    and ratio >2 or ratio < .5 order by ratio
    "
    ]])
end
end


init(arg[1])
if #arg>1 and (string.len(arg[2])>0) then dir2=arg[2] else dir2="nope" end
readin(arg[1], dir2)
get_subgraph_frequencies()
