/*
 * Copyright (c) 2021 Li hsilin <lihsilyn@gmail.com>
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

#include <axe/buff.h>
#include <axe/any.h>
#include <axe/base.h>
#include <axe/scope.h>
#include <axe/pool.h>
#include <axe/error.h>
#include <axe/def.h>

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "check.h"
#define DEFAULT_MAX ((~(size_t)0) >> 1)

struct ax_buff_st
{
	ax_any _any;
	size_t used;
	size_t real;
	size_t min;
	size_t max;
	void *buf;
};

static ax_any *any_copy(const ax_any *any);
static ax_any *any_move(ax_any *any);

static void    one_free(ax_one *one);

static const ax_any_trait any_trait;

static void one_free(ax_one *one)
{
	if (!one)
		return;
	ax_scope_detach(one);
	ax_pool_free(((ax_buff *)one)->buf);
	ax_pool_free(one);
}

static ax_any *any_copy(const ax_any *any)
{
	CHECK_PARAM_NULL(any);

	const ax_one *src_one = ax_cr(any, any).one;
	const ax_buff *src_buff = (const ax_buff *)any;

	ax_base *base = ax_one_base(src_one);
	ax_pool *pool = ax_base_pool(base);

	ax_buff *dst_buff = NULL;
	void *buffer = NULL;

	dst_buff = ax_pool_alloc(pool, sizeof(ax_buff));
	if (!dst_buff) {
		ax_base_set_errno(base, AX_ERR_NOMEM);
		goto fail;
	}
	buffer = ax_pool_alloc(pool, src_buff->used);
	if (!buffer) {
		ax_base_set_errno(base, AX_ERR_NOMEM);
		goto fail;
	}
	memcpy(dst_buff, src_buff, sizeof(ax_buff));

	memcpy(buffer, src_buff->buf, src_buff->used);
	dst_buff->real = src_buff->used;
	dst_buff->buf = buffer;

	dst_buff->_any.env.scope.macro = NULL;
	dst_buff->_any.env.scope.micro = 0;
	ax_scope_attach(ax_base_local(base), ax_r(buff, dst_buff).one);
	return ax_r(buff, dst_buff).any;
fail:
	ax_pool_free(buffer);
	ax_pool_free(dst_buff);
	return NULL;
}

static ax_any *any_move(ax_any *any)
{
	CHECK_PARAM_NULL(any);

	const ax_one *src_one = ax_cr(any, any).one;
	ax_buff *src_buff = (ax_buff *) any, *dst_buff = NULL;
	void *buf = NULL;

	ax_base *base = ax_one_base(src_one);
	ax_pool *pool = ax_base_pool(base);

	dst_buff = ax_pool_alloc(pool, sizeof(ax_buff));
	if (!dst_buff) {
		ax_base_set_errno(base, AX_ERR_NOMEM);
		goto fail;
	}

	buf = ax_pool_alloc(pool, src_buff->min);
	if (!buf) {
		ax_base_set_errno(base, AX_ERR_NOMEM);
		goto fail;
	}

	memcpy(dst_buff, src_buff, sizeof(ax_buff));
	src_buff->used = 0;
	src_buff->real = src_buff->min;
	src_buff->buf = buf;
	
	dst_buff->_any.env.scope.micro = 0;
	dst_buff->_any.env.scope.macro = NULL;
	ax_scope_attach(ax_base_local(base), ax_r(buff,dst_buff).one);
	return ax_r(buff, dst_buff).any;
fail:
	ax_pool_free(dst_buff);
	ax_pool_free(buf);
	return NULL;

}

static ax_fail mem_resize(const ax_buff *buff, size_t require, size_t *alloc)
{
	ax_assert(buff->used <= buff->real
			&& buff->real >= buff->min
			&& buff->used < buff->max
			, "invalid msize");

	if (require > buff->max) {
		ax_base *base = ax_one_base(ax_cr(buff, buff).one);
		ax_base_set_errno(base, AX_ERR_FULL);
		return ax_true;
	}

	if (require > buff->real) {
		*alloc = buff->max >> 1 <= require ? buff->max : (require << 1) | 1;
		return ax_false;
	}
	if (buff->real >> 2 >= require) {
		size_t new_size = buff->real >> 1;
		*alloc = new_size < buff->min ? buff->min : new_size;
		return ax_false;
	}
	*alloc = buff->real;
	return ax_false;
}

static const ax_any_trait any_trait =
{
	.one = {
		.name = "one.buff",
		.free = one_free
	},
	.move = any_move,
	.copy = any_copy
};

ax_any *__ax_buff_construct(ax_base *base)
{
	CHECK_PARAM_NULL(base);

	ax_pool *pool = ax_base_pool(base);
	ax_buff *buff = NULL;

	buff = ax_pool_alloc(pool, sizeof(ax_buff));

	if (!buff)
		goto fail;

	ax_buff buff_init = {
		._any = {
			.tr = &any_trait,
			.env = {
				.base = base,
				.scope = { NULL },
			}
		},
		
		.used = 0,
		.real = 0,
		.min = 0,
		.max = DEFAULT_MAX,
		.buf = NULL
	};
	memcpy(buff, &buff_init, sizeof buff_init);
	return ax_r(buff, buff).any;
fail:
	ax_pool_free(buff);
	return NULL;
}

ax_buff_r ax_buff_create(ax_scope *scope)
{
	CHECK_PARAM_NULL(scope);

	ax_base *base = ax_one_base(ax_r(scope, scope).one);
	ax_any *any = __ax_buff_construct(base);
	ax_buff_r buff_r = { .any = any };
	if (!any)
		return buff_r;
	ax_scope_attach(scope, buff_r.one);
	return buff_r;
}


ax_fail ax_buff_set_max(ax_buff *buff, size_t max)
{
	CHECK_PARAM_NULL(buff);
	ax_assert(buff->min <= max, "min is greater then max");

	if (buff->real > max) {
		ax_base *base = ax_one_base(ax_r(buff, buff).one);
		ax_pool *pool = ax_base_pool(base);
		size_t size_copy = buff->used < max ? buff->used : max;

		void *new_buf = ax_pool_alloc(pool, max);
		if (!new_buf) {
			ax_base_set_errno(base, AX_ERR_NOMEM);
			return ax_true;
		}
		memcpy(new_buf, buff->buf, size_copy);
		ax_pool_free(buff->buf);
		buff->buf = new_buf;
		buff->real = max;
		buff->used = size_copy;
	}

	buff->max = max;
	return ax_false;
}

ax_fail ax_buff_adapt(ax_buff *buff, size_t size)
{
	CHECK_PARAM_NULL(buff);

	size_t size_realloc;
	if (mem_resize(buff, size, &size_realloc))
		return ax_true;

	if (size_realloc != buff->real) {
		ax_base *base = ax_one_base(ax_r(buff, buff).one);
		ax_pool *pool = ax_base_pool(base);
		void *buf = ax_pool_realloc(pool, buff->buf, size_realloc);
		if (!buf) {
			ax_base_set_errno(base, AX_ERR_NOMEM);
			return ax_true;
		}
		buff->buf = buf;
		buff->real = size_realloc;
	}

	buff->used = size;
	return ax_false;
}

ax_fail ax_buff_resize(ax_buff *buff, size_t size)
{
	CHECK_PARAM_NULL(buff);

	if (size <= buff->real) {
		buff->used = size;
		return ax_false;
	}

	return ax_buff_adapt(buff, size);
}

ax_fail ax_buff_alloc(ax_buff *buff, size_t size, void **obuf)
{
	CHECK_PARAM_NULL(buff);
	CHECK_PARAM_NULL(obuf);

	size_t size_alloc;
	if (mem_resize(buff, size, &size_alloc))
		return ax_true;

	if (size_alloc != buff->real) {
		ax_base *base = ax_one_base(ax_r(buff, buff).one);
		ax_pool *pool = ax_base_pool(base);
		void *buf = ax_pool_alloc(pool, size_alloc);
		if (!buf) {
			ax_base_set_errno(base, AX_ERR_NOMEM);
			return ax_true;
		}
		*obuf = buff->buf;
		buff->buf = buf;
		buff->real = size_alloc;
	}

	buff->used = size;
	return ax_false;
}

ax_fail ax_buff_shrink(ax_buff *buff)
{
	ax_base *base = ax_one_base(ax_r(buff, buff).one);
	ax_pool *pool = ax_base_pool(base);
	void *new_buf = NULL;
	new_buf = ax_pool_realloc(pool, buff->buf, buff->used);
	if (!new_buf)
		return ax_true;
	buff->buf = new_buf;
	buff->real = buff->used;
	if (buff->min > buff->used)
		buff->min = buff->used;

	return ax_false;
}

ax_fail ax_buff_reserve(ax_buff *buff, size_t size)
{
	ax_assert(size < buff->max, "size too large");

	ax_base *base = ax_one_base(ax_r(buff, buff).one);
	ax_pool *pool = ax_base_pool(base);
	
	void *new_buf = NULL;

	if (size <= buff->real) {
		new_buf = ax_pool_realloc(pool, buff->buf, size);
		if (!new_buf)
			goto fail;
		buff->min = size;
	} else {
		new_buf = ax_pool_alloc(pool, size);
		if (!new_buf)
			return ax_true;

		memcpy(new_buf, buff->buf, buff->used);
		free(buff->buf);
	}

	buff->buf = new_buf;
	buff->real = size;
	if (buff->used > size)
		buff->used = size;
	buff->min = size;
	return ax_false;
fail:
	ax_base_set_errno(base, AX_ERR_NOMEM);
	return ax_true;
}

size_t ax_buff_size(const ax_buff *buff, size_t *real)
{
	if (real)
		*real = buff->real;
	return buff->used;
}

size_t ax_buff_max(const ax_buff *buff)
{
	return buff->max;
}

size_t ax_buff_min(const ax_buff *buff)
{
	return buff->min;
}

void *ax_buff_ptr(ax_buff *buff)
{
	return buff->buf;
}

