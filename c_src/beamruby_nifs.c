#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "erl_nif.h"
#include "erl_driver.h"

#include "mruby.h"
#include "mruby/compile.h"
#include "mruby/proc.h"
#include "mruby/string.h"
/* #include "beamruby_conv.h" */

#define NIF_FUNC(name) static ERL_NIF_TERM name(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])

static ErlNifResourceType* beamruby_nifs_RESOURCE = NULL;
static ERL_NIF_TERM ATOM_OK;
static ERL_NIF_TERM ATOM_ERROR;
static ERL_NIF_TERM ATOM_TRUE;
static ERL_NIF_TERM ATOM_RUBY_EXCEPTION;
static ERL_NIF_TERM ATOM_CODEGEN_ERROR;
static ERL_NIF_TERM ATOM_PARSE_ERROR;
static ERL_NIF_TERM ATOM_RHASH;

typedef struct
{
  mrb_state *mrb;
  /* uint8_t debug; */
} beamruby_nifs_handle;

// Prototypes
NIF_FUNC(beamruby_nifs_new);
NIF_FUNC(beamruby_nifs_eval);
NIF_FUNC(beamruby_nifs_require);

static void* beamruby_allocf(mrb_state* mrb, void* p, size_t size);
ERL_NIF_TERM beamruby_run(ErlNifEnv*, mrb_state*, struct mrb_parser_state*);

// NIFs table
static ErlNifFunc nif_funcs[] = {
  {"new",     0, beamruby_nifs_new},
  {"eval",    2, beamruby_nifs_eval},
  /* {"debug",   2, beamruby_nifs_debug}, */
  {"require", 2, beamruby_nifs_require}
};

// NIFs
NIF_FUNC(beamruby_nifs_new)
{
  beamruby_nifs_handle* handle = enif_alloc_resource(beamruby_nifs_RESOURCE,
                                                     sizeof(beamruby_nifs_handle));
  handle->mrb = mrb_open_allocf(beamruby_allocf);
  /* handle->debug = FALSE; */

  ERL_NIF_TERM result = enif_make_resource(env, handle);
  enif_release_resource(handle);
  return enif_make_tuple2(env, ATOM_OK, result);
}

NIF_FUNC(beamruby_nifs_eval)
{
  beamruby_nifs_handle* handle;
  struct mrb_parser_state *p;
  mrb_state *mrb;
  char *code;
  unsigned codelen;

  if (enif_get_resource(env, argv[0], beamruby_nifs_RESOURCE, (void**)&handle) &&
      enif_is_list(env, argv[1])) {
    enif_get_list_length(env, argv[1], (unsigned*)&codelen);
    if(!enif_get_string(env, argv[1], (char*)&code, codelen+1, ERL_NIF_LATIN1))
      return enif_make_badarg(env);

    mrb = handle->mrb;

    // Parse
    p = mrb_parse_string(mrb, (char*)&code);
    return beamruby_run(env, mrb, p);
  }
  else  {
    return enif_make_badarg(env);
  }
}

/* NIF_FUNC(beamruby_nifs_debug) */
/* { */
/*   beamruby_nifs_handle* handle; */
/*   if(enif_get_resource(env, argv[0], beamruby_nifs_RESOURCE, (void**)&handle) && */
/*      enif_is_atom(env, argv[1])) */
/*     { */
/*       if(argv[1] == ATOM_TRUE) { */
/*         handle->debug = TRUE; */
/*       } */
/*       else { */
/*         handle->debug = FALSE; */
/*       } */
/*       return ATOM_OK; */
/*     } */
/*   else */
/*     { */
/*       return enif_make_badarg(env); */
/*     } */
/* } */

NIF_FUNC(beamruby_nifs_require)
{
  beamruby_nifs_handle* handle;
  char filename[4096];
  if (enif_get_resource(env, argv[0], beamruby_nifs_RESOURCE, (void**)&handle) &&
      enif_get_string(env, argv[1], (char*)&filename, sizeof(filename), ERL_NIF_LATIN1)) {
    // Open file
    FILE *file = fopen(filename, "r");
    if(file != NULL) {
      // Parse file
      struct mrb_parser_state* p = mrb_parse_file(handle->mrb, file);
      return beamruby_run(env, handle->mrb, p);
    } else {
      return enif_make_tuple2(env, ATOM_ERROR, enif_make_atom(env, erl_errno_id(errno)));
    } // end file open
  } else {
    return enif_make_badarg(env);
  }
}

// resource cleanup
static void beamruby_nifs_resource_cleanup(ErlNifEnv* env, void* arg)
{
  beamruby_nifs_handle* handle = (beamruby_nifs_handle*)arg;
  mrb_close(handle->mrb);
}

// Internal functions
static void* beamruby_allocf(mrb_state *mrb, void *p, size_t size)
{
  if(size == 0) {
    enif_free(p);
    return NULL;
  }
  else {
    return enif_realloc(p, size);
  }
}

ERL_NIF_TERM beamruby_run(ErlNifEnv *env, mrb_state *mrb, struct mrb_parser_state *p) {
  int n = -1;
  if(!p || !p->tree || p->nerr) {
    return enif_make_tuple2(env, ATOM_ERROR, ATOM_PARSE_ERROR);
  }

  // Codegen
  n = mrb_generate_code(mrb, p->tree);
  mrb_pool_close(p->pool);

  if(n >= 0) {
    mrb_run(mrb, mrb_proc_new(mrb, mrb->irep[n]), mrb_top_self(mrb));
    if(mrb->exc){
      mrb_value exc = mrb_inspect(mrb, mrb_obj_value(mrb->exc));
      ERL_NIF_TERM exc_str = enif_make_string_len(env, RSTRING_PTR(exc), RSTRING_LEN(exc), ERL_NIF_LATIN1);
      mrb->exc = NULL;
      return enif_make_tuple2(env, ATOM_ERROR,
                              enif_make_tuple2(env, ATOM_RUBY_EXCEPTION, exc_str));
    }
    else {
      return ATOM_OK;
    }
  }
  else {
    return enif_make_tuple2(env, ATOM_ERROR, ATOM_CODEGEN_ERROR);
  }
}


// Init the NIFs
static int on_load(ErlNifEnv* env, void** priv_data, ERL_NIF_TERM load_info)
{
  ErlNifResourceFlags flags = ERL_NIF_RT_CREATE | ERL_NIF_RT_TAKEOVER;
  ErlNifResourceType* rt = enif_open_resource_type(env, NULL,
                                                   "beamruby_nifs_resource",
                                                   &beamruby_nifs_resource_cleanup,
                                                   flags, NULL);

  if (rt == NULL)
    return -1;

  beamruby_nifs_RESOURCE = rt;

  ATOM_OK = enif_make_atom(env, "ok");
  ATOM_ERROR = enif_make_atom(env, "error");
  ATOM_TRUE = enif_make_atom(env, "true");
  ATOM_RUBY_EXCEPTION = enif_make_atom(env, "ruby_exception");
  ATOM_PARSE_ERROR = enif_make_atom(env, "parse_error");
  ATOM_CODEGEN_ERROR = enif_make_atom(env, "codegen_error");
  ATOM_RHASH = enif_make_atom(env, "rhash");
  return 0;
}

ERL_NIF_INIT(beamruby_nifs, nif_funcs, &on_load, NULL, NULL, NULL);
