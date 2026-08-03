/* A ring buffer.
 * This block comment spans several lines on purpose, so `loc` has
 * a real /* */ block to attribute to comments rather than code.
 */
#include <stdlib.h>

typedef struct {
    int *slots;
    int cap, head, len;
} Queue;

Queue *queue_new(int cap) {
    Queue *q = malloc(sizeof *q);   // caller owns it
    q->slots = malloc(sizeof(int) * cap);
    q->cap = cap;
    q->head = 0;
    q->len = 0;
    return q;
}

int queue_push(Queue *q, int v) {
    if (q->len == q->cap) return 0;
    q->slots[(q->head + q->len) % q->cap] = v;
    q->len++;
    return 1;
}

void queue_free(Queue *q) {
    free(q->slots);
    free(q);
}
