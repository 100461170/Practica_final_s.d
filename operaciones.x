const MAX = 256;

struct operation_log{
    opaque username[MAX];
    opaque operation[MAX];
    opaque date_time[MAX];
    opaque file_name[MAX];
};



program RPC {
    version RPCVER {
        int send_op_log(struct operation_log op_log) = 1;
    } = 1;
} = 99;