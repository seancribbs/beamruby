#ifndef BEAMRUBY_CONV_H
#define BEAMRUBY_CONV_H

#include "mruby/compile.h"
#include "beamruby.h"

#define RUBY_TO_ERL(name) ERL_NIF_TERM name(ErlNifEnv* env, mrb_value obj, beamruby_handle *handle)

mrb_value beamruby_to_rb(ErlNifEnv* env, ERL_NIF_TERM term, beamruby_handle* handle);
ERL_NIF_TERM beamruby_to_erl(ErlNifEnv* env, mrb_value value, beamruby_handle* handle);

#endif
