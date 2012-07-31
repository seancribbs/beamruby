#ifndef BEAMRUBY_H
#define BEAMRUBY_H

#include "mruby/compile.h"
#include "mruby.h"

typedef struct
{
  mrb_state *mrb;
  struct RClass *cTuple;
  mrbc_context* mrbc;
} beamruby_handle;

static ERL_NIF_TERM ATOM_OK;
static ERL_NIF_TERM ATOM_ERROR;
static ERL_NIF_TERM ATOM_TRUE;
static ERL_NIF_TERM ATOM_FALSE;
static ERL_NIF_TERM ATOM_UNDEFINED;
static ERL_NIF_TERM ATOM_RUBY_EXCEPTION;
static ERL_NIF_TERM ATOM_CODEGEN_ERROR;
static ERL_NIF_TERM ATOM_PARSE_ERROR;
static ERL_NIF_TERM ATOM_STRUCT;
static ERL_NIF_TERM ATOM_RANGE;

#endif
