/*
 * This is sample code generated by rpcgen.
 * These are only templates and you can use them
 * as a guideline for developing your own functions.
 */

#include "operaciones.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

bool_t
send_op_log_1_svc(struct operation_log op_log, int *result,  struct svc_req *rqstp)
{
	bool_t retval;
	for (int i = 0; i < sizeof(op_log.operation); i++){
		op_log.operation[i] = toupper(op_log.operation[i]);
	}
	if (strcmp(op_log.operation, "PUBLISH") == 0 || strcmp(op_log.operation, "DELETE") == 0){
		printf("%s %s %s %s \n", op_log.username, op_log.operation, op_log.file_name, op_log.date_time);
	}
	else{
		printf("%s %s %s \n", op_log.username, op_log.operation, op_log.date_time);
	}
	*result = 1;
	retval = 1;
	return retval;
}

int
rpc_1_freeresult (SVCXPRT *transp, xdrproc_t xdr_result, caddr_t result)
{
	xdr_free (xdr_result, result);

	/*
	 * Insert additional freeing code here, if needed
	 */

	return 1;
}
