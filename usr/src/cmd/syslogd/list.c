#pragma ident "@(#)list.c 1.1	96/11/04 SMI"

#include <pthread.h>
#include <malloc.h>
#include <memory.h>
#include <assert.h>
#include <poll.h>
#include <stdio.h>
#include "llt.h"
void
ll_init(llh_t *head)
{
	head->back = &head->front;
	head->front = NULL;
}
void
ll_enqueue(llh_t *head, ll_t *data)
{
	data->n = NULL;
	*head->back = data;
	head->back = &data->n;
}
ll_t *
ll_peek(llh_t *head)
{
	return (head->front);
}
ll_t *
ll_dequeue(llh_t *head)
{
	ll_t *ptr;
	ptr = head->front;
	if (ptr && ((head->front = ptr->n) == NULL))
		head->back = &head->front;
	return (ptr);
}
ll_t *
ll_traverse(llh_t *ptr, int (*func)(void *, void *), void *user)
{
	ll_t *t;
	ll_t **prev = &ptr->front;

	t = ptr->front;
	while (t) {
		switch (func(t, user)) {
		case 1:
			return (NULL);
		case 0:
			prev = &(t->n);
			t = t->n;
			break;
		case -1:
			if ((*prev = t->n) == NULL)
				ptr->back = prev;
			return (t);
		}
	}
	return (NULL);
}
/* Make sure the list isn't corrupt and returns number of list items */
int
ll_check(llh_t *head)
{
	int i = 0;
	ll_t *ptr = head->front;
	ll_t **prev = &head->front;

	while (ptr) {
		i++;
		prev = &ptr->n;
		ptr = ptr->n;
	}
	assert(head->back == prev);
	return (i);
}
typedef struct list_entry {
	struct ll list;
	void *data;
} list_entry_t;

typedef struct list {
	int count;
	pthread_mutex_t lock;
	llh_t head;
} list_t;
static int
list_check(list_t *ptr)										/* call while holding lock */
{
	assert(ptr->count == ll_check(&ptr->head));
	return (1);
}
int
list_init(list_t *ptr)
{
	ptr->count = 0;
	ll_init(&ptr->head);
	pthread_mutex_init(&ptr->lock, NULL);
	assert(pthread_mutex_lock(&ptr->lock) == 0 &&
		  list_check(ptr) &&
		  pthread_mutex_unlock(&ptr->lock) == 0);
	return (0);
}
int
list_add(list_t *ptr, void *item)
{
	/* we call malloc w/o holding lock to save time */
	list_entry_t *e = (list_entry_t *) malloc(sizeof (*e));

	if (e == NULL)
		return (-1);
	e->data = item;
	pthread_mutex_lock(&ptr->lock);
	assert(list_check(ptr));
	ptr->count++;
	ll_enqueue(&ptr->head, &e->list);
	assert(list_check(ptr));
	pthread_mutex_unlock(&ptr->lock);
	return (0);
}
struct wrap {
	int (*func)(void *, void *);
	void *user;
};
static int
wrapper(void *e, void *u)
{
	list_entry_t *q = (list_entry_t *) e;
	struct wrap *w = (struct wrap *)u;
	return (w->func(q->data, w->user));
}
int
list_traverse(list_t *ptr, int (*func)(void *, void *), void *user)
{
	list_entry_t *ret;

	struct wrap wrap;
	wrap.func = func;
	wrap.user = user;
	pthread_mutex_lock(&ptr->lock);
	assert(list_check(ptr));
	ret = (list_entry_t *)
		ll_traverse(&ptr->head, wrapper, (void *) &wrap);
	if (ret) {
		free(ret);
		ptr->count--;
	}
	assert(list_check(ptr));
	pthread_mutex_unlock(&ptr->lock);
	return (0);
}
static int
matchit(void *data, void *compare)
{
	return ((data == compare)? -1: 0);
}
int
list_remove(list_t *ptr, void *item)
{
	return (list_traverse(ptr, matchit, item)? -1: 0);
}
int
list_destroy(list_t *ptr)
{
	list_entry_t *e;

	while ((e = (list_entry_t *) ll_dequeue(&ptr->head)) != NULL)
		free(e);
	return (pthread_mutex_destroy(&ptr->lock));
}
