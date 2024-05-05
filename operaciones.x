const MAX_STR = 256;

struct operation_log{
    opaque username[MAX_STR];
    opaque operation[MAX_STR];
    opaque date_time[MAX_STR];
    opaque file_name[MAX_STR];
};



program RPC {
    version RPCVER {
        int send_op_log(struct operation_log op_log) = 1;
    } = 1;
} = 99;