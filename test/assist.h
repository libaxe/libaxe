#ifndef UTIL_H_
#define UTIL_H_
#include <axe/algo.h>
#include <axe/seq.h>

static ax_bool seq_equal_array(ax_seq *seq, void *arr, size_t mem_size)
{
	ax_iter first = ax_box_begin(ax_r(seq, seq).box);
	ax_iter last = ax_box_end(ax_r(seq, seq).box);
	return ax_equal_to_arr(&first, &last, arr, mem_size);
}
#endif
