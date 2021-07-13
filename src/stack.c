#include "stack.h"
#include <stdlib.h>
#include <string.h>

stack *stack_new(void)
{
	stack *s = calloc(1, sizeof(*s));
	if (!s)
		goto fail_stack;
	s->data_cap = 256;
	s->indices_cap = 16;
	s->data = malloc(s->data_cap);
	if (!s->data)
		goto fail_stack_data;
	s->indices = malloc(s->indices_cap * sizeof(*s->indices));
	if (!s->indices)
		goto fail_stack_indices;

	return s;

fail_stack_indices:
	free(s->data);
fail_stack_data:
	free(s);
fail_stack:
	return NULL;
}

unsigned stack_add(stack *s, const char *str, size_t len)
{
	size_t new_data_cap = s->data_cap;
	while (s->data_len + len + 1 > new_data_cap) {
		new_data_cap = 3 * new_data_cap / 2;
	}

	if (new_data_cap != s->data_cap) {
		void *p = realloc(s->data, new_data_cap);
		if (!p)
			return 0;
		s->data = p;
		s->data_cap = new_data_cap;
	}

	size_t new_indices_cap = s->indices_cap;
	while (s->indices_len + 1 > new_indices_cap) {
		new_indices_cap = 3 * new_indices_cap / 2;
	}
	if (new_indices_cap != s->indices_cap) {
		void *p =
		    realloc(s->indices, new_indices_cap * sizeof(*s->indices));
		if (!p)
			return 0;
		s->indices = p;
		s->indices_cap = new_indices_cap;
	}
	s->indices_len++;
	s->indices[s->indices_len - 1] = s->data_len;
	memcpy(s->data + s->data_len, str, len);
	s->data_len += len;
	s->data[s->data_len] = '\0';
	s->data_len++;
	return 1;
}

char *stack_at(stack *s, size_t i) { return &s->data[s->indices[i]]; }

void stack_free(stack *s)
{
	free(s->data);
	free(s->indices);
	free(s);
}
