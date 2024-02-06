# Probabilistic motivic analysis for large graphs

This package, a work in progress, provides a simple algorithm to generate all subgraphs of all sizes up to the user's chosen maximum.
It does so by generating a sequence of dual graphs, in which every node in the previous graph becomes a set of
edges in the next graph, and every edge becomes a node.

It is built on a library of Lua functions which use the (plain C) Berkeley Database to represent
graphs. The library has many graph functions which may make it useful in its own right.

This package also includes a graph generator, using a preferential-attachment method described below.
Or, graphs can be read from a pipe-delimited file (p.d.f.).
Once a graph is read in to the database format used here, it can be saved in that format for faster loading.

* This was written by a U.S. Government employee in the course of work, so U.S.C. Title 17 (copyright) does
  not apply. As a courtesy, please cite the work if you use it in your own.

* For especially large graphs,
this package includes an option to take a subset of the nodes in the graph, as set in the sample script, `go.lua`.
Then the process of generating samples is embarassingly parallel—just write a shell script to run this program
with a different random seed on every processor—and generates draws with uniform
probability within the set of subgraph types.
[Your shell may be able to do this in one line. E.g., `for i in {0..10}; do make OUT=out.$i; done`.]
The `final_to_sql.lua` and `boot_moments.lua` sample scripts find means and variances of the distribution of the count of
each motif given a set of runs.

* This program is intended for graphs of millions of nodes.
For smaller subgraphs of only thousands of nodes, no sampling is necessary.

* If you want subgraphs of size <em>N</em>, the method necessarily generates subgraphs of size 3
through <em>N-1</em> along the way.

* The algorithm searches for induced motifs, meaning there are no edges between the nodes in the motif 
that are not part of the motif itself.

* All motifs found by this program are connected. In a full triadic census, triads such as one edge A→B plus a node not
connected to either A or B—or even the null triad of three entirely disconnected nodes—would be enumerated.
For a large graph, this is approximately the count of edges times the count of nodes. Similarly for
a larger disconnected motif, their count is approximately the product of the counts of its separate submotifs.

* Output is another plain-text p.d.f.

## Writeup

Please see the doc directory for a full writeup.
There is a copy in Portable Document Format, but the source is also present; on a
POSIX-compliant system with XeLaTeX installed just run `make`. [If you only have plain old LaTeX,
just replace `xelatex` with `latex` in the first target in the `makefile` and it should work.]
The three diagrams were generated using graphviz; see `makefile`.

The lead application was [this paper](https://doi.org/10.48550/arXiv.2401.12118) on the United
States's network of business entities, where statistics on 4- and 5-motifs are presented.

## Technical

This Readme gives a more technical overview of the basic execution of the program and the components provided.
The code itself also has in-line documentation for the finer details.

### Running it
A `Dockerfile` building an environment in which the program runs is provided.
Try `make dock` to generate the image and `make shell` to open a shell in a container.
The project directory on the parent system is mounted in the `/home/ldb/@` directory in the container.

* If not using the Dockerfile, the requirements are a C compiler, the Berkeley database library, the Apophenia library of
statistical functions, and a Lua interpreter plus two small Lua utility packages.
Everything is installable via `apt-get` and `luarocks`, if those are available, and the commands
listed in the `Dockerfile` calling those package managers may also be the ones you need on your local environment.

* A `makefile` is provided, and it takes an argument giving the output directory name, e.g.,
  `make OUT=test1` will create a directory named `test1` and start putting output there.
You are strongly encouraged to `rm -rf test1` before re-running.

* The `go.lua` script is the entry point and what you will want to modify for your purposes.

* Preferential-attachment graphs can be generated in the lua script; the example in `go.lua` may be
  easily modified for your needs. There are a few options toward the top of `netlib.lua`.
  At the bottom, you'll see some commented-out elements to load in a pre-generated graph.

* Or the graph can be read via a simple pipe-delimited file where each node has a positive integer value, and each row of the
file lists an edge, such as `12|187`. The node numbers need not begin at one or be sequential. The file should have no header.

* Compile using the makefile (i.e., run `make`), then run via `./main`. Provide a single argument
  like `./main out.dir` to put the output and all intermediate databases in a directory named `out.dir`.
  Anything named `.db` in that directory could be considered intermediate and removed after the runs
  (or interrogate it further using any Berkeley database utilities).
  Output is written to a file of your choosing within that directory, specified in `go.lua`.

* Motifs are expressed as an upper-diagonal matrix, expressed as a string.
  If nodes 0 and 1 own node 2, and node 2 owns nodes 3 and
  4, the ownership subgraph will include rows for ownership by nodes 0, 1, 2, and 3 (in a DAG, there
  must be a last node that doesn't own anything); and columns for who owns nodes 1, 2, 3, and 4:
  
```
  1234
  ----
0|0100
1| 100
2|  11
3|   0
```
which the output writes as the string `0100 100 11 0`.

The J-shaped graph 0→2, 2→3, 1→3 (010 01 1)
is isomorphic to the graph 1→2, 2→3, 0→3 (001 10 1). The output is canonicalized to the subgraph
string with the highest numeric value: 010011 > 001101, so both graphs are expressed using the first
string.

* Final output is two pipe-separated value files, currently named `out.graph.psv`  and
  `out.nodes.psv`. On each row of the first is the subgraph number `|` subgraph shape string as above.
  Each row of the second is subgraph number `|` node id. The node id refers to your original input
  data. An M-subgraph will have M such rows in `out.nodes.psv`. This format is intended to be
  SQL-friendly. If you provide a header, this will read in nicely to sqlite. A sample `queries.lua`
  does so using a command-line utility and queries aggregate statistics and optionally compares two
  runs.

### The algorithm
Here is a summary of the procedure from a more technical perspective. See the writeup for a more
mathematical overview. All functions referenced are described in detail in the components list below.

* Build a graph via `generate_graph` or `read_csv`.
* In the `go` function in `go.lua`, loop over subgraph sizes; let the current size be $M$. Beginning with size 3,
  * Call `declare_dual` to generate a new graph.
    * For each edge in the pre-graph, put a node in the post-graph. Its label will be a tuple giving the join of
      the labels of the nodes at either side of the edge in the pre-graph.
    * For each node in the pre-graph, add edges in the post-graph linking all nodes representing
      pre-graph edges adjacent to the pre-graph node. But after linking, check that the newly
      generated node is already in the database. If it is, discard the new duplicate.
    * The label for each node in the post-graph is a subgraph in the original graph with size $M$. Via
      `lineage_sort`, generate the canonicalized subgraph between the nodes in a given label. Write to output.

### The technical components

* `netlib.c`
The core of the system is a Berkeley database (a key-value store) expressing a graph, or set of
graphs in multiple databases.
This component is easy to extract from the rest of the package, and could be useful
without regard to the motivic analysis. The C-side components have corresponding Lua equivalents
(see below) for those who prefer Lua over C.

#### Database structure
 * The database is actually four sub-databases, the `pc` database listing parent-to-child edges, the
  `cp` database listing child-to-parent edges, and two side-databases named `colordb` and `dupdb` to keep track of details
  for the purpose of subgraph calculation. If using the graph database outside the context of 
  the subgraph calculator only the first two databases will be of any relevance.
  Remove the other two from the `pcdb_s_pre` and `close_dbs` functions if desired.
 * One key in the `pc` database is a node, and the value associated with that key is a list of the
child nodes of that parent.
Conversely, the `cp` database has keys of a child node, and associated value is a list of that child's parents.
* There is a structure on the C side holding a cumulative distribution function (CDF) of the number
   of nodes in the list of parents. This is used for building networks via rich-get-richer preferential
   attachment.
   * The structure currently doesn't handle node removal. Build your graph via whatever algorithm
     you prefer (as long as it doesn't involve deleting nodes as part of the generation process),
     or read the graph via text file input, then remove nodes after the CDF is no longer needed.
* There is no table of nodes. If you want a list, `get_all_keys` iterates over the join of `pc` keys plus `cp` keys.
  Standalone nodes with neither parents nor children cannot be expressed.
 
 * `net.lua`
The C functions for general graph database handling have a Lua front-end. See the sample `go.lua`
script for examples of the use of most of these. The functions are summarized in a Lua object
named `net`, including:
   * The `net.open` function generates a new database tagged with the number provided. For example,
     `local twonet = net:open(2)` gives a handle to database two, and generates files on disk named `dbpc.2` and `dbcp.2`.
     The `net.close` function is largely unnecessary; the database closes itself on exit.
   * The `net.cp` function copies one database into another. A Berkeley database is a single file,
     of which we have several, featuring `pcdb.n` and `cpdb.n`, where _n_ is the database number.
     Then copying the database is an operating system-level file copying operation, not a record-level operation.
   * The `net.add_edge` function will add any not-yet-named nodes as well as adding the edge you
     specified to both the parent→child and child→parent tables. Its inverse is `net.rm_edge`.
   * The `net.rm_node` function removes all edges related to the node you specify.
   * The `net.preferential_draw` function uses the above-mentioned CDF to draw a node
     in proportion to its popularity. See `net.generate_a_graph` in `net.lua` for a typical use.
   * The `net.read_csv` function reads from a pipe-delimited file; see above for format.
     This function is technically a misnomer, because commas are not an option.
   * The `net.get_key` function retrieves the list of parents or children the given graph node has.
     Specify `'p'` or `'c'` as the second argument to select whether the _input_ node is a parent or a child.
     For example, `twonet:get_degree(10, 'p')` checks the parent-child table and reports the number of children
   node ten has.
   * The `net.get_degree` function retrieves a key as would `net.get_key` but returns only how many
     elements the list has.
   * The `net.get_all_keys` function returns the list of all nodes.
   * The `net.db_dump_1d` function prints the full state of the network, with one `parent|child` row
     per edge (even if that means printing millions of rows).

Finally, `net.get_subgraphs` does the subgraph search. But you will want to use the `go` function in
`go.lua` to do the search, because correctly using `get_subgraphs` depends on incrementally doing
larger and larger subgraphs, which the loop in `go` does.
     
### Support structures and per-file notes
These are objects packaging elements of the procedure. They don't stand alone.

 * `motifs.c`
This file does the work of dealing with subgraph strings as described above.
   * `generate_motif_string`: build the upper-diagonal adjacency matrix for the nodes in the
     list as described above.
   * `get_sets`: The label for every node in an M-graph is an M-subgraph of the original graph. This
     function calls `generate_motif_string`, then writes the result to the output files.
   * `get_motif`: Does the work to go from a list of nodes to the subgraph
     string expressing the upper-diagonal ownership matrix as described above.
     It needs to sort the nodes so parents always appear before their children, which
     `lineage_sort` does, and which is the first time the program assumes acyclicity.
     If there is a cycle, the order of nodes in the motif string is undefined, meaning some motifs
     will be expressed in multiple ways that could conceivably be merged.
   * `canonicalize`: Does the search for subgraph string with the largest numeric value.

 * `nodeinfo_s`
The main struct used in `dual.c` below, maintaining the node labels and the list of adjacent nodes.

These are stored in the now-misnamed `colors.db` [The coloring technique was baroque and didn't
work as well as the dual graph method now implemented].

Each node has a unique ID. With the first graph, this is as given by the input data or generated
graph; subsequently it is a simple incremented counter across the full sequence of duals.

The labels are stored in a simple array named `elmts`. Right now it's hard-coded in `netlib.h` to a fixed size of 15,
which is sufficient to find 14-subgraphs.

The list of adjacent nodes are maintained for converting nodes from the prior graph to edges in the
dual. This is a fixed list of unknown and possibly very large size, so it is `malloc`ed as needed.
The `adj_len` element of the struct gives the current number of adjacencies.
This is stored in the struct in an extremely C manner: declare an array of length 1 at the end of
the `nodeinfo_s` definition, but `malloc` the struct to whatever the needed size might be and ignore the claim
that the array is only one element. All work with the BDB requires `malloc`ing or directly
stating the size of the struct to be stored or retrieved, so this is typically seamless.

 * `netlib.c:dup_exists`: Given a list of element ids, concatenate them into a string, and check
     whether that string is already in the `dp` dup-checking database. If it isn't store it there.
    By the design of the algorithm, two dups should be either neighbors or neighbors of neighbors,
    so one could conceivably predict when a neighbor is going to be a dup.
    But the accounting gets onerous and we just dump everything into `dp`, a global lookup table.
 * `dual.c:join_nodes`: first check whether joining two nodes would duplicate an extant node.
   If it would not, generate a new node with the joined element lists. The adjacency lists are not
   handled, because this function is only used for nodes with no adjacency info (see below).
 * `netlib.c:add_adjacency_1d`: Do the memory allocation work to put one more element on the adjacency
   list of a `nodeinfo_s` struct.


 * `dual.c`
The main procedure to find subgraphs by generating the dual of the input graph. Recall that the Lua
script does the loop to call this to generate the 3-subgraph, 4-subgraph, 5-...

   * `cut_graph`: Defined in this file, does the sampling from the graph, by removing all but the given
     percentage of nodes. Call once for the original graph. The graph is itself torn apart via
     `rm_node`, so do a `db:cp` operation if you want to retain the intact original.
   * `declare_dual`: The real action. Get all nodes in the pre-graph, then for each node:
       * Get its adjacent nodes via the lists of edges.
       * For each adjacent node, add to the dual graph.
           * Get the `nodeinfo_s` for both, then call `join_nodes` to generate the node for the dual.
           * In `join_new_neighbor`, go through all extant adjacencies for the pre-graph nodes, and
             add an edge in the dual graph to the just-generated node.
           * In the same function use `add_adjacency` to add the new node to both pre-graph nodes'
             adjacency lists.
