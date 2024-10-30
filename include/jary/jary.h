/*
BSD 3-Clause License

Copyright (c) 2024. Muhammad Raznan. All Rights Reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef JARY_H
#define JARY_H

#include <stdbool.h>
#include <stdlib.h>
#define JARY_OK		  0
// generic error, which need to be updated later!
#define JARY_ERROR	  0x2
#define JARY_ERR_OOM	  0x4
#define JARY_ERR_COMPILE  0x10
#define JARY_ERR_EXEC	  0x11
#define JARY_ERR_SQLITE3  0x20
#define JARY_ERR_NOTEXIST 0x21
#define JARY_ERR_MISMATCH 0x22
#define JARY_INT_CRASH	  0x101
#define JARY_INT_FINAL	  0x102

struct jary;
struct jyOutput;
struct sqlite3;

int jary_open(struct jary **, struct sqlite3 *);
int jary_close(struct jary *);
int jary_modulepath(struct jary *, const char *path);
int jary_event(struct jary *, const char *name, unsigned int *event);

int jary_field_str(struct jary *,
		   unsigned int event,
		   const char  *field,
		   const char  *value);

int jary_field_long(struct jary *,
		    unsigned int event,
		    const char	*field,
		    long	 value);

int jary_rule_clbk(struct jary *jary,
		   const char  *name,
		   int (*callback)(void *, const struct jyOutput *),
		   void *data);

int jary_compile_file(struct jary *, const char *path, char **errmsg);
int jary_compile(struct jary *, size_t size, const char *source, char **errmsg);
int jary_execute(struct jary *);

void jary_output_len(const struct jyOutput *output, unsigned int *length);
int  jary_output_str(const struct jyOutput *output,
		     unsigned int	    index,
		     const char		  **str);
int  jary_output_long(const struct jyOutput *, unsigned int index, long *num);
int  jary_output_ulong(const struct jyOutput *,
		       unsigned int   index,
		       unsigned long *num);
int  jary_output_bool(const struct jyOutput *,
		      unsigned int index,
		      bool	  *boolean);

const char *jary_errmsg(struct jary *);
#endif
