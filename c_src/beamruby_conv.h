#ifndef BEAMRUBY_CONV_H
#define BEAMRUBY_CONV_H

#include "beamruby.h"

mrb_value beamruby_to_rb(ErlNifEnv* env, ERL_NIF_TERM term, beamruby_handle* handle);
ERL_NIF_TERM beamruby_to_erl(ErlNifEnv* env, mrb_value value, beamruby_handle* handle);

#endif
