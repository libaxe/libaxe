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

#include <axe/base.h>
#include <axe/scope.h>
#include <axe/pool.h>
#include <axe/debug.h>
#include <axe/log.h>
#include <axe/error.h>

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "check.h"

struct ax_base_st
{
	ax_pool *pool;
	ax_scope *global_scope;
	ax_scope **stack;
	size_t stack_size;
	size_t stack_capacity;
	int err;
};

ax_base* ax_base_create()
{
	ax_pool *pool = NULL;
	ax_base* base = NULL;
	ax_scope *gscope = NULL;

	pool = ax_pool_create();
	if (!pool)
		goto pool_fail;

	base = ax_pool_alloc(pool, (sizeof(struct ax_base_st)));
	if (!base)
		goto fail;

	ax_base base_init = {
		.pool = pool,
		.global_scope = NULL,
		.stack = NULL,
		.stack_size = 0,
		.stack_capacity = 0,
		.err = AX_ERR_SUCCEED
	};
	memcpy(base, &base_init, sizeof base_init);

	gscope = (ax_scope *)__ax_scope_construct(base);
	if (!gscope)
		goto fail;
	base->global_scope = gscope;

	return base;
fail:
	ax_scope_destroy(gscope);
	ax_pool_free(base);
pool_fail:
	ax_pool_destroy(pool);
	return NULL;
}

void ax_base_destroy(ax_base* base)
{
	ax_scope_destroy(base->global_scope);
	ax_pool *pool = base->pool;
	free(base->stack);
	ax_pool_free(base);
	ax_pool_destroy(pool);
}


ax_pool *ax_base_pool(ax_base* base)
{
	CHECK_PARAM_NULL(base);
	return base->pool;
}

ax_scope *ax_base_global(ax_base *base)
{
	CHECK_PARAM_NULL(base);
	return base->global_scope;
}

ax_scope *ax_base_local(ax_base *base)
{
	CHECK_PARAM_NULL(base);
	return base->stack_size ? base->stack[base->stack_size - 1] : base->global_scope;
}

int ax_base_enter(ax_base *base)
{
	CHECK_PARAM_NULL(base);

	if (base->stack_size == base->stack_capacity) {
		base->stack_capacity <<= 1;
		base->stack_capacity |= 1;
		base->stack = realloc(base->stack, base->stack_capacity * sizeof (*base->stack));
	}

	ax_scope *scope = ax_scope_create(base->global_scope).scope;
	if (!scope) {
		ax_base_set_errno(base, AX_ERR_NOMEM);
		return -1;
	}
	base->stack[base->stack_size] = scope;
	base->stack_size++;
	return base->stack_size;
}

void ax_base_leave(ax_base *base, int depth)
{
	CHECK_PARAM_NULL(base);
	if (depth < 0)
		return;
	CHECK_PARAM_VALIDITY(depth, depth <= base->stack_size);
	ax_assert(base->stack_size != 0,  "leave overmuch");
	if (depth == 0)
		depth = base->stack_size;
	int i;
	for (i = base->stack_size - 1; i >= depth - 1; i--)
		ax_scope_destroy(base->stack[i]);
	base->stack_size = i + 1;
}


void ax_base_set_errno(ax_base *base, int err)
{
	base->err = err;
}

int ax_base_errno(ax_base *base)
{
	return base->err;
}
