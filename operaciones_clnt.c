/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include <memory.h> /* for memset */
#include "operaciones.h"

/* Default timeout can be changed using clnt_control() */
static struct timeval TIMEOUT = { 25, 0 };

enum clnt_stat 
send_op_log_1(struct operation_log op_log, int *clnt_res,  CLIENT *clnt)
{
	return (clnt_call(clnt, send_op_log,
		(xdrproc_t) xdr_operation_log, (caddr_t) &op_log,
		(xdrproc_t) xdr_int, (caddr_t) clnt_res,
		TIMEOUT));
}
