#include <stdbool.h>
#include <db.h>

#define Key_list_marker -101
#define Max_colors 15

typedef struct nodeinfo_s {
    int id, len, size, adj_len, weight;
    int elmts[Max_colors];
    int adjacents[1]; //There's going to be trickery here; see db_get_info and db_put_info.
} nodeinfo_s;

typedef struct ptr_len_s {
    int *ptr; int len;
} ptr_len_s;

typedef struct pcdb_s {
    DB *pcdb, *cpdb, *colordb, *dupdb; 
    int *ba_outdegree, *ba_indegree;
    int outsize;
    int ba_Σ_size;
} pcdb_s;

extern pcdb_s *db_list;

void print_ptr_len_s(double mark, ptr_len_s p);
int db_get_len(DB *db, int left);
//void pcdb_s_prep(int n);
//void update_degree(pcdb_s *db, int n, bool add, bool rm_this_one);
int db_put(DB *db, int key, ptr_len_s val);
void add_edge(pcdb_s *dbs, int parent, int child, bool init_info);
void add_edge_1d(DB *db, int left, int right, bool init_info);
void rm_edge_1d(DB *db, int left, int right);
void rm_edge(pcdb_s *dbs, int parent, int child);
void rm_node(pcdb_s *dbs, int N);
void set_outdir(char const *in);
char* get_outdir();
bool db_found_in_node(DB *db, int key, int findme);
ptr_len_s db_get_key(DB *db, int left);
ptr_len_s db_get_all_keys_1d(DB * db);
ptr_len_s get_all_keys(pcdb_s db);
ptr_len_s get_node_surroundings(pcdb_s dbs, int node);
ptr_len_s sort_uniq(ptr_len_s left, ptr_len_s right); //by the way.

//nodeinfo
int db_set_info(DB *db, nodeinfo_s *val);
nodeinfo_s *db_get_info(DB *db, int id);
void print_nodeinfo(nodeinfo_s *ni);

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
void register_db_luas(lua_State *L);
int luaopen_dual_register(lua_State *L);
