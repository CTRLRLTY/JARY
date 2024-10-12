#ifndef JAYVM_EXEC_H
#define JAYVM_EXEC_H

#include "jary/types.h"

#include <stdint.h>

struct jy_asts;
struct jy_tkns;

int jry_sqlstr_crt_event(const char	     *name,
			 unsigned short	      colsz,
			 char *const	     *columns,
			 const enum jy_ktype *types,
			 char		    **sql);

int jry_sqlstr_ins_event(const char    *name,
			 unsigned short colsz,
			 char *const   *columns,
			 char	      **sql);

#endif // JAYVM_EXEC_H
