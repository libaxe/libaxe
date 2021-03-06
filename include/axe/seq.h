/*
  *Copyright (c) 2020 Li hsilin <lihsilyn@gmail.com>
 *
  *Permission is hereby granted, free of charge, to any person obtaining a copy
  *of seq software and associated documentation files (the "Software"), to deal
  *in the Software without restriction, including without limitation the rights
  *to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  *copies of the Software, and to permit persons to whom the Software is
  *furnished to do so, subject to the following conditions:
 *
  *The above copyright notice and seq permission notice shall be included in
  *all copies or substantial portions of the Software.
 *
  *THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  *IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  *FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  *AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  *LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  *OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  *THE SOFTWARE.
 */

#ifndef AXE_SEQ_H_
#define AXE_SEQ_H_
#include "box.h"
#include "def.h"

#define AX_SEQ_NAME AX_BOX_NAME ".seq"

#ifndef AX_SEQ_DEFINED
#define AX_SEQ_DEFINED
typedef struct ax_seq_st ax_seq;
#endif

#ifndef AX_TRAIT_DEFINED
#define AX_TRAIT_DEFINED
typedef struct ax_seq_trait_st ax_seq_trait;
#endif

typedef ax_fail (*ax_seq_push_f)   (ax_seq *seq, const void *val);
typedef ax_fail (*ax_seq_pop_f)    (ax_seq *seq);
typedef void    (*ax_seq_invert_f) (ax_seq *seq);
typedef ax_fail (*ax_seq_trunc_f)  (ax_seq *seq, size_t size);
typedef ax_iter (*ax_seq_at_f)     (const ax_seq *seq, size_t index);
typedef ax_fail (*ax_seq_insert_f) (ax_seq *seq, ax_iter *iter, const void *val);
typedef void   *(*ax_seq_end_f)    (const ax_seq *seq);

typedef ax_seq *(ax_seq_construct_f)(ax_base *base, const ax_stuff_trait *tr);

struct ax_seq_trait_st
{
	const ax_box_trait box; /* Keep this first */
	const ax_seq_push_f   push;
	const ax_seq_pop_f    pop;
	const ax_seq_push_f   pushf;
	const ax_seq_pop_f    popf;
	const ax_seq_invert_f invert;
	const ax_seq_trunc_f  trunc;
	const ax_seq_at_f     at;
	const ax_seq_insert_f insert;
	const ax_seq_end_f   first;
	const ax_seq_end_f   last;
};

typedef struct ax_seq_env_st
{
	ax_one_env one; /* Keep this first */
	const ax_stuff_trait *const elem_tr;
} ax_seq_env;

struct ax_seq_st
{
	const ax_seq_trait *const tr; /* Keep this first */
	ax_seq_env env;
};

typedef union
{
	const ax_seq *seq;
	const ax_box *box;
	const ax_any *any;
	const ax_one *one;
} ax_seq_cr;

typedef union
{
	ax_seq *seq;
	ax_box *box;
	ax_any *any;
	ax_one *one;
	ax_seq_cr c;
} ax_seq_r;

inline static ax_fail ax_seq_push(ax_seq *seq, const void *val)
{
	ax_trait_require(seq, seq->tr->push);
	return seq->tr->push(seq, val);
}

inline static ax_fail ax_seq_pop(ax_seq *seq)
{
	ax_trait_require(seq, seq->tr->pop);
	return seq->tr->pop(seq);
}

inline static ax_fail ax_seq_pushf(ax_seq *seq, const void *val)
{
	ax_trait_optional(seq, seq->tr->pushf);
	return seq->tr->pushf(seq, val);
}

inline static ax_fail ax_seq_popf(ax_seq *seq)
{
	ax_trait_optional(seq, seq->tr->popf);
	return seq->tr->popf(seq);
}

inline static void ax_seq_invert(ax_seq *seq)
{
	ax_trait_require(seq, seq->tr->invert);
	seq->tr->invert(seq);
}

inline static ax_fail ax_seq_trunc(ax_seq *seq, size_t size)
{
	ax_trait_optional(seq, seq->tr->trunc);
	return seq->tr->trunc(seq, size);
}

inline static ax_iter ax_seq_at(ax_seq *seq, size_t index)
{
	ax_trait_optional(seq, seq->tr->at);
	return seq->tr->at(seq, index);
}

inline static ax_citer ax_seq_cat(const ax_seq *seq, size_t index)
{
	ax_trait_optional(seq, seq->tr->at);
	ax_iter it = seq->tr->at((ax_seq *)seq, index);
	void *p = &it;
	return *(ax_citer *)p;
}

static inline ax_fail ax_seq_insert(ax_seq *seq, ax_iter *it, const void *val)
{
	ax_trait_optional(seq, seq->tr->at);
	return seq->tr->insert(seq, it, val);
}

static inline void *ax_seq_first(ax_seq *seq)
{
	ax_trait_optional(seq, seq->tr->first);
	return seq->tr->first(seq);
}

static inline const void *ax_seq_cfirst(const ax_seq *seq)
{
	ax_trait_optional(seq, seq->tr->first);
	return seq->tr->first(seq);
}

static inline void *ax_seq_last(ax_seq *seq)
{
	ax_trait_optional(seq, seq->tr->last);
	return seq->tr->last(seq);
}

static inline const void *ax_seq_clast(const ax_seq *seq)
{
	ax_trait_optional(seq, seq->tr->last);
	return seq->tr->last(seq);
}

ax_seq *ax_seq_init(ax_scope *scope, ax_seq_construct_f *builder, const char *fmt, ...);
ax_seq *ax_seq_vinit(ax_scope *scope, ax_seq_construct_f *builder, const char *fmt, va_list varg); 
ax_fail ax_seq_vpushl(ax_seq *seq, const char *fmt, va_list varg);
ax_fail ax_seq_pushl(ax_seq *seq, const char *fmt, ...);

#if 0
ax_fail ax_seq_vmpop(ax_seq *seq, unsigned int count, va_list varg);
ax_fail ax_seq_mpop(ax_seq *seq, unsigned int count, ...);
#endif

#endif
