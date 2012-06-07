%% A record representing a Ruby "Range" object. 
-record(range, {min :: term(), %% Range#begin
                max :: term(), %% Range#end
                exclusive = true :: boolean()}).
