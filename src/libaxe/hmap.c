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

#include <axe/hmap.h>
#include <axe/map.h>
#include <axe/iter.h>
#include <axe/scope.h>
#include <axe/pool.h>
#include <axe/debug.h>
#include <axe/base.h>
#include <axe/error.h>
#include <axe/log.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "check.h"

#undef free

struct node_st
{
	struct node_st *next;
	ax_byte kvbuffer[];
};

struct bucket_st
{
	struct node_st *node_list;
	struct bucket_st *prev;
	struct bucket_st *next;
};

struct ax_hmap_st
{
	ax_map _map;
	size_t size;
	size_t buckets;
	size_t threshold;
	size_t reserved;
	struct bucket_st *bucket_list;
	struct bucket_st *bucket_tab;
};

static void    *map_put(ax_map *map, const void *key, const void *val);
static ax_fail  map_erase(ax_map *map, const void *key);
static void    *map_get(const ax_map *map, const void *key);
static ax_iter  map_at(const ax_map *map, const void *key);
static ax_bool  map_exist(const ax_map *map, const void *key);
static void    *map_chkey(ax_map *map, const void *key, const void *new_key);

static const void *map_it_key(const ax_citer *it);

static size_t   box_size(const ax_box *box);
static size_t   box_maxsize(const ax_box *box);
static ax_iter  box_begin(ax_box *box);
static ax_iter  box_end(ax_box *box);
static void     box_clear(ax_box *box);
static const ax_stuff_trait *box_elem_tr(const ax_box *box);

static void     any_dump(const ax_any *any, int ind);
static ax_any  *any_copy(const ax_any *any);
static ax_any  *any_move(ax_any *any);

static void     one_free(ax_one *one);

static void     citer_next(ax_citer *it);
static void    *iter_get(const ax_iter *it);
static ax_fail  iter_set(const ax_iter *it, const void *p);
static void     iter_erase(ax_iter *it);

static ax_fail rehash(ax_hmap *hmap, size_t new_size);
static struct node_st *make_node(ax_map *map, const void *key, const void *val);
static struct bucket_st *locate_bucket(const ax_hmap *hmap, const void *key);
static void bucket_push_node(ax_hmap *hmap, struct bucket_st *bucket, struct node_st *node);
static struct bucket_st *unlink_bucket(struct bucket_st *head, struct bucket_st *bucket);
static struct node_st **find_node(const ax_map *map, struct bucket_st *bucket, const void *key);
static void free_node(ax_map *map, struct node_st **pp_node);

static ax_fail rehash(ax_hmap *hmap, size_t new_size)
{
	ax_hmap_r hmap_r = { hmap };
	ax_base *base = ax_one_base(hmap_r.one);
	ax_pool *pool = ax_base_pool(base);

	struct bucket_st *new_tab = ax_pool_alloc(pool, (new_size * sizeof(struct bucket_st)));
	if (!new_tab) {
		ax_base_set_errno(base, AX_ERR_NOMEM);
		return ax_true;
	}
	for (size_t i = 0; i != new_size; i++)
		new_tab[i].node_list = NULL;

	struct bucket_st *bucket = hmap->bucket_list;
	hmap->bucket_list = NULL; //re-link
	for (;bucket; bucket = bucket->next) {
		for (struct node_st *currnode = bucket->node_list; currnode;) {
			struct bucket_st *new_bucket = new_tab
				+ hmap->_map.env.key_tr->hash(currnode->kvbuffer, hmap->_map.env.key_tr->size)
				% new_size;
			if (!new_bucket->node_list) {
				new_bucket->next = hmap->bucket_list;
				new_bucket->prev = NULL;
				if (hmap->bucket_list)
					hmap->bucket_list->prev = new_bucket;
				hmap->bucket_list = new_bucket;
			}

			struct node_st *old_head = new_bucket->node_list, * node = currnode;
			currnode = currnode->next;
			node->next = old_head;
			new_bucket->node_list = node;
		}
		bucket->node_list = NULL;
	}
	ax_pool_free(hmap->bucket_tab);
	hmap->buckets = new_size;
	hmap->bucket_tab = new_tab;
	return ax_false;
}

static struct node_st *make_node(ax_map *map, const void *key, const void *val)
{
	ax_hmap_r hmap_r = { .map = map };

	ax_base *base = ax_one_base(hmap_r.one);
	ax_pool *pool = ax_base_pool(base);

	size_t key_size = map->env.key_tr->size;
	size_t node_size = sizeof(struct node_st) + key_size + map->env.val_tr->size;

	struct node_st *node = ax_pool_alloc(pool, node_size);
	if (!node) {
		ax_base_set_errno(base, AX_ERR_NOKEY);
		return NULL;
	}
	map->env.key_tr->copy(pool, node->kvbuffer, key, map->env.key_tr->size);
	map->env.val_tr->copy(pool, node->kvbuffer + key_size, val, map->env.val_tr->size);
	return node;
}

static inline struct bucket_st *locate_bucket(const ax_hmap *hmap, const void *key)
{
	size_t index = hmap->_map.env.key_tr->hash(key, hmap->_map.env.key_tr->size) % hmap->buckets;
	return hmap->bucket_tab + index;
}

static void bucket_push_node(ax_hmap *hmap, struct bucket_st *bucket, struct node_st *node)
{
	if (!bucket->node_list) {
		if (hmap->bucket_list)
			hmap->bucket_list->prev = bucket;
		bucket->prev = NULL;
		bucket->next = hmap->bucket_list;
		hmap->bucket_list = bucket;
	}
	node->next = bucket->node_list;
	bucket->node_list = node;
}

static struct bucket_st *unlink_bucket(struct bucket_st *head, struct bucket_st *bucket)
{
	assert(head);
	assert(bucket);
	struct bucket_st *ret;
	ret = head == bucket ? bucket->next : head;

	if (bucket->prev)
		bucket->prev->next = bucket->next;
	if (bucket->next)
		bucket->next->prev = bucket->prev;
	return ret;
}

static struct node_st **find_node(const ax_map *map, struct bucket_st *bucket, const void *key)
{
	struct node_st **pp_node;
	for (pp_node = &bucket->node_list; *pp_node; pp_node = &((*pp_node)->next))
		if (map->env.key_tr->equal((*pp_node)->kvbuffer, key, map->env.key_tr->size))
			return pp_node;
	return NULL;
}

static void free_node(ax_map *map, struct node_st **pp_node)
{
	assert(pp_node);
	ax_byte *value_ptr = (*pp_node)->kvbuffer + map->env.key_tr->size;
	map->env.key_tr->free((*pp_node)->kvbuffer);
	map->env.val_tr->free(value_ptr);
	struct node_st *node_to_free = *pp_node;
	(*pp_node) = node_to_free->next;
	ax_pool_free(node_to_free);
}

static void citer_next(ax_citer *it)
{
	CHECK_PARAM_NULL(it);
	CHECK_PARAM_VALIDITY(it, it->owner && it->tr && it->point);

	const ax_hmap *hmap= it->owner;
	struct node_st *node = it->point;
	struct bucket_st *bucket = locate_bucket(hmap, node->kvbuffer);
	assert(bucket);
	node = node->next;
	if (!node) {
		bucket = bucket->next;
		node = bucket ? bucket->node_list : NULL;
	}
	it->point = node;
}

static void *iter_get(const ax_iter *it)
{
	CHECK_PARAM_NULL(it);
	CHECK_PARAM_VALIDITY(it, it->owner && it->tr && it->point);

	ax_hmap_cr hmap_r = { .hmap = it->owner };
	struct node_st *node = it->point;

	const ax_stuff_trait
		*key_tr = hmap_r.map->env.key_tr,
		*val_tr = hmap_r.map->env.val_tr;

	void *pval = node->kvbuffer + key_tr->size;

	void *val = (val_tr->link)
		? *(void**)pval
		: pval;

	return val;
}

static ax_fail iter_set(const ax_iter *it, const void *p)
{
	CHECK_PARAM_NULL(it);
	CHECK_PARAM_VALIDITY(it, it->owner && it->point);

	struct node_st *node = it->point;

	ax_hmap_r hmap_r = { it->owner };
	ax_hmap *hmap= (ax_hmap*)it->owner;
	assert(hmap->size != 0);

	ax_base *base = ax_one_base(hmap_r.one);
	ax_pool *pool = ax_base_pool(base);
	const ax_stuff_trait *val_tr = hmap_r.map->env.val_tr;

	val_tr->free(node->kvbuffer + hmap_r.map->env.key_tr->size);
	if (val_tr->copy(pool, node->kvbuffer + hmap_r.map->env.key_tr->size,
				p, hmap_r.map->env.val_tr->size)) {
		ax_base_set_errno(base, AX_ERR_NOMEM);
		return ax_true;
	}
	return ax_false;
}

static void iter_erase(ax_iter *it)
{
	CHECK_PARAM_NULL(it);
	CHECK_PARAM_NULL(it->point);

	ax_hmap_r hmap_r = { it->owner };
	struct node_st *node = it->point;
	void *pkey = node->kvbuffer;

	struct bucket_st *bucket = locate_bucket(hmap_r.hmap, pkey);
	struct node_st **findpp= find_node(hmap_r.map, bucket, pkey);
	ax_assert(findpp, "bad iterator");
	assert(it->point == *findpp);

	free_node(hmap_r.map, findpp);
	if (!bucket->node_list)
		hmap_r.hmap->bucket_list = unlink_bucket(hmap_r.hmap->bucket_list, bucket);
	hmap_r.hmap->size --;

	it->point = NULL; //TODO do not rehash
}

inline static void *node_val(const ax_map* map, struct node_st *node)
{
	const ax_stuff_trait *ktr = map->env.key_tr;
	const ax_stuff_trait *vtr = map->env.val_tr;
	return vtr->link
		? *(void **) (node->kvbuffer + ktr->size)
		: (node->kvbuffer + ktr->size);
}

inline static void *node_key(const ax_map* map, struct node_st *node)
{
	const ax_stuff_trait *ktr = map->env.key_tr;
	return ktr->link
		? *(void **) (node->kvbuffer)
		: (node->kvbuffer);
}

static void *map_put(ax_map *map, const void *key, const void *val)
{
	CHECK_PARAM_NULL(map);

	ax_hmap_r hmap_r = { .map = map };
	ax_base *base  = ax_one_base(hmap_r.one);
	ax_pool *pool = ax_base_pool(base);

	const ax_stuff_trait
		*ktr = map->env.key_tr,
		*vtr = map->env.val_tr;

	const void *pkey = ktr->link ? &key : key;
	const void *pval = vtr->link ? &val : val;

	struct bucket_st *bucket = locate_bucket(hmap_r.hmap, pkey);
	struct node_st **findpp = find_node(hmap_r.map, bucket, pkey);
	if (findpp) {
		ax_byte *value_ptr = (*findpp)->kvbuffer + ktr->size;
		vtr->free(value_ptr);
		if (vtr->copy(pool, value_ptr, pval, vtr->size)) {
			ax_base_set_errno(base, AX_ERR_NOMEM);
			return NULL;
		}
		return node_val(map, *findpp);
	}

	if (hmap_r.hmap->size == hmap_r.hmap->buckets * hmap_r.hmap->threshold) {
		if (hmap_r.hmap->buckets == ax_box_maxsize(ax_r(map, map).box)) {
			ax_base_set_errno(base, AX_ERR_FULL);
			return NULL;
		}
		size_t new_size = hmap_r.hmap->buckets << 1 | 1;
		if(rehash(hmap_r.hmap, new_size))
			return NULL;
		bucket = locate_bucket(hmap_r.hmap, pkey);//bucket is invalid
	}
	struct node_st *new_node = make_node(hmap_r.map, pkey, pval);
	if (!new_node)
		return NULL;
	bucket_push_node(hmap_r.hmap, bucket, new_node);
	hmap_r.hmap->size ++;
	return node_val(map, new_node);
}

static ax_fail map_erase (ax_map *map, const void *key)
{
	CHECK_PARAM_NULL(map);

	ax_hmap_r hmap_r = { .map = map };
	const void *pkey = map->env.key_tr->link ? &key : key;
	struct bucket_st *bucket = locate_bucket(hmap_r.hmap, pkey);
	struct node_st **findpp = find_node(hmap_r.map, bucket, pkey);
	ax_assert(findpp, "invalid iterator");

	free_node(map, findpp);
	if (!bucket->node_list)
		hmap_r.hmap->bucket_list = unlink_bucket(hmap_r.hmap->bucket_list, bucket);

	hmap_r.hmap->size --;

	if (hmap_r.hmap->size <= (hmap_r.hmap->buckets >> 2) * hmap_r.hmap->threshold)
		return (rehash(hmap_r.hmap, hmap_r.hmap->buckets >>= 1));

	return ax_false;
}

static void *map_get (const ax_map *map, const void *key)
{
	CHECK_PARAM_NULL(map);

	const ax_hmap_cr hmap_r = { .map = map };
	const void *pkey = map->env.key_tr->link ? &key : key;

	struct bucket_st *bucket = locate_bucket(hmap_r.hmap, pkey);
	struct node_st **findpp = find_node(hmap_r.map, bucket, pkey);

	return findpp ? node_val(hmap_r.map, *findpp) : NULL;
}


static ax_iter  map_at(const ax_map *map, const void *key)
{
	CHECK_PARAM_NULL(map);

	const ax_hmap_cr hmap_r = { .map = map };
	const void *pkey = map->env.key_tr->link ? &key : key;

	struct bucket_st *bucket = locate_bucket(hmap_r.hmap, pkey);
	struct node_st **findpp = find_node(hmap_r.map, bucket, pkey);
	if (!findpp) {
		return box_end((ax_box *)hmap_r.box);
	}
	return (ax_iter) {
		.owner = (void *)map,
		.tr = &ax_hmap_tr.box.iter,
		.point = *findpp
	};

}

static ax_bool map_exist (const ax_map *map, const void *key)
{
	CHECK_PARAM_NULL(map);

	ax_hmap_r hmap_r = { .map = (ax_map*)map };
	const void *pkey = map->env.key_tr->link ? &key : key;
	struct bucket_st *bucket = locate_bucket(hmap_r.hmap, pkey);
	struct node_st **findpp = find_node(hmap_r.map, bucket, pkey);
	if (findpp)
		return ax_true;
	return ax_false;
}

static void *map_chkey(ax_map *map, const void *key, const void *new_key)
{
	const ax_hmap_r hmap_r = { .map = map };
	ax_base *base  = ax_one_base(hmap_r.one);
	ax_pool *pool = ax_base_pool(base);

	const ax_stuff_trait *ktr = hmap_r.hmap->_map.env.key_tr;
	const void *pkey = ktr->link ? &key : key;

	struct bucket_st *bucket = locate_bucket(hmap_r.hmap, pkey);
	struct node_st **findpp = find_node(hmap_r.map, bucket, pkey);
	ax_assert(findpp, "key does not exists");

	const void *pnewkey = ktr->link ? &new_key: new_key;
	struct node_st *node = *findpp;
	if (ktr->copy(pool, node->kvbuffer, pnewkey, ktr->size)) {
		ax_base_set_errno(base, AX_ERR_NOMEM);
	}

	(*findpp) = node->next;

	struct bucket_st *new_bucket = locate_bucket(hmap_r.hmap, pnewkey);
	struct node_st **destpp = find_node(hmap_r.map, new_bucket, pnewkey);
	if (destpp)
		free_node(hmap_r.map, destpp);

	bucket_push_node(hmap_r.hmap, new_bucket, node);
	return node_key(hmap_r.map, node);
}

static const void *map_it_key(const ax_citer *it)
{
	CHECK_PARAM_NULL(it);
	CHECK_PARAM_VALIDITY(it, it->owner && it->tr && it->point);
	CHECK_ITER_TYPE(it, AX_HMAP_NAME);

	ax_hmap_cr hmap_r = { .hmap = it->owner };
	struct node_st *node = it->point;

	return (hmap_r.map->env.key_tr->link)
		? *(void**)node->kvbuffer
		: node->kvbuffer;
}

static void one_free(ax_one *one)
{
	if (!one)
		return;

	ax_hmap_r hmap_r= { .one = one };
	ax_scope_detach(hmap_r.one);
	box_clear(hmap_r.box);
	ax_pool_free(hmap_r.hmap->bucket_tab);
	ax_pool_free(hmap_r.hmap);
}

static void any_dump(const ax_any *any, int ind)
{
	CHECK_PARAM_NULL(any);

	ax_pinfo("have not implemented");
}

static ax_any *any_copy(const ax_any *any)
{
	CHECK_PARAM_NULL(any);

	ax_hmap_r src_r = { .any = (ax_any *)any };
	ax_base *base = ax_one_base(src_r.one);
	const ax_stuff_trait *ktr = src_r.map->env.key_tr;
	const ax_stuff_trait *vtr = src_r.map->env.val_tr;
	ax_hmap_r dst_r = { .map = __ax_hmap_construct(base, ktr, vtr)};
	ax_map_cforeach(src_r.map, const void *, key, const void *, val) {
		if (!ax_map_put(dst_r.map, key, val)) {
			ax_one_free(dst_r.one);
			return NULL;
		}
	}

	dst_r.hmap->_map.env.one.scope.macro = NULL;
	dst_r.hmap->_map.env.one.scope.micro = 0;
	ax_scope_attach(ax_base_local(base), dst_r.one);

	return dst_r.any;
}

static ax_any *any_move(ax_any *any)
{
	CHECK_PARAM_NULL(any);

	ax_hmap_r src_r = { .any = any };
	ax_base *base = ax_one_base(src_r.one);
	ax_pool *pool = ax_base_pool(base);

	ax_hmap *dst = ax_pool_alloc(pool, sizeof(ax_hmap));
	memcpy(dst, src_r.hmap, sizeof(ax_hmap));
	src_r.hmap->buckets = 0;
	src_r.hmap->size = 0;

	dst->_map.env.one.scope.macro = NULL;
	dst->_map.env.one.scope.micro = 0;
	ax_scope_attach(ax_base_local(base), ax_r(hmap, dst).one);

	return (ax_any *) dst;
}

static size_t box_size(const ax_box *box)
{
	CHECK_PARAM_NULL(box);

	ax_hmap_r hmap_r = { .box = (ax_box*)box };
	return hmap_r.hmap->size;
}

static size_t box_maxsize(const ax_box *box)
{
	CHECK_PARAM_NULL(box);

	return (~(size_t)0) >> 1;
}

static ax_iter box_begin(ax_box *box)
{
	CHECK_PARAM_NULL(box);

	ax_hmap_r hmap_r = { .box = box };

	ax_iter it = {
		.owner = (void *)box,
		.tr = &ax_hmap_tr.box.iter,
		.point = hmap_r.hmap->bucket_list ? hmap_r.hmap->bucket_list->node_list : NULL
	};
	return it;
}

static ax_iter box_end(ax_box *box)
{
	CHECK_PARAM_NULL(box);

	ax_iter it = {
		.owner = box,
		.tr = &ax_hmap_tr.box.iter,
		.point = NULL
	};
	return it;
}

static void box_clear(ax_box *box)
{
	CHECK_PARAM_NULL(box);

	ax_hmap_r hmap_r = { .box = (ax_box*)box };
	for (struct bucket_st *bucket = hmap_r.hmap->bucket_list; bucket; bucket = bucket->next)
		for (struct node_st **pp_node = &bucket->node_list; *pp_node;)
			free_node(hmap_r.map, pp_node);

	ax_pool *pool = ax_base_pool(ax_one_base(hmap_r.one));
	
	void *new_bucket_tab = ax_pool_realloc(pool, hmap_r.hmap->bucket_tab,
			sizeof(struct bucket_st));
	if (new_bucket_tab) {
		hmap_r.hmap->bucket_tab = new_bucket_tab;
	}

	hmap_r.hmap->bucket_tab->node_list = NULL;
	hmap_r.hmap->size = 0;
	hmap_r.hmap->bucket_list = NULL;
	hmap_r.hmap->buckets = 1;
}

static const ax_stuff_trait *box_elem_tr(const ax_box *box)
{
	ax_hmap_r hmap_r = { .box = (ax_box*)box };
	return hmap_r.map->env.val_tr;
}

const ax_map_trait ax_hmap_tr =
{
	.box = {
		.any = {
			.one = {
				.name  = AX_HMAP_NAME,
				.free  = one_free,
			},
			.dump = any_dump,
			.copy = any_copy,
			.move = any_move,
		},
		
		.iter = {
			.ctr = {
				.norm  = ax_true,
				.type  = AX_IT_FORW,
				.move = NULL,
				.prev = NULL,
				.next = citer_next,
				.less  = NULL,
				.dist  = NULL,
			},
			.get   = iter_get,
			.set   = iter_set,
			.erase = iter_erase,
		},
		.riter = { { NULL } },

		.size    = box_size,
		.maxsize = box_maxsize,
		.begin   = box_begin,
		.end     = box_end,
		.rbegin  = NULL,
		.rend    = NULL,
		.clear   = box_clear,
		.elem_tr = box_elem_tr
	},
	.put   = map_put,
	.get   = map_get,
	.at    = map_at,
	.erase = map_erase,
	.exist = map_exist,
	.chkey = map_chkey,
	.itkey = map_it_key
};

ax_map *__ax_hmap_construct(ax_base *base, const ax_stuff_trait *key_tr, const ax_stuff_trait *val_tr)
{
	CHECK_PARAM_NULL(base);

	CHECK_PARAM_NULL(key_tr);
	CHECK_PARAM_NULL(key_tr->equal);
	CHECK_PARAM_NULL(key_tr->hash);
	CHECK_PARAM_NULL(key_tr->copy);
	CHECK_PARAM_NULL(key_tr->free);

	CHECK_PARAM_NULL(val_tr);
	CHECK_PARAM_NULL(val_tr->copy);
	CHECK_PARAM_NULL(val_tr->free);


	ax_hmap *hmap = ax_pool_alloc(ax_base_pool(base), sizeof(ax_hmap));
	if (!hmap) {
		ax_base_set_errno(base, AX_ERR_NOKEY);
		return NULL;
	}
	
	ax_hmap hmap_init = {
		._map = {
			.tr = &ax_hmap_tr,
			.env = {
				.one = {
					.base = base,
					.scope = { NULL },
				},
				.key_tr = key_tr,
				.val_tr = val_tr,
			},
		},
		.buckets = 1,
		.size = 0,
		.threshold = 8,
		.bucket_tab = NULL,
		.bucket_list = NULL,
	};

	hmap_init.bucket_tab = ax_pool_alloc(ax_base_pool(base), sizeof(struct bucket_st) * hmap_init.buckets);
	if (!hmap_init.bucket_tab) {
		ax_base_set_errno(base, AX_ERR_NOMEM);
		return NULL;
	}
	hmap_init.bucket_tab->node_list = NULL;
	memcpy(hmap, &hmap_init, sizeof hmap_init);
	return (ax_map *) hmap;
}

ax_hmap_r ax_hmap_create(ax_scope *scope, const ax_stuff_trait *key_tr, const ax_stuff_trait *val_tr)
{
	CHECK_PARAM_NULL(scope);
	CHECK_PARAM_NULL(key_tr);
	CHECK_PARAM_NULL(val_tr);

	ax_base *base = ax_one_base(ax_r(scope, scope).one);
	ax_hmap_r hmap_r =  { .map = __ax_hmap_construct(base, key_tr, val_tr) };
	if (!hmap_r.one)
		return hmap_r;
	ax_scope_attach(scope, hmap_r.one);
	return hmap_r;
}

