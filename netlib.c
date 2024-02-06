#define _GNU_SOURCE
#include <stdio.h>
#include <lauxlib.h>

#include "netlib.h"
#include <string.h>
#include <stdlib.h>
#include <assert.h>
double draw_from_uniform();
int Ldraw_exponential(lua_State *L);
int Ldraw_uniform(lua_State *L);
int Lrng_init(lua_State *L);

char *outdir="out";
pcdb_s *db_list = NULL;

//// Open db et al

static void pcdb_s_prep(int n, char *outd){
    static int db_list_len = 0;
    if(db_list_len < n){
        db_list = realloc(db_list, (n+1)* sizeof(pcdb_s));
        db_list[n]=(pcdb_s){};
        db_list_len = n;
    }

    char dbnamp[1000]; snprintf(dbnamp, 1000, "%s/dbpc.%i", outd, n);
    char dbnamc[1000]; snprintf(dbnamc, 1000, "%s/dbcp.%i", outd, n);
    char dbnaml[1000]; snprintf(dbnaml, 1000, "%s/dbcl.%i", outd, n);
    char dbnamd[1000]; snprintf(dbnamd, 1000, "%s/dbdp.%i", outd, n);

    if (db_create(&(db_list[n].pcdb), NULL, 0)
     || db_create(&(db_list[n].cpdb), NULL, 0)
     || db_create(&(db_list[n].colordb), NULL, 0)
     || db_create(&(db_list[n].dupdb), NULL, 0)
     )
        {printf("DB create failed.\n"); exit(1);}

    if (db_list[n].pcdb->open(db_list[n].pcdb, NULL, dbnamp, "pc", DB_BTREE, DB_CREATE, 0) ||
        db_list[n].colordb->open(db_list[n].colordb, NULL, dbnaml, "color", DB_BTREE, DB_CREATE, 0) ||
        db_list[n].dupdb->open(db_list[n].dupdb, NULL, dbnamd, "dup", DB_BTREE, DB_CREATE, 0) ||
        db_list[n].cpdb->open(db_list[n].cpdb, NULL, dbnamc, "cp", DB_BTREE, DB_CREATE, 0))
       {printf("DB open failed on %s.\n", dbnamp); exit(1);}

    db_list[n].ba_outdegree = calloc(1, sizeof(int));
    db_list[n].ba_indegree =  calloc(1, sizeof(int));
    db_list[n].outsize = 0;
    db_list[n].ba_Σ_size = 0;
}

static void close_dbs(int n){
    db_list[n].pcdb->close(db_list[n].pcdb, 0);
    db_list[n].colordb->close(db_list[n].colordb, 0);
    db_list[n].cpdb->close(db_list[n].cpdb, 0);
    db_list[n].dupdb->close(db_list[n].dupdb, 0);
}


//degree distribution for preferential draws.

void update_degree(pcdb_s *db, int n, int c, bool add, bool rm_this_one){
    //For now, we only add.
    db->ba_Σ_size++;
    if (db->outsize < db->ba_Σ_size) {
        db->outsize = db->ba_Σ_size*2;
        db->ba_outdegree = realloc(db->ba_outdegree, sizeof(int)*(db->outsize));
        db->ba_indegree  = realloc(db->ba_indegree,  sizeof(int)*(db->outsize));
    }
    db->ba_outdegree[db->ba_Σ_size-1] = n;
    if (c!=-100) db->ba_indegree [db->ba_Σ_size-1] = c;
}

/////// Node info

//See the notes in the edge-addition section of dual.c on the size tricks
int db_set_info(DB *db, nodeinfo_s *val){
    return db->put(db, NULL, &(DBT){.data = &val->id, .size = sizeof(int)},
                             &(DBT){.data = val, .size = val->size}, 0);
}

nodeinfo_s *db_get_info(DB *db, int id){
    DBT k = (DBT){.data=&id, .size=sizeof(int)};
    DBT d = {.flags=DB_DBT_MALLOC};

    if (db->get(db, NULL, &k, &d, 0)!=DB_NOTFOUND)
       return d.data; 
    else {
        nodeinfo_s preout= (nodeinfo_s){.id=id, .size=sizeof(nodeinfo_s), .elmts[0]=id, .len=1};
        nodeinfo_s *out = malloc(sizeof(nodeinfo_s));
        memcpy(out, &preout, sizeof(nodeinfo_s));
        db_set_info(db, out);
        return out;
    }
}

nodeinfo_s * add_adjacency_1d(nodeinfo_s *ni, int newedge, DB *oldinfodb){
   //Caller has to free the now-obsolete prior node.

    int newsize = ni->size+ sizeof(int);
    nodeinfo_s *out = malloc(newsize);
    memcpy(out, ni, ni->size); //we'll have room for one more
    out->adj_len++;
    out->size=newsize;
    *(out->adjacents + out->adj_len-1) = newedge;
    db_set_info(oldinfodb, out);
    return out;
}


int dup_exists(DB *db, ptr_len_s elmts, int id){
    if (elmts.len<3) return false;
    char checkme[300];
    checkme[0]='\0';
    for (int i=0; i< elmts.len; i++) 
        sprintf(checkme+strlen(checkme), "|%i", elmts.ptr[i]);
    DBT k = {.data = checkme, .size = strlen(checkme)+1};
    int status = db->exists(db, NULL, &k, 0);
    if (status==DB_NOTFOUND || status==DB_KEYEMPTY) {
        db->put(db, NULL, &k, &(DBT){.data = &id, .size = sizeof(int)}, 0);
        return -1;
    } else {
        DBT d = {.data=NULL, .size=0, .flags=DB_DBT_MALLOC};
        db->get(db, NULL, &k, &d, 0);
        int out = *(int*)(d.data);
        free(d.data);
        return out;
    }
}

void test_dup_exists(DB *db){
    static int have_checked=0; have_checked=1;
    if (have_checked) return;

    int d[] = {-100, -103, -128};
    int id = -213;
    db->del(db, NULL, &(DBT){.data = &id, .size = sizeof(int)}, 0);

    ptr_len_s t1 = (ptr_len_s){.len=3, .ptr= d};
    assert(dup_exists(db, t1, id)==-1);
    assert(dup_exists(db, t1, id)==-213);

    db->del(db, NULL, &(DBT){.data = &id, .size = sizeof(int)}, 0);
}


// put/get/rm

ptr_len_s db_get_key(DB *db, int left){
    DBT k = {.data = &left, .size = sizeof(int)};
    DBT d = {.data=NULL, .size=0, .flags=DB_DBT_MALLOC};
    return (db->get(db, NULL, &k, &d, 0)==DB_NOTFOUND)
        ? (ptr_len_s){malloc(1), -1}
        : (ptr_len_s){d.data, d.size/sizeof(int)};
}

int db_get_len(DB *db, int i){ //Often all we need.
    ptr_len_s o = db_get_key(db, i);
    free(o.ptr);
    return o.len;
}

//textbook cut/paste:
int cmpfunc (const void * a, const void * b) { return ( *(int*)a - *(int*)b ); }

ptr_len_s sort_uniq(ptr_len_s L, ptr_len_s R){
    //Sometimes we have to do boring things.
    if(L.len+R.len==0) return (ptr_len_s){0,0};

    int *out = malloc(sizeof(int)*(L.len + R.len));
    memcpy(out, L.ptr, sizeof(int)*L.len);
    memcpy(out+L.len, R.ptr, sizeof(int)*R.len);
    qsort(out, L.len+R.len, sizeof(int), cmpfunc);

    int ok_len = 1;
    for (int i=1; i<L.len+R.len; i++)
        if (out[i] != out[ok_len-1])
            out[ok_len++] = out[i];
    return (ptr_len_s){.ptr=out, .len=ok_len};
}

ptr_len_s db_get_all_keys_1d(DB * db){
    DBC *cursor;
    db->cursor(db, NULL, &cursor, 0);
    DBT k={};
    DBT v={};
    int *ct = malloc(1);
int len=0;
int ret;
while ((ret = cursor->get(cursor, &k, &v, DB_NEXT)) == 0) {
    ct = realloc(ct, sizeof(int)* ++len);
    ct[len-1] = *(int*)k.data;
    //free(v.data);
}
return (ptr_len_s){.ptr=ct, .len=len};
}

ptr_len_s get_all_keys(pcdb_s dbs){
    ptr_len_s parents = db_get_all_keys_1d(dbs.pcdb);
    ptr_len_s kids = db_get_all_keys_1d(dbs.cpdb);
    ptr_len_s out = sort_uniq(parents, kids);
    free(parents.ptr);
    free(kids.ptr);
    return out;
}

ptr_len_s get_node_surroundings(pcdb_s dbs, int node){
    ptr_len_s parents = db_get_key(dbs.pcdb, node);
    ptr_len_s kids = db_get_key(dbs.cpdb, node);
    if (parents.len==-1) {free(parents.ptr); return kids;}
    if (kids.len==-1) {free(kids.ptr); return parents;}

    ptr_len_s out = sort_uniq(parents, kids);
    free(parents.ptr);
    free(kids.ptr);
    return out;
}

int db_put(DB *db, int key, ptr_len_s val){
    return db->put(db, NULL, &(DBT){.data = &key, .size = sizeof(int)},
                             &(DBT){.data = val.ptr, .size = sizeof(int)*val.len}, 0);
}

bool compare_ptr_len_s(ptr_len_s L, ptr_len_s R){
    if (L.len != R.len) goto nope;
    for (int i=0; i < L.len; i++)
        if(L.ptr[i] != R.ptr[i]) goto nope;
    return true;

nope:
        print_ptr_len_s(-2, L);
        print_ptr_len_s(-1, R);
        return false;
}

#define Free_return {free(val.ptr); return;}
void add_edge_1d(DB *db, int left, int right, bool init_info){
    ptr_len_s val = db_get_key(db, left);
    if (val.len < 0){ //new node
        int ptr[1] = {right};
        db_put(db, left, (ptr_len_s){ptr, 1});
    ptr_len_s fff = db_get_key(db, left);
    assert(compare_ptr_len_s(fff, (ptr_len_s){ptr, 1}));
    free(fff.ptr);
        Free_return
        //val = (ptr_len_s) {};
    }

    val.len += 1;
    val.ptr = realloc(val.ptr, sizeof(int)*val.len);
    val.ptr[val.len-1]=right;
    db_put(db, left, val);
    ptr_len_s fff = db_get_key(db, left);
    assert(compare_ptr_len_s(fff, val));
    free(fff.ptr);
    free(val.ptr);
}

void rm_edge_1d(DB *db, int left, int right){
    ptr_len_s val = db_get_key(db, left);
    if (val.len<0) Free_return //Left not found

    int i=0;
    for (; i< val.len; i++)
        if (val.ptr[i]==right) break;
    if (val.len == i) Free_return //Right isn't there to remove
    val.len --;

    memmove(val.ptr+i, val.ptr+i+1, sizeof(int)*(val.len-i));
    db_put(db, left, val);
    ptr_len_s fff = db_get_key(db, left);
    assert(compare_ptr_len_s(fff, val));
    free(fff.ptr);
    Free_return
}

// one step up to nodes/edges

void print_ptr_len_s(double mark, ptr_len_s p){ //for debugging
    printf("%g\t = %i|\t", mark, p.len);
    for (int i=0; i< p.len; i++){
        printf("%i, ", p.ptr[i]);
    }
    printf("\n");
}

void add_edge(pcdb_s *dbs, int parent, int child, bool init_info){
    update_degree(dbs, parent, child, true, false);
    ptr_len_s child_list = db_get_key(dbs->pcdb, child);
    if (child_list.len< 0) //newborn, needs to start with one point in its outdegree scale.
        update_degree(dbs, child, -100, true, false);
    //indegree scale doesn't need priming.
    free(child_list.ptr);

    assert(parent>0 && child>0);

    add_edge_1d(dbs->pcdb, parent, child, init_info);
    add_edge_1d(dbs->cpdb, child, parent, init_info);
}

void rm_edge(pcdb_s *dbs, int parent, int child){
    ptr_len_s kids = db_get_key(dbs->pcdb, parent);
    /*for (int i=0; i < kids.len; i++)
        update_degree(dbs, parent, false, false);
    update_degree(dbs, parent, false, true);
    */

    rm_edge_1d(dbs->pcdb, parent, child);
    rm_edge_1d(dbs->cpdb, child, parent);
    free(kids.ptr);
}

void rm_node(pcdb_s *dbs, int N){
    ptr_len_s kids = db_get_key(dbs->pcdb, N);
    for (int i=0; i < kids.len; i++)
        rm_edge_1d(dbs->cpdb, kids.ptr[i], N);

    ptr_len_s parents = db_get_key(dbs->cpdb, N);
    for (int i=0; i < parents.len; i++)
        rm_edge_1d(dbs->pcdb, parents.ptr[i], N);

    dbs->pcdb->del(dbs->pcdb, NULL, &(DBT){.data = &N, .size = sizeof(int)}, 0);
    dbs->pcdb->del(dbs->cpdb, NULL, &(DBT){.data = &N, .size = sizeof(int)}, 0);
    free(kids.ptr);
    free(parents.ptr);
}

bool db_found_in_node(DB *db, int key, int findme){
    bool out = false;
    ptr_len_s k = db_get_key(db, key);
    if (k.len >0) for (int i =0; i< k.len; i++) if (k.ptr[i]==findme) {
        out = true;
        break;
    }
    free(k.ptr);
    return out;
}


//// coloring

void print_nodeinfo(nodeinfo_s *ni){
    printf("Node %i (len %i):\t", ni->id, ni->len);
    for (int i=0; i< ni->len; i++) printf("%i\t", ni->elmts[i]);
    printf("\n");
    printf("%i adjacents:\t", ni->adj_len);
    for (int i=0; i< ni->adj_len; i++) printf("%i\t", *(ni->adjacents+i));
    printf("\n");
}


void test_canonicalization();

//// Lua fns.

#define Thedb(lua_stack_posn) db_list[lua_tointeger(L, lua_stack_posn)]
#define L_int(lua_stack_posn)  lua_tointeger(L, lua_stack_posn)

int Lopen_db(lua_State *L) {
    pcdb_s_prep(L_int(1), outdir);
    test_dup_exists((Thedb(1)).dupdb);
    lua_pop(L, 1);
    test_canonicalization();
    return 0;
}

int Ldb_close(lua_State *L){ close_dbs(L_int(1)); lua_pop(L, 1); return 0; }

int Ladd_edge(lua_State *L) {
    add_edge(&Thedb(1), L_int(2), L_int(3), true);
    lua_pop(L,3);
    return 0;
}

int Lrm_edge(lua_State *L) {
    rm_edge(&Thedb(1), L_int(2), L_int(3));
    lua_pop(L,3);
    return 0;
}

int Lrm_node(lua_State *L) {
    rm_node(&Thedb(1), L_int(2));
    lua_pop(L,2);
    return 0;
}

int Lget_len(lua_State *L) {
    pcdb_s dbs = Thedb(1);
    int out = db_get_len((lua_tostring(L, 3)[0]=='p' ? dbs.pcdb : dbs.cpdb), L_int(2));
    lua_pop(L,3);
    lua_pushinteger(L, out>-1 ? out: 0);
    return 1;
}

int Lfind(lua_State *L) {
    pcdb_s dbs = Thedb(1);
    bool out = db_found_in_node((lua_tostring(L, 4)[0]=='p' ? dbs.pcdb : dbs.cpdb), L_int(2), L_int(3));
    lua_pop(L,4);
    lua_pushboolean(L, out);
    return 1;
}

static void lua_tab_from_ptr_len_s(ptr_len_s in, lua_State *L){
    lua_createtable(L, (in.len>=0? in.len: 0), 0);
    for (int i=0; i< in.len; i++){
        lua_pushinteger(L, in.ptr[i]);
        lua_rawseti(L, -2, i+1);
    }
}

static int Lget_all_keys(lua_State *L){
    assert(L_int(1));
    pcdb_s dbs = Thedb(1);
    ptr_len_s got = get_all_keys(dbs);
    lua_pop(L, 1);
    lua_tab_from_ptr_len_s(got, L);
    return 1;
}

static int Lget_key(lua_State *L){
    assert(L_int(1));
    pcdb_s dbs = Thedb(1);
    ptr_len_s got = db_get_key((lua_tostring(L, 3)[0]=='p' ? dbs.pcdb : dbs.cpdb), L_int(2));
    lua_pop(L, 3);
    lua_tab_from_ptr_len_s(got, L);
    return 1;
}

void set_outdir(char const *c) { outdir = strdup(c); }
char* get_outdir() { return outdir; }

static int Lset_outdir(lua_State *L){
    set_outdir(lua_tostring(L, 1));
    return 1;
}

static int Lget_outdir(lua_State *L){
    lua_pushstring(L, outdir);
    return 1;
}
//from draws.c
int Lupdate_beta_binomial(lua_State *L);

void register_db_luas(lua_State *L){
    lua_register(L, "db_open", Lopen_db);
    lua_register(L, "db_close", Ldb_close);
    lua_register(L, "add_edge", Ladd_edge);
    lua_register(L, "rm_edge", Lrm_edge);
    lua_register(L, "db_rm_node", Lrm_node);
    lua_register(L, "get_key", Lget_key);
    lua_register(L, "get_all_keys", Lget_all_keys);
    lua_register(L, "db_find", Lfind);
    lua_register(L, "get_degree", Lget_len);
    lua_register(L, "set_outdir", Lset_outdir);
    lua_register(L, "get_outdir", Lget_outdir);
    lua_register(L, "draw_exponential", Ldraw_exponential);
    lua_register(L, "draw_uniform", Ldraw_uniform);
    lua_register(L, "rng_init", Lrng_init);
    lua_register(L, "update_beta_binomial", Lupdate_beta_binomial);
}

//from dual.c
int Lpreferential_draw(lua_State *L) ;
int Lcut_graph(lua_State *L);
int Ldeclare_dual(lua_State *L);
int Lget_dual_motifs(lua_State *L);

int luaopen_register(lua_State* L) {
    register_db_luas(L);
    luaopen_dual_register(L);
//    luaL_newlib(L, fns);
    return 0;
}
