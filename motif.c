#include "netlib.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

//current cell is one-offset, because Lua history.
//int current_cell(int j, int k, int len){return len*(j-1)- j*(j-1)/2 + k -j;}
int current_cell(int j, int k, int len){return len*(j-1)- j*(j-1)/2 + k;}

//zero-offset edition
//int current_cell_zo(int j, int k, int len){return len*j- (j+1)*j/2 + k -j0-1;}
int current_cell_zo(int j, int k, int len){return len*j- (j+1)*j/2 + k -1;}

#define Cell(graph, i, j, len) graph[current_cell_zo(i, j, len)]

void print_graph(char *out, int len){
    printf("%s\n", out);
    for(int i=0; i< len; i++){
        for(int j=0; j< len; j++)
            if(i>=j) printf("∙");
            else {printf("%c", Cell(out, i, j, len));}
        printf("\n");
        }
}

void swap_elmts(char *L, char *R){char tmp=*L; *L=*R; *R=tmp;}

void swap_graphbits(char *out, int i, int j, int len){
    for (int col=j+1; col< len; col++)
        swap_elmts(&out[current_cell_zo(i, col, len)], &out[current_cell_zo(j, col, len)]);
    for (int row=0; row< i; row++)
        swap_elmts(&out[current_cell_zo(row, i, len)], &out[current_cell_zo(row, j, len)]);
}

bool should_swap(char const *subgraph, int i, int j, int len){
    char *sg2 = strdup(subgraph);
    swap_graphbits(sg2, i, j, len);
    bool out = strcmp(sg2, subgraph) > 0;
    free(sg2);
    return out;
}

bool kids_greater_than_j(char const *graph, int start, int j, int len){
    for (int k=start+1; k<len; k++)
        if (Cell(graph, start, k, len)=='1'){
           if (k<=j) return false;
           else return kids_greater_than_j(graph, k, j, len);
        }
    return true;
}

bool parents_less_than_i(char const *graph, int i, int start, int len){
    for (int p=0; p<start-1; p++)
        if (Cell(graph, p, start, len)=='1'){
           if (p>=i) return false;
           else return parents_less_than_i(graph, i, p, len);
        }
    return true;
}

bool can_swap(char const *graph, int i, int j, int len){
    return kids_greater_than_j(graph, i, j, len) && parents_less_than_i(graph, i, j, len);
}

void canonicaliz(char *out, int len, int ctr){
/* This could surely be rolled up with lineage_sort, below, but this is how it evolved and time is
   running short. Pull requests welcome.
   Also, 'canonicalize' seems to be reserved by some inner workings of GCC.

   Our canonicalization objective is an output string with the largest numeric value.
   Two node positions _should_ be swapped if doing so would increase the value of the output string.
   */
    assert(ctr < len * len/2); //I assert we can't get into swapping loops.

    for (int i=0; i< len; i++)
        for (int j=i+1; j< len; j++)
            if (can_swap(out, i, j, len) && should_swap(out, i, j, len)){
                  swap_graphbits(out, i, j, len);
                  return canonicaliz(out, len, ctr+1); //Yes, this is more force than is necessary.
              }
}

#include <assert.h>

void one_test(char *subgraph, char *should_be, int len){
    char *t=strdup(subgraph);
    canonicaliz(t, len, 0);
    //printf("%s should be %s\n", t, should_be);
    assert(!strcmp(t, should_be));
    free(t);
}

void test_canonicalization(){
    //0→3 1→2 1→3 2→6 2→4 3→4 3→5 ⇒ swap 0 and 1
    one_test("001000 11000 0101 110 00 0", "101000 00110 1000 101 00 0", 7);

    one_test("011 01 0", "101 00 1", 4); //0→1 and 0→3 and 1→3; Note that nobody owns 2.
    one_test("110 00 1", "110 01 0", 4); //0→1 and 0→2→3; swap 1 and 2;
    one_test("110 01 0", "110 01 0", 4); //0→1→3 and 0→2; no swap.
    one_test("010 11 0", "011 10 0", 4); //0→2 and 1→2 and 1→3; swap 0 and 1.
}

static void swapints(int*val, int j, int k){int t=val[k]; val[k]=val[j]; val[j]=t;}
static void swaplists(ptr_len_s *val, int j, int k){ptr_len_s t=val[k]; val[k]=val[j]; val[j]=t;}

static bool found_in(ptr_len_s L, int findme){
    for (int i=0; i< L.len; i++) if (L.ptr[i]==findme) return true;
    return false;
}

static bool is_connected(char *subgraph, int len){
    //every node n has a 1 in its column or in its row.
    for (int n=0; n<len; n++){
        bool hooked_in = false;
        if (n<len-1) //last node has no column
            for (int j=n+1; j<len; j++) //n's row
                if (Cell(subgraph, n, j, len)=='1')
                    {hooked_in = true; break;}

        for (int i=0; i<n; i++) //n's column
            if (Cell(subgraph, i, n, len)=='1')
                {hooked_in = true; break;}
        if (!hooked_in) {printf("Subgraph is: %s\n", subgraph); return false;}
    }
    return true;
}

static void lineage_sort(ptr_len_s *kids_of, ptr_len_s nodes, char * out, int ctr){
// If we find a back-link val[k]→val[j] where k>j, swap the two.
    if (ctr > nodes.len*(nodes.len-1)) {
        printf("Loop:\t"); print_ptr_len_s(0, nodes);
        out[0] = '\0';
        return;
    }

    int len=nodes.len;
    for (int j=0; j<len; j++){
        for (int k=j+1; k<len; k++)
          if (found_in(kids_of[j], nodes.ptr[k]))
               out[current_cell_zo(j, k, len)]='1';
          else {
              out[current_cell_zo(j, k, len)]='0';
              if (found_in(kids_of[k], nodes.ptr[j])){
                  swapints(nodes.ptr, k, j);
                  swaplists(kids_of, k, j);
                  return lineage_sort(kids_of, nodes, out, ctr+1);
                  }
              }
        out[current_cell_zo(j, len, len)] = ' ';
    }
    out[current_cell_zo(len-1,len-1,len)] = '\0';

    assert(is_connected(out, len));

    canonicaliz(out, len, 0);
}

void get_motif(ptr_len_s v, DB *pcdb, FILE *outgf, int ctr){//v is a zero-offset vector
    /*The profiler tells us that motif generation merits optimization.
      Get all the p→c links only once. This will often be unnecessary,
      but pays off when it is. */
    ptr_len_s kids_of[v.len];
    char out[v.len*v.len];
    for (int i=0; i<v.len; i++) kids_of[i] = db_get_key(pcdb, v.ptr[i]);
    lineage_sort(kids_of, v, out, 0);
    for (int i=0; i<v.len; i++) free(kids_of[i].ptr);

    fprintf(outgf, "%s-%i|%s\n", get_outdir(), ctr, out);
}

void get_sets(pcdb_s infodb, pcdb_s reference_db, int size, char *fnode_name, char *fgraph_name){
    FILE *outnf = fopen(fnode_name, "a");
    FILE *outgf = fopen(fgraph_name, "a");
    DBC *cursor;
    infodb.colordb->cursor(infodb.colordb, NULL, &cursor, 0);
    DBT k={}, v={};
    static int ctr;
    while (cursor->get(cursor, &k, &v, DB_NEXT) != DB_NOTFOUND) {
        if (!*(int*)k.data %1000) printf(".%i", *(int*)k.data);
        nodeinfo_s *ni = v.data;

        for (int i=0; i<ni->len; i++) fprintf(outnf, "%s-%i|%i\n", get_outdir(), ctr, ni->elmts[i]);

        get_motif((ptr_len_s){ni->elmts, ni->len}, reference_db.pcdb, outgf, ctr);
        //free(v.data);
        ctr++;
    }
    cursor->close(cursor);
}
