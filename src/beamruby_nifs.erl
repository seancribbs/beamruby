-module(beamruby_nifs).

-on_load(init/0).

-export([new/0, eval/2, require/2]).

-define(nif_stub, nif_stub_error(?LINE)).
nif_stub_error(Line) ->
    erlang:nif_error({nif_not_loaded,module,?MODULE,line,Line}).

-ifdef(TEST).
-include_lib("eunit/include/eunit.hrl").
-endif.

init() ->
    PrivDir = case code:priv_dir(?MODULE) of
                  {error, bad_name} ->
                      EbinDir = filename:dirname(code:which(?MODULE)),
                      AppPath = filename:dirname(EbinDir),
                      filename:join(AppPath, "priv");
                  Path ->
                      Path
              end,
    erlang:load_nif(filename:join(PrivDir, ?MODULE), 0).

new() ->
    ?nif_stub.

eval(_VM, _String) ->
    ?nif_stub.

require(_VM, _File) ->
    ?nif_stub.
%% ===================================================================
%% EUnit tests
%% ===================================================================
-ifdef(TEST).


-endif.
