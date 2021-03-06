#include "assist.h"

#include "axe/iter.h"
#include "axe/vector.h"
#include "axe/algo.h"
#include "axe/base.h"

#include "axut.h"

#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void create(axut_runner *r)
{
	ax_base* base = ax_base_create();
	ax_vector_r vec_r;

	vec_r = ax_vector_create(ax_base_local(base), ax_stuff_traits(AX_ST_I32));
	axut_assert(r, vec_r.any != NULL);
	axut_assert(r, ax_box_size(vec_r.box) == 0);

	ax_seq *seq = ax_seq_init(ax_base_local(base), __ax_vector_construct, "i32x3", 1, 2, 3);
	int i = 1;
	ax_box_cforeach(ax_r(seq, seq).box, const int*, v) {
		axut_assert(r, *v == i++);
	}

	ax_base_destroy(base);
}

static void push(axut_runner *r)
{
	ax_base* base = ax_base_create();
	ax_vector_r vec_r = ax_vector_create(ax_base_local(base), ax_stuff_traits(AX_ST_I32));
	for (int i = 0; i < 20; i++) {
		ax_seq_push(vec_r.seq, &i);
	}

	for (int i = 0; i < 20; i++) {
		ax_iter it = ax_seq_at(vec_r.seq, i);
		axut_assert(r, *(int*)ax_iter_get(&it) == i);
	}

	for (int i = 0; i < 20; i++) {
		ax_seq_pop(vec_r.seq);
	}
	axut_assert(r, ax_box_size(vec_r.box) == 0);

	ax_base_destroy(base);
}

static void iter(axut_runner *r)
{
	ax_iter cur, last;
	int i;

	ax_base* base = ax_base_create();

	ax_vector_r vec_r = ax_vector_create(ax_base_local(base), ax_stuff_traits(AX_ST_I32));
	for (i = 0; i < 20; i++) {
		ax_seq_push(vec_r.seq, &i);
	}

	i = 0;
	ax_box_cforeach(vec_r.box, const int*, v) {
		axut_assert(r, *v == i++);
	}

	i = 0;
	cur = ax_box_begin(vec_r.box);
	last = ax_box_end(vec_r.box);
	while (!ax_iter_equal(&cur, &last)) {
		axut_assert(r, *(int32_t*)ax_iter_get(&cur) == i++);
		ax_iter_next(&cur);
	}

	ax_one_free(vec_r.one);
	ax_base_destroy(base);
}

static void riter(axut_runner *r)
{
	ax_iter cur, last;
	int i;

	ax_base* base = ax_base_create();
	ax_vector_r vec_r = ax_vector_create(ax_base_local(base), ax_stuff_traits(AX_ST_I32));
	for (i = 0; i < 20; i++) {
		ax_seq_push(vec_r.seq, &i);
	}

	i = 20 - 1;
	cur = ax_box_rbegin(vec_r.box);
	last = ax_box_rend(vec_r.box);
	while (!ax_iter_equal(&cur, &last)) {
		axut_assert(r, *(int32_t*)ax_iter_get(&cur) == i--);
		ax_iter_next(&cur);
	}

	i = 20 - 1;
	cur = ax_box_end(vec_r.box);
	last = ax_box_begin(vec_r.box);
	do {
		ax_iter_prev(&cur);
		axut_assert(r, *(int32_t*)ax_iter_get(&cur) == i--);
	} while (!ax_iter_equal(&cur, &last));

	i = 0;
	cur = ax_box_rend(vec_r.box);
	last = ax_box_rbegin(vec_r.box);
	do {
		ax_iter_prev(&cur);
		axut_assert(r, *(int32_t*)ax_iter_get(&cur) == i++);
	} while (!ax_iter_equal(&cur, &last));

	ax_one_free(vec_r.one);
	ax_base_destroy(base);
}


static void seq_insert(axut_runner *r)
{
	int ins;
	ax_base* base = ax_base_create();
	ax_vector_r vec_r = ax_vector_init(ax_base_local(base), "i32x2", 1, 2);

	ax_iter it = ax_box_begin(vec_r.box);

	ins = 3;
	ax_seq_insert(vec_r.seq, &it, &ins);
	int32_t table1[] = {3, 1, 2};
	axut_assert(r, seq_equal_array(vec_r.seq, table1, sizeof table1));

	ins = 4;
	ax_seq_insert(vec_r.seq, &it, &ins);
	int32_t table2[] = {3, 4, 1, 2};
	axut_assert(r, seq_equal_array(vec_r.seq, table2, sizeof table2));

	it = ax_box_end(vec_r.box);
	ins = 5;
	ax_seq_insert(vec_r.seq, &it, &ins);
	int32_t table3[] = {3, 4, 1, 2, 5};
	axut_assert(r, seq_equal_array(vec_r.seq, table3, sizeof table3));

	ax_base_destroy(base);
}

static void seq_insert_for_riter(axut_runner *r)
{
	int ins;
	ax_base* base = ax_base_create();
	ax_vector_r vec_r = ax_vector_init(ax_base_local(base), "i32x2", 2, 1);

	ax_iter it = ax_box_rbegin(vec_r.box);

	ins = 3;
	ax_seq_insert(vec_r.seq, &it, &ins);
	int32_t table1[] = {2, 1, 3};
	axut_assert(r, seq_equal_array(vec_r.seq, table1, sizeof table1));

	ins = 4;
	ax_seq_insert(vec_r.seq, &it, &ins);
	int32_t table2[] = {2, 1, 4, 3};
	axut_assert(r, seq_equal_array(vec_r.seq, table2, sizeof table2));

	it = ax_box_rend(vec_r.box);
	ins = 5;
	ax_seq_insert(vec_r.seq, &it, &ins);
	int32_t table3[] = {5, 2, 1, 4, 3};
	axut_assert(r, seq_equal_array(vec_r.seq, table3, sizeof table3));

	ax_base_destroy(base);
}

static void any_move(axut_runner *r)
{
	ax_base* base = ax_base_create();
	ax_vector_r role1 = ax_vector_init(ax_base_local(base), "i32x4", 1, 2, 3, 4);
	ax_vector_r role2 = { .any = ax_any_move(role1.any) };

	int32_t table1[] = {1, 2, 3, 4};
	axut_assert(r, ax_box_size(role1.box) == 0);
	axut_assert(r, seq_equal_array(role2.seq, table1, sizeof table1));

	ax_vector_r role3 = { .any = ax_any_move(role1.any) };
	axut_assert(r, ax_box_size(role3.box) == 0);

	ax_base_destroy(base);
}

static void any_copy(axut_runner *r)
{
	ax_base* base = ax_base_create();
	ax_vector_r role1 = ax_vector_init(ax_base_local(base), "i32x4", 1, 2, 3, 4);
	ax_vector_r role2 = { .any = ax_any_copy(role1.any) };

	int32_t table1[] = {1, 2, 3, 4};
	axut_assert(r, seq_equal_array(role1.seq, table1, sizeof table1));
	axut_assert(r, seq_equal_array(role2.seq, table1, sizeof table1));

	ax_box_clear(role1.box);

	ax_vector_r role3 = { .any = ax_any_copy(role1.any) };
	axut_assert(r, ax_box_size(role3.box) == 0);

	ax_base_destroy(base);
}

static void seq_trunc(axut_runner *r)
{
	ax_base* base = ax_base_create();

	ax_vector_r vec_r = ax_vector_init(ax_base_local(base), "i32x3", 1, 2, 3);

	int32_t table1[] = {1, 2, 3, 0, 0};
	ax_seq_trunc(vec_r.seq, 5);
	axut_assert(r, seq_equal_array(vec_r.seq, table1, sizeof table1));

	ax_seq_trunc(vec_r.seq, 0);
	axut_assert(r, seq_equal_array(vec_r.seq, NULL, 0));

	int32_t table3[] = {0, 0, 0, 0, 0};
	ax_seq_trunc(vec_r.seq, 5);
	axut_assert(r, seq_equal_array(vec_r.seq, table3, sizeof table3));

	ax_base_destroy(base);
}

static void seq_invert(axut_runner *r)
{
	ax_base* base = ax_base_create();
	ax_vector_r vec_r = ax_vector_init(ax_base_local(base), "i32x5", 1, 2, 3, 4, 5);

	int32_t table1[] = {5, 4, 3, 2, 1};
	ax_seq_invert(vec_r.seq);
	axut_assert(r, seq_equal_array(vec_r.seq, table1, sizeof table1));

	ax_box_clear(vec_r.box);
	axut_assert(r, ax_box_size(vec_r.box) == 0);

	ax_seq_invert(vec_r.seq);
	axut_assert(r, ax_box_size(vec_r.box) == 0);

	int32_t table3[] = {1};
	ax_seq_push(vec_r.seq, table3);
	ax_seq_invert(vec_r.seq);
	axut_assert(r, seq_equal_array(vec_r.seq, table3, sizeof table3));

	ax_base_destroy(base);
}

axut_suite *suite_for_vector(ax_base *base)
{
	axut_suite* suite = axut_suite_create(ax_base_local(base), "vector");

	axut_suite_add(suite, create, 0);
	axut_suite_add(suite, push, 0);
	axut_suite_add(suite, any_move, 0);
	axut_suite_add(suite, any_copy, 0);
	axut_suite_add(suite, iter, 0);
	axut_suite_add(suite, riter, 0);
	axut_suite_add(suite, seq_insert, 0);
	axut_suite_add(suite, seq_insert_for_riter, 0);
	axut_suite_add(suite, seq_trunc, 0);
	axut_suite_add(suite, seq_invert, 0);

	return suite;
}
