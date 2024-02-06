#include "netlib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdbool.h>
double draw_from_uniform();
void get_sets(pcdb_s infodb, pcdb_s reference_db, int size, char *, char *);
int dup_exists(DB *db, ptr_len_s elmts, int id);

int Max_future_nodes= 5000;

int preferential_draw(pcdb_s db, int indicator){
    //degree distribution maintained via netlib.c:updated_degree.
    return (indicator ? db.ba_indegree : db.ba_outdegree)[(int)(draw_from_uniform()*db.ba_Σ_size)];
}

/* Hubs with hundreds of nodes are a problem.
   The next few functions find sets of nodes whose sole edges are all to a single hub.
   Their call is currently commented out, though, in cut_graph.
   Such nodes are identical as far as subgraph generation is concerned, so it is more efficient to reduce them all to a single representative node, then assign a weight to that node.
   This is called from cut_graph—but it is currently commented out, and the weights are not reported.
   */
void reweight(pcdb_s db, int node){
    nodeinfo_s *n = db_get_info(db.colordb, node);
    n->weight = n->weight ? n->weight+1 : 2;
    db_set_info(db.colordb, n);
    free(n);
}

void find_singletons(ptr_len_s relations, DB *pc_or_cp, pcdb_s db){
    int first_singleton = -100;
    for (int c=0; c<relations.len; c++){
        ptr_len_s subrelations = db_get_key(pc_or_cp, relations.ptr[c]);
        if (subrelations.len==0){
            if (first_singleton==-100)
                first_singleton= relations.ptr[c];
            else {
                reweight(db, first_singleton);
                rm_node(&db, relations.ptr[c]);
            }
            free(subrelations.ptr);
        }
    }
}

void reweight_neighborhood(pcdb_s db){
    ptr_len_s list = get_all_keys(db);
    for (int ego=0; ego< list.len; ego++){
        ptr_len_s cs = db_get_key(db.pcdb, list.ptr[ego]);
        find_singletons(cs, db.pcdb, db);
        ptr_len_s ps = db_get_key(db.cpdb, list.ptr[ego]);
        find_singletons(ps, db.cpdb, db);
        free(cs.ptr); free(ps.ptr);
    }
}

static void cut_graph(pcdb_s *db, double retain_prob){
    ptr_len_s list = get_all_keys(*db);
    printf("Sampling from the graph; Precut ct: %i; ", list.len);
    for (int p=0; p< list.len; p++)
        if(draw_from_uniform() > retain_prob) rm_node(db, list.ptr[p]);
    free(list.ptr);
    list = get_all_keys(*db);
    printf("Postcut ct: %i; ", list.len);
    free(list.ptr);
    /*
    reweight_neighborhood(*db);
    list = get_all_keys(*db);
    printf("Postweighting ct: %i; ", list.len);
    free(list.ptr);
    */
    printf("Sampled.\n");
}

/* We've been given two nodes, Node Left and Node Right, and need to combine their label lists.
  This is used in the context of the source nodes being in the pre-graph and the joined node being in the dual.
  The adjacency lists of the parents is not transferred.
*/
static nodeinfo_s *join_nodes(int id, nodeinfo_s *nl, nodeinfo_s  *nr, DB *dupdb, DB *colordb){
    ptr_len_s su = sort_uniq((ptr_len_s) {nl->elmts,nl->len}, (ptr_len_s){nr->elmts, nr->len});

    int potential_dup = dup_exists(dupdb, (ptr_len_s){su.ptr, su.len}, id);
    if (potential_dup > 0 || potential_dup==id) { free(su.ptr); return NULL; }

    nodeinfo_s out_base = (nodeinfo_s){.id=id, .size=sizeof(nodeinfo_s)};
    nodeinfo_s *out = malloc(out_base.size);
    memcpy(out, &out_base, sizeof(nodeinfo_s));
    out->weight = (nl->weight? nl->weight : 1) *(nr->weight? nr->weight : 1);

    out->len = su.len;
    memcpy(out->elmts, su.ptr, sizeof(int)*su.len);
    free(su.ptr);

    return out;
}



// Finally, the main graph duality loop.

/* Start at declare_dual to shift from the $(M-1)$-subgraph to the $M$-subgraph.
For each node (ego), be they parents or kids: get its adjacent surroundings,
and generate a new node from the edge linking ego and adj for each adj in surroundings.
TO DO: iff ego> adj? Otherwise we're redundant?

When generating a node in the $M$-graph, record it in the adjacency list of the two nodes in the $(M-1)$-graph.
The adjacents element of a nodeinfo_s in the old database lists pairs of
(nodes adjacent to this one in the old db, new edge ID in the new db).

We now have all the nodes generated. In fact, we have too many, because of duplicates.
On a second pass, go over all adjacencies in the $(M-1)$-graph.
If two $M$-nodes are in the same adjacency list and have identical labels in the $M$-graph, then replace the higher ID number with the lower, then uniq-ify the list.
This pass is a lot of database calls, but deduplication is essential.
I'm not sure if we can do it sooner, because we need all the nodes registered.
It would be difficult to do it later, because after some edges are in place, redrawing is possible but complicated.

On the third pass, for each pair in each adjacency list, add_edge_1d(this pair of nodes).
This is the part where we may have 5000*4999 edges. We put our faith in BDB, and at least we've deduplicated.

That's it: you now have all new nodes stored in the new_db and annotated in
the nodeinfo_s for both old nodes, and all the new edges formed from turning
the ego node into an edge.
*/

nodeinfo_s * add_adjacency_1d(nodeinfo_s *ni, int newedge, DB *infodb);

void join_new_neighbor(nodeinfo_s *prior_vertex, nodeinfo_s *new_node, pcdb_s new_db){
    for (int j=0; j< prior_vertex->adj_len; j++){
        if (new_node->id == prior_vertex->adjacents[j]) continue;
        add_edge_1d(new_db.pcdb, new_node->id, prior_vertex->adjacents[j], false);
    }
}

nodeinfo_s * add_adjacency(nodeinfo_s *ego, nodeinfo_s *adj, nodeinfo_s * new_node, DB *oldinfodb, pcdb_s new_db){
    join_new_neighbor(ego, new_node, new_db);
    join_new_neighbor(adj, new_node, new_db);

    nodeinfo_s *adjn = add_adjacency_1d(adj, new_node->id, oldinfodb); //both directions, but the algorithm only has to hang on to ego.
    free(adjn);
    return add_adjacency_1d(ego, new_node->id, oldinfodb);
}

#define Max(a, b) (a> b? a : b)

ptr_len_s get_subgraph_adjacent_nodes(pcdb_s old_db, nodeinfo_s subgraph){
    ptr_len_s adjacents = (ptr_len_s){NULL, 0};

    for (int i=0; i< subgraph.len; i++){
        ptr_len_s n1 = get_node_surroundings(old_db, subgraph.elmts[i]);
        if (n1.len == -1) {free(n1.ptr); continue;}
        ptr_len_s next_adjacents = sort_uniq(adjacents, n1);
        free(n1.ptr); free(adjacents.ptr);
        adjacents = next_adjacents;
    }
    return adjacents;
}

int get_subgraph_adjacent_ct(pcdb_s old_db, nodeinfo_s subgraph){
    ptr_len_s outp = get_subgraph_adjacent_nodes(old_db, subgraph);
    free(outp.ptr);
    return outp.len;
}

nodeinfo_s * get_new_node_from_old_pair(pcdb_s old_db, pcdb_s newdbs, nodeinfo_s *old_ego, nodeinfo_s *old_adj, int *next_node, int color_ct){
    //Pairs of nodes coming into this fn will have one edge between them, generated once. Both parents' adjacency tabs are modified en route.
    //Return only the ego's newly-built node. The others are stored, but this one needs to be ready for re-updating in the main loop.
    if (Max(get_subgraph_adjacent_ct(old_db, *old_adj), 1) * Max(get_subgraph_adjacent_ct(old_db, *old_ego), 1) > Max_future_nodes)
    {/*printf("Boom: %i x %i = %i\t",
        get_subgraph_adjacent_ct(old_db, *old_adj) , get_subgraph_adjacent_ct(old_db, *old_ego),
        (get_subgraph_adjacent_ct(old_db, *old_adj) * get_subgraph_adjacent_ct(old_db, *old_ego))
            );*/ return NULL;} //Exploding node.

    nodeinfo_s *outnode = join_nodes(*next_node, old_ego, old_adj, newdbs.dupdb, newdbs.colordb);

    if (!outnode || outnode->id !=*next_node) return NULL; //there was a dup; change nothing.  

    (*next_node) ++;
    db_set_info(newdbs.colordb, outnode);
	//we don't care about adjacency info in the last round.
    if (color_ct==outnode->len) { free(outnode); return NULL;}
    else {
        nodeinfo_s *out = add_adjacency(old_ego, old_adj, outnode, old_db.colordb, newdbs);
        free(outnode);
        return out;
    }
}

void declare_dual(pcdb_s old_db, pcdb_s new_db, pcdb_s reference_db, int color_ct){
    ptr_len_s old_all = db_get_all_keys_1d(old_db.pcdb); //main loop, intended over pre-graph edges, not old nodes.
    static int next_node = 1;

    for (int i=0; i< old_all.len; i++){
        if(!(i%2000)) {printf(".");fflush(NULL);}
        nodeinfo_s *old_ego_ni = db_get_info(old_db.colordb, old_all.ptr[i]);
        ptr_len_s old_adjacents = get_node_surroundings(old_db, old_all.ptr[i]);
        for (int j=0; j< old_adjacents.len; j++){
            if(old_adjacents.ptr[j] >= old_ego_ni->id) continue;

            nodeinfo_s *old_adj_ni = db_get_info(old_db.colordb, old_adjacents.ptr[j]);
            nodeinfo_s *rebuilt_ego = get_new_node_from_old_pair(old_db, new_db,
                                            old_ego_ni, old_adj_ni, &next_node, color_ct);
            if (rebuilt_ego==NULL) {free(old_adj_ni); continue;} //it was a dup or we're in the last round and don't care.
            if (rebuilt_ego!=old_ego_ni){
                free(old_ego_ni);
                old_ego_ni = rebuilt_ego;
            }
            free(old_adj_ni);
        }
        free(old_ego_ni);
        free(old_adjacents.ptr);
    }
    free(old_all.ptr);
}
 

//// Lua fns.

#define Thedb(lua_stack_posn) db_list[lua_tointeger(L, lua_stack_posn)]
#define L_int(lua_stack_posn)  lua_tointeger(L, lua_stack_posn)

int Lpreferential_draw(lua_State *L) {
    int out = preferential_draw(Thedb(1), lua_tonumber(L, 2));
    lua_pop(L, 2);
    lua_pushinteger(L, out);
    return 1;
}

int Lcut_graph(lua_State *L){cut_graph(&Thedb(1), lua_tonumber(L, 2)); lua_pop(L, 2); return 0;}

int Ldeclare_dual(lua_State *L) {
    declare_dual(Thedb(1), Thedb(2), Thedb(3), L_int(4));
    lua_pop(L, 4);
    return 1;
}

int Lget_dual_motifs(lua_State *L){
    printf("\n%lli---%lli--%lli--\n", L_int(1), L_int(2), L_int(3));
    char outn[1000]; snprintf(outn, 1000, "%s/out.nodes.psv", get_outdir());
    char outg[1000]; snprintf(outg, 1000, "%s/out.graph.psv", get_outdir());
    get_sets(Thedb(1), Thedb(2), L_int(3), outn, outg);
    lua_pop(L, 3);
    return 1;
}

int luaopen_dual_register(lua_State* L) {
    lua_register(L, "preferential_draw", Lpreferential_draw);
    lua_register(L, "cut_graph", Lcut_graph);
    lua_register(L, "declare_dual", Ldeclare_dual);
    lua_register(L, "get_motifs", Lget_dual_motifs);
    return 0;
}
