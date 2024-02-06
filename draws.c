#include <lua.h>
#include <lualib.h>
#include <apop.h>
#include <stdio.h>
#include <time.h>

//This is called at the head of go.lua via the lua-side rng_init.
//Note that rng_init(-1) seeds with the time.
void rng_init(int seed){ apop_opts.rng_seed = (seed >=0 ? seed : time(NULL)); }

double draw_from_uniform(){
    static apop_data  *d = NULL; if (!d) d= apop_data_alloc(1,1);
    static apop_model *m; if (!m) m = apop_model_set_parameters(apop_uniform, 0, 1);
    apop_model_draws(m, 1, d);
    return *d->matrix->data;
}

double draw_from_exponential(double lamb){
    static apop_data  *d = NULL; if (!d) d= apop_data_alloc(1,1);
    static apop_model *m = NULL; if (!m) m= apop_model_copy(apop_exponential);
    static apop_model *pvm = NULL;
        if (!pvm) {
            m->parameters = apop_data_alloc(1);
        }
    apop_data_set(m->parameters, 0, .val=lamb);
    apop_model_draws(m, 1, d);
    return *d->matrix->data;
}

typedef struct α_β {double α; double β;} α_β;

α_β update_beta_binomial(α_β prior_in, double N, double p){
    static apop_model *prior; if (!prior) prior = apop_model_set_parameters(apop_beta, prior_in.α, prior_in.β);
    apop_data_set(prior->parameters, .val=prior_in.α);
    apop_data_set(prior->parameters, 1, .val=prior_in.β);
    apop_data_show(prior->parameters);
    static apop_model *likelihood; if (!likelihood) likelihood = apop_model_set_parameters(apop_binomial, N, p);
    apop_data_set(likelihood->parameters, N);
    apop_data_set(likelihood->parameters, 1, .val=p);
    apop_model *updated = apop_update(.prior= prior, .likelihood=likelihood);
    apop_model_show(updated);
    α_β out = (α_β) {apop_data_get(updated->parameters, 0), apop_data_get(updated->parameters, 1)};
    apop_model_free(updated);
    return out;
}


//Lua hooks:
int Ldraw_exponential(lua_State *L) {
    double lamb = lua_tonumber(L, 1); lua_pop(L, 1);
    lua_pushnumber(L, draw_from_exponential(lamb));
    return 1;
}

int Ldraw_uniform(lua_State *L) {
    lua_pushnumber(L, draw_from_uniform());
    return 1;
}

int Lrng_init(lua_State *L) {
    rng_init(lua_tointeger(L, 1));
    lua_pop(L, 1);
    return 0;
}

int Lupdate_beta_binomial(lua_State *L) {
    //printf("%g, %g vs %g, %g\n", lua_tonumber(L, 1), lua_tonumber(L, 2), lua_tonumber(L, 3), lua_tonumber(L, 4));
    α_β out = update_beta_binomial((α_β){lua_tonumber(L, 1), lua_tonumber(L, 2)}, lua_tonumber(L, 3), lua_tonumber(L, 4));
    lua_pop(L, 4);
    //printf("posterior %g, %g\n", out.α, out.β);
    lua_pushnumber(L, out.α); lua_pushnumber(L, out.β);
    return 2;
}

