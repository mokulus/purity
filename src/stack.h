#ifndef STACK_H
#define STACK_H
#include <stddef.h>

typedef struct {
	char *data;
	size_t *indices;
	size_t data_len;
	size_t data_cap;
	size_t indices_len;
	size_t indices_cap;
} stack;

void stack_add(stack *s, const char *str, size_t len);
char *stack_at(stack *s, size_t i);
void stack_free(stack *s);

#endif
