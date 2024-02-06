package.path = package.path .. [[;/usr/local/share/lua/5.1/?.lua;./?.lua]]
local netlib, load_error = package.loadlib("netlib.so", "luaopen_register")
if netlib == nil then
    print(netlib, load_error)
    else netlib()
end
print("bbin, shouldn't be nil", update_beta_binomial)

local mean="sum(ct)/count(*)"

local normalize_a_table = function(subgraphs)
    print("Porm", #subgraphs)
    sum=0
    for k, v in pairs(subgraphs) do
        sum = sum + v
    end
    for k, v in pairs(subgraphs) do
        subgraphs[k] = {sum, v/(sum+0.0)}
    end
    print("Norm", #subgraphs)
    return subgraphs
end

function flatten(subgraphs)
    local out = {}
    for _, subtab in pairs(subgraphs) do
        for k, v in pairs(subtab) do
            out[k] = v
        end
    end
    return out
end

local get_a_motif_tab = function(dir)
    local handle = io.popen([[
	sqlite3 ]] .. dir .. [[/graphs.db "select subgraph, count(*) from graphs group by subgraph"
    ]])

    subgraphs = {}
    subgraphs[4] = {}   --We'll want percents within subgraph sizes
    subgraphs[8] = {}
    subgraphs[13] = {}
    subgraphs[13] = {}

    ctr = 0
    for line in handle:lines() do
        local key, value = line:match("([^|]+)|([^|]+)")
        print(dir, key, value)
        if key ~= nil and value ~= nil then
            subgraphs[#key][key] = value
            ctr = ctr + 1
        end
        print(dir, key, value, subgraphs[key], #subgraphs,  "**")
    end

    print (dir, ctr)
    handle:close()

    normalize_a_table(subgraphs[4])
    normalize_a_table(subgraphs[8])
    normalize_a_table(subgraphs[13])
    return flatten(subgraphs)  --done with subgraphs, return just the motifs.
end

function beta_bi_loop(inlist)
    a=0.5
    b=0.5
    for _, value in pairs(inlist) do
        if value ~= 0 then
            a, b = update_beta_binomial(a, b, value[1], value[2])
            print("step: ", a, b, "μ: ", a/(a+b))
        end
    end
    return {a, b}
end

local overalltab = {}   -- a list of all motifs, for for-looping over
local motiftab_tab = {}  --one table for each directory

--Get each subtable, and make sure every subgraph is in the overall list.
for i, motif in ipairs(arg) do
	motiftab_tab[motif] = get_a_motif_tab(motif)
    print(arg, #(motiftab_tab[motif]))

    for k, v in pairs(motiftab_tab[motif]) do
		overalltab[k] = {}
	end
end

--Now go through all lists again and add either a zero or a value for each motif
for i, outdir in ipairs(arg) do
    for k, v in pairs(overalltab) do
		if motiftab_tab[outdir][k] == nil then
		    table.insert(overalltab[k], 0)
		else
		    table.insert(overalltab[k], motiftab_tab[outdir][k])
		end
	end
end

for k, v in pairs(overalltab) do
	posterior= beta_bi_loop(v)
	print("For", k, "(α, β, μ)=", posterior[1], posterior[2], posterior[1]/(posterior[1]+ posterior[2]))
	print("\t given", overalltab[k])
    for kk, vv in pairs(overalltab[k]) do
print(k, "|", kk, vv)
	end
end
