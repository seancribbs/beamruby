#ifndef BEAMRUBY_H
#define BEAMRUBY_H

typedef struct
{
  mrb_state *mrb;
  struct RClass *cTuple;
} beamruby_handle;

#endif
