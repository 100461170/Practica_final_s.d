/*
 * Please do not edit this file.
 * It was generated using rpcgen.
 */

#include "operaciones.h"

bool_t
xdr_operation_log (XDR *xdrs, operation_log *objp)
{
	register int32_t *buf;

	int i;
	 if (!xdr_opaque (xdrs, objp->username, MAX_STR))
		 return FALSE;
	 if (!xdr_opaque (xdrs, objp->operation, MAX_STR))
		 return FALSE;
	 if (!xdr_opaque (xdrs, objp->date_time, MAX_STR))
		 return FALSE;
	 if (!xdr_opaque (xdrs, objp->file_name, MAX_STR))
		 return FALSE;
	return TRUE;
}