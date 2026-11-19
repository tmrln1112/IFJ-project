#include <stdio.h>
#include <stdlib.h>


#include "scanner.h"
#include "ast.h"
#include "pstack.h"

int prec_stack_init(precedence_stack_t *stack, int capacity)
{   
    if (!stack || capacity <= 0) { return 1; }

    stack->items = malloc(sizeof(stack_item_t) * capacity);
    if (!stack->items)
    {
        stack->top = -1;
        stack->capacity = 0;
        return 1;
    }

    stack->top = -1;
    stack->capacity = capacity;
    return 0;
}

int prec_stack_push(precedence_stack_t *stack, stack_item_t item) 
{
    if (!stack) { return 1; }

    if (stack->top + 1 == stack->capacity) 
    {
        int new_capacity = stack->capacity + 5;
        stack_item_t* new_items= realloc(stack->items, sizeof(stack_item_t) * new_capacity);
        if (!new_items) { return 1; }

        stack->items = new_items;
        stack->capacity = new_capacity;
    }
    stack->items[++stack->top] = item;
    return 0;
}

int prec_stack_pop(precedence_stack_t *stack, stack_item_t *item) 
{
    if (!stack || !item || stack->top <= -1) { return 1; }

    *item = stack->items[stack->top--];
    return 0;
}

int prec_stack_top(precedence_stack_t *stack, stack_item_t *item)
{
    if (!stack || !item || stack->top <= -1) { return 1; }

    *item = stack->items[stack->top];
    return 0;
}

int prec_stack_top_terminal(precedence_stack_t *stack, stack_item_t *item)
{
    if (!stack || stack->top <= -1) { return 1; }

    for (int i = stack->top; i >= 0; i--) 
    {
        if (stack->items[i].type != S_EXP) 
        {
            *item = stack->items[i];
            return 0;
        }
    }
    return 0;
}

void prec_stack_free(precedence_stack_t *stack) 
{
    if (!stack) { return; }
    for (int i = 0; i <= stack->top; i++) 
    {
        free(stack->items[i].token.attribute);
        if (stack->items[i].node) 
        {
            ast_destroy(stack->items[i].node);
            stack->items[i].node = NULL;
        }
    }
    free(stack->items);
}