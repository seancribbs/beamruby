#include <sys/types.h>
#include "erl_nif.h"
#include "mruby.h"
#include "mruby/compile.h"
#include "mruby/proc.h"
#define NIF_FUNC(name) static ERL_NIF_TERM name(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])

static ErlNifResourceType* beamruby_nifs_RESOURCE = NULL;

static ERL_NIF_TERM ATOM_OK;
static ERL_NIF_TERM ATOM_ERROR;
static ERL_NIF_TERM ATOM_TRUE;
static ERL_NIF_TERM ATOM_RUBY_EXCEPTION;
static ERL_NIF_TERM ATOM_CODEGEN_ERROR;
static ERL_NIF_TERM ATOM_PARSE_ERROR;

typedef struct
{
  mrb_state *mrb;
  uint8_t debug;
} beamruby_nifs_handle;

// Prototypes
NIF_FUNC(beamruby_nifs_new);
NIF_FUNC(beamruby_nifs_eval);
NIF_FUNC(beamruby_nifs_debug);

static ErlNifFunc nif_funcs[] =
  {
    {"new", 0, beamruby_nifs_new},
    {"eval", 2, beamruby_nifs_eval},
    {"debug", 2, beamruby_nifs_debug}
    /* {"new", 0, beamruby_nifs_new}, */
    /* {"myfunction", 1, beamruby_nifs_myfunction} */
  };

NIF_FUNC(beamruby_nifs_new)
{
  beamruby_nifs_handle* handle = enif_alloc_resource(beamruby_nifs_RESOURCE,
                                                     sizeof(beamruby_nifs_handle));
  handle->mrb = mrb_open();
  handle->debug = 0;
  ERL_NIF_TERM result = enif_make_resource(env, handle);
  enif_release_resource(handle);
  return enif_make_tuple2(env, ATOM_OK, result);
}

NIF_FUNC(beamruby_nifs_eval)
{
  beamruby_nifs_handle* handle;
  struct mrb_parser_state *p;
  mrb_state *mrb;
  char code[4096];
  int n = -1;

  if (enif_get_resource(env, argv[0], beamruby_nifs_RESOURCE, (void**)&handle) &&
      enif_get_string(env, argv[1], (char*)&code, sizeof(code), ERL_NIF_LATIN1))
    {
      mrb = handle->mrb;

      // Parse
      p = mrb_parse_string(mrb, (char*)&code);
      if(!p || !p->tree || p->nerr) {
        return enif_make_tuple2(env, ATOM_ERROR, ATOM_PARSE_ERROR);
      }

      // Codegen
      n = mrb_generate_code(mrb, p->tree);
      mrb_pool_close(p->pool);

      if(n >= 0)
        {
          mrb_run(mrb, mrb_proc_new(mrb, mrb->irep[n]), mrb_top_self(mrb));
          if(mrb->exc){
            if(handle->debug)
              mrb_p(mrb, mrb_obj_value(mrb->exc));
            mrb->exc = NULL;
            return enif_make_tuple2(env, ATOM_ERROR, ATOM_RUBY_EXCEPTION);
          }
          else
            {
              return ATOM_OK;
            }
        }
      else
        {
          return enif_make_tuple2(env, ATOM_ERROR, ATOM_CODEGEN_ERROR);
        }
    }
  else
    {
      return enif_make_badarg(env);
    }
}

NIF_FUNC(beamruby_nifs_debug)
{
  beamruby_nifs_handle* handle;
  if(enif_get_resource(env, argv[0], beamruby_nifs_RESOURCE, (void**)&handle) &&
     enif_is_atom(env, argv[1]))
    {
      if(argv[1] == ATOM_TRUE) {
        handle->debug = 1;
      }
      else {
        handle->debug = 0;
      }
      return ATOM_OK;
    }
  else
    {
      return enif_make_badarg(env);
    }
}

static void beamruby_nifs_resource_cleanup(ErlNifEnv* env, void* arg)
{
  beamruby_nifs_handle* handle = (beamruby_nifs_handle*)arg;
  mrb_close(handle->mrb);
}

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
  return 0;
}

ERL_NIF_INIT(beamruby_nifs, nif_funcs, &on_load, NULL, NULL, NULL);
