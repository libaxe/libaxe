/*
 * Copyright (c) 2020 Li hsilin <lihsilyn@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <axe/stack.h>
#include <axe/vector.h>
#include <axe/tube.h>
#include <axe/base.h>
#include <axe/pool.h>
#include <axe/scope.h>
#include <axe/any.h>
#include <axe/error.h>
#include <axe/mem.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "check.h"

#undef free

#define MIN_SIZE

struct ax_stack_st
{
	ax_tube tube;
	ax_vector_r vector;
};

static ax_fail tube_push(ax_tube *tube, const void *val);
static void    tube_pop(ax_tube *tube);
static size_t  tube_size(const ax_tube *tube);
static void   *tube_prime(const ax_tube *tube);
static ax_any *any_copy(const ax_any *any);
static ax_any *any_move(ax_any *any);
static void    one_free(ax_one *one);

const ax_tube_trait ax_stack_tr =
{
		.any = {
			.one = {
				.name = AX_STACK_NAME,
				.free = one_free,
			},
			.copy = any_copy,
			.move = any_move
		},
		.push = tube_push,
		.pop = tube_pop,
		.size = tube_size,
		.prime = tube_prime
};


static ax_fail tube_push(ax_tube *tube, const void *val)
{
	CHECK_PARAM_NULL(tube);

	ax_stack_cr self_r = { .tube = tube };
	return ax_vector_tr.push(self_r.stack->vector.seq, val);
}

static void tube_pop(ax_tube *tube)
{
	CHECK_PARAM_NULL(tube);

	ax_stack_cr self_r = { .tube = tube };
	ax_vector_tr.pop(self_r.stack->vector.seq);
}

static size_t tube_size(const ax_tube *tube)
{
	CHECK_PARAM_NULL(tube);

	ax_stack_cr self_r = { .tube = tube };
	return ax_vector_tr.box.size(self_r.stack->vector.box);
}

static void *tube_prime(const ax_tube *tube)
{
	CHECK_PARAM_NULL(tube);

	ax_stack_cr self_r = { .tube = tube };
	return ax_vector_tr.last(self_r.stack->vector.seq);
}

static ax_any *any_copy(const ax_any *any)
{
	return NULL;
}

static ax_any *any_move(ax_any *any)
{
	return NULL;
}

static void one_free(ax_one *one)
{
	if (!one)
		return;

	ax_stack_r self_r = { .one = one };
	ax_scope_detach(one);
	ax_vector_tr.box.any.one.free(self_r.stack->vector.one);
	ax_pool_free(one);
}

ax_tube *__ax_stack_construct(ax_base *base, const ax_stuff_trait *elem_tr)
{
	CHECK_PARAM_NULL(base);
	CHECK_PARAM_NULL(elem_tr);

	ax_pool *pool = ax_base_pool(base);

	ax_tube *self= NULL;
	ax_seq *vector = __ax_vector_construct(base, elem_tr);
	if (!vector)
		goto fail;

	self = ax_pool_alloc(pool, sizeof(ax_stack));
	if (!self) {
		ax_base_set_errno(base, AX_ERR_NOMEM);
		goto fail;
	}

	ax_stack stack_init = {
		.tube = {
			.tr = &ax_stack_tr,
			.env = {
				.one = {
					.base = base,
					.scope = { NULL }
				},
				.elem_tr = elem_tr
			},
		},
		.vector = {
			.seq = vector
		}
	};

	memcpy(self, &stack_init, sizeof stack_init);
	return self;
fail:
	ax_one_free(ax_r(seq, vector).one);
	ax_pool_free(self);
	return NULL;
}

ax_stack_r ax_stack_create(ax_scope *scope, const ax_stuff_trait *elem_tr)
{
	CHECK_PARAM_NULL(scope);
	CHECK_PARAM_NULL(elem_tr);

	ax_base *base = ax_one_base(ax_r(scope, scope).one);
	ax_stack_r self_r = { .tube = __ax_stack_construct(base, elem_tr) };
	if (!self_r.one)
		return self_r;
	ax_scope_attach(scope, self_r.one);
	return self_r;
}

