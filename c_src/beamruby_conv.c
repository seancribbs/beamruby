#include <unistd.h>

#include "erl_nif.h"
#include "mruby.h"
#include "mruby/string.h"
#include "mruby/array.h"
#include "mruby/hash.h"
#include "mruby/variable.h"
#include "mruby/range.h"

#include "beamruby_conv.h"

mrb_value number_to_numeric(ErlNifEnv* env, ERL_NIF_TERM term, beamruby_handle *handle);
mrb_value atom_to_symbol(ErlNifEnv* env, ERL_NIF_TERM term, beamruby_handle *handle);
mrb_value binary_to_string(ErlNifEnv* env, ERL_NIF_TERM term, beamruby_handle *handle);
mrb_value list_to_array(ErlNifEnv* env, ERL_NIF_TERM term, beamruby_handle *handle);
mrb_value plist_to_hash(ErlNifEnv* env, ERL_NIF_TERM term, beamruby_handle *handle);
mrb_value tuple_to_rbtuple(ErlNifEnv* env, int size, ERL_NIF_TERM *term, beamruby_handle *handle);

void init_rb_tuple(mrb_state*);

extern ERL_NIF_TERM ATOM_STRUCT;
extern ERL_NIF_TERM ATOM_TRUE;
extern ERL_NIF_TERM ATOM_FALSE;
extern ERL_NIF_TERM ATOM_UNDEFINED;
extern ERL_NIF_TERM ATOM_RANGE;

mrb_value beamruby_to_rb(ErlNifEnv* env, ERL_NIF_TERM term, beamruby_handle *handle){
  mrb_state *mrb = handle->mrb;
  if(term == ATOM_TRUE){
    return mrb_true_value();
  }
  else if(term == ATOM_FALSE){
    return mrb_false_value();
  }
  else if(enif_is_number(env, term)){
    return number_to_numeric(env, term, handle);
  }
  else if(enif_is_atom(env, term)) {
    return atom_to_symbol(env, term, handle);
  }
  else if(enif_is_binary(env, term)) {
    return binary_to_string(env, term, handle);
  }
  else if(enif_is_list(env, term)) {
    return list_to_array(env, term, handle);
  }
  else if(enif_is_tuple(env, term)) {
    ERL_NIF_TERM *elements;
    int size;
    enif_get_tuple(env, term, (int*)&size, &elements);
    if(size == 2 && elements[0] == ATOM_STRUCT) { // mochijson style struct
      return plist_to_hash(env, elements[1], handle);
    }
    else if(size == 1 && enif_is_list(env, elements[0])) { // CouchDB/EEP8 style
      return plist_to_hash(env, elements[0], handle);
    }
    else if(size == 4 && elements[0] == ATOM_RANGE) { // "range" record from beamruby.hrl
      int excl = (elements[3] == ATOM_TRUE) ? 0 : 1;
      return mrb_range_new(mrb, 
                           beamruby_to_rb(env, elements[1], handle),
                           beamruby_to_rb(env, elements[2], handle),
                           excl);
    }
    /* TODO: add ability to bind Erlang tuples to Ruby struct types and auto-marshal */
    else {
      return tuple_to_rbtuple(env, size, elements, handle);
    }
  }
  else {
    return mrb_nil_value();
  }
}

ERL_NIF_TERM beamruby_to_erl(ErlNifEnv* env, mrb_value obj, beamruby_handle *handle) {
  mrb_state mrb = handle->mrb;
  switch(mrb_type(obj)){
  case MRB_TT_FALSE:
    return (obj.value.p) ? ATOM_FALSE : ATOM_UNDEFINED; break;
  case MRB_TT_TRUE:
    return ATOM_TRUE; break;
  case MRB_TT_FIXNUM:
    return fixnum_to_number(env, obj, handle); break;
  case MRB_TT_SYMBOL:
    return symbol_to_atom(env, obj, handle); break;
  case MRB_TT_FLOAT:
    return float_to_number(env, obj, handle); break;
  case MRB_TT_OBJECT:
    if(mrb_obj_is_instance_of(mrb, obj, handle->cTuple))
      return rbtuple_to_tuple(env, obj, handle);
    else
      return ATOM_UNDEFINED;
    break;
  case MRB_TT_ARRAY:
    return array_to_list(env, obj, handle); break;
  case MRB_TT_HASH:
    return hash_to_plist_struct(env, obj, handle); break;
  case MRB_TT_STRING:
    return string_to_binary(env, obj, handle); break;
  case MRB_TT_RANGE:
    return range_to_record(env, obj, handle); break;
  /* TODO: add ability to bind Erlang tuples to Ruby struct types and auto-marshal */
  /* case MRB_TT_STRUCT: */    
  default:
    return ATOM_UNDEFINED;
  }
}

mrb_value number_to_numeric(ErlNifEnv* env, ERL_NIF_TERM term, beamruby_handle *handle) {
  /* TODO */
  mrb_state mrb = handle->mrb;
  mrb_value numeric = mrb_nil_value();
  return numeric;
}

mrb_value atom_to_symbol(ErlNifEnv* env, ERL_NIF_TERM atom, beamruby_handle *handle) {
  mrb_state mrb = handle->mrb;
  unsigned len;
  char *atomstr;
  mrb_value symbol;
  if(enif_get_atom_length(env, term, (unsigned*)&len, ERL_NIF_LATIN1)) {
    atomstr = enif_alloc((size_t)len);
    if(enif_get_atom(env, term, atomstr, len, ERL_NIF_LATIN1)) {
      return mrb_intern(mrb, atomstr, len);
    }
  }
  return mrb_nil_value();
}

mrb_value binary_to_string(ErlNifEnv* env, ERL_NIF_TERM term, beamruby_handle *handle) {
  mrb_state *mrb = handle->mrb;
  ErlNifBinary binary;
  if(enif_inspect_binary(env, term, (ErlNifBinary*)&binary)) {
    return mrb_str_new(mrb, binary.data, binary.size);
  }
  else
    return mrb_nil_value();
}

mrb_value list_to_array(ErlNifEnv* env, ERL_NIF_TERM list, beamruby_handle *handle) {
  mrb_state mrb = handle->mrb;
  ERL_NIF_TERM element, tail;
  unsigned len, i;
  mrb_value array, obj;
  if(enif_get_list_length(env, term, (unsigned*)&len)) {
    /* TODO: This might break in an integer overflow condition, mruby
       should use a size_t! */
    array = mrb_ary_new_capa(mrb, (int)len);
    for(i = 0; i < len; i++) {
      if(enif_get_list_cell(env, list, (ERL_NIF_TERM*)&element, (ERL_NIF_TERM*)&tail))
        obj = beamruby_to_rb(env, element, mrb);
      else
        break;
      mrb_ary_push(mrb, array, obj);
      list = tail;
    }
    return array;
  }
  else
    return mrb_ary_new(mrb);
}

mrb_value plist_to_hash(ErlNifEnv* env, ERL_NIF_TERM plist, beamruby_handle *handle) {
  mrb_state *mrb = handle->mrb;
  ERL_NIF_TERM element, tail;
  ERL_NIF_TERM *pair;
  unsigned len, i, pair_size;
  mrb_value hash, rkey, rvalue;
  if(enif_get_list_length(env, term, (unsigned*)&len)){
    hash = mrb_hash_new_capa(mrb, (int)len);
    for(i = 0; i < len; i++){
      if(enif_get_list_cell(env, plist, (ERL_NIF_TERM*)&element, (ERL_NIF_TERM*)&tail)){
        if(enif_get_tuple(env, element, (int*)&pair_size, (ERL_NIF_TERM**)&pair)
           && pair_size == 2) {
          rkey = beamruby_to_rb(env, pair[0], mrb);
          rvalue = beamruby_to_rb(env, pair[1], mrb);
          mrb_hash_set(mrb, hash, rkey, rvalue);
        }
        plist = tail;
      }
      else
        break;
    }
    return hash;
  }
  else
    return mrb_hash_new(mrb);
}

mrb_value tuple_to_rbtuple(ErlNifEnv* env, int size, ERL_NIF_TERM *tuple, beamruby_handle *handle) {
  mrb_state *mrb = handle->mrb;
  ERL_NIF_TERM element, tail;
  mrb_value rtuple;
  mrb_value *relements;
  mrb_value init_args[1];
  int i;
  // Create a Tuple instance from the handle's class
  rtuple = mrb_instance_new(mrb, handle->cTuple);

  if(size > 0) {
    /* Can we use enif_alloc here? */
    relements = mrb_malloc(mrb, sizeof(mrb_value) * size);
    for(i = 0; i < size; i++){
      relements[i] = beamruby_to_rb(env, tuple[i], mrb);
    }
    init_args[0] = mrb_ary_new_from_values(mrb, len, relements);
    rtuple = mrb_obj_call_init(mrb, rtuple, 1, init_args);
  } else {
    mrb_obj_call_init(mrb, rtuple, 0, NULL);
  }
  return rtuple;
}
