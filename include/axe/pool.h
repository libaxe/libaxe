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

#ifndef AXE_POOL_H_
#define AXE_POOL_H_
#include "debug.h"
#include <stddef.h>
#include <stdint.h>

#ifndef AX_BASE_DEFINED
#define AX_BASE_DEFINED
typedef struct ax_base_st ax_base;
#endif

#ifndef AX_POOL_DEFINED
#define AX_POOL_DEFINED
typedef struct ax_pool_st ax_pool;
#endif

typedef void *(*ax_mem_alloc_f)(size_t size);
typedef void (*ax_mem_free_f)(void *ptr);

void *ax_pool_alloc(ax_pool* pool, size_t size);

void *ax_pool_realloc(ax_pool* pool, void *ptr, size_t size);

void ax_pool_free(void* ptr);

ax_pool *ax_pool_create();

void ax_pool_destroy(ax_pool* pool);

void ax_pool_dump(ax_pool* pool);

#endif
