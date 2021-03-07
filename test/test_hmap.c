#include <axut.h>

#include <axe/hmap.h>
#include <axe.h>

#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


static void test_complex(axut_runner* r)
{
#define N 500
	ax_base* base = ax_base_create();
	ax_hmap_role hmap_r = ax_hmap_create(ax_base_local(base), ax_stuff_traits(AX_ST_I32),
			ax_stuff_traits(AX_ST_I32));
	for (int k = 0, v = 0; k < N; k++, v++) {
		ax_map_put(hmap_r.map, &k, &v);
	}

	int table[N] = {0};
	ax_iter it = ax_box_begin(hmap_r.box), end = ax_box_end(hmap_r.box);
	while (!ax_iter_equal(it, end)) {
		ax_pair *pair = ax_iter_get(it);
		int *k = (int*)ax_pair_key(pair);
		int *v = (int*)ax_pair_value(pair);
		axut_assert(r, *k == *v);
		table[*k] = 1;
		it = ax_iter_next(it);
	}

	for (int i = 0; i < N; i++) {
		ax_map_erase(hmap_r.map, &i);
	}
	axut_assert(r, ax_box_size(hmap_r.box) == 0);

	for (int i = 0; i < N; i++) {
		axut_assert(r, table[i] == 1);
	}
	ax_base_destroy(base);
}

static void test_foreach(axut_runner *r)
{
	ax_base* base = ax_base_create();
	ax_hmap_role hmap_r = ax_hmap_create(ax_base_local(base), ax_stuff_traits(AX_ST_S),
			ax_stuff_traits(AX_ST_I32));
	const int count = 100;
	for (int32_t i = 0; i < count; i++) {
		char key[4];
		sprintf(key, "%d", i);
		ax_map_put(hmap_r.map, key, &i);
	}

	int32_t check_table[count];
	for (int i = 0; i < count; i++) check_table[i] = -1;
	ax_foreach(ax_pair *, pair, hmap_r.box) {
		int key;
		sscanf((char*)ax_pair_key(pair), "%d", &key);
		axut_assert(r, key == *(uint32_t*)ax_pair_value(pair));
		check_table[key] = 0;
	}
	for (int i = 0; i < count; i++) 
		axut_assert(r, check_table[i] == 0);

	ax_base_destroy(base);
}

axut_suite *suite_for_hmap(ax_base *base)
{
	axut_suite *suite = axut_suite_create(ax_base_local(base), "hmap");
	axut_suite_add(suite, test_complex, 1);
	axut_suite_add(suite, test_foreach, 1);
	return suite;
}