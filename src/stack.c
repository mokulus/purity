#include "stack.h"
#include <stdlib.h>
#include <string.h>

void stack_add(stack *s, const char *str, size_t len) {
	size_t new_data_cap = s->data_cap;
	if (s->data_cap == 0) {
		new_data_cap = 256;
	}
	while (s->data_len + len + 1 > new_data_cap) {
		new_data_cap = 3 * new_data_cap / 2;
	}
	if (new_data_cap != s->data_cap) {
		s->data_cap = new_data_cap;
		s->data = realloc(s->data, s->data_cap);
	}

	s->indices_len++;
	size_t new_indices_cap = s->indices_cap;
	if (s->indices_cap == 0) {
		new_indices_cap = 16;
	}
	while (s->indices_len + len + 1 > new_indices_cap) {
		new_indices_cap = 3 * new_indices_cap / 2;
	}
	if (new_indices_cap != s->indices_cap) {
		s->indices_cap = new_indices_cap;
		s->indices = realloc(s->indices, s->indices_cap * sizeof(*s->indices));
	}
	s->indices[s->indices_len - 1] = s->data_len;
	memcpy(s->data + s->data_len, str, len);
	s->data_len += len;
	s->data[s->data_len] = '\0';
	s->data_len++;
}

char *stack_at(stack *s, size_t i) {
	return &s->data[s->indices[i]];
}

void stack_free(stack *s) {
	free(s->data);
	free(s->indices);
	free(s);
}
