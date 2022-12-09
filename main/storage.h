#pragma once
#include <string.h>
#include "nvs_flash.h"
#define STORAGE_NVS_PARTITION_NAME "report_maps"

// basic nvs tools
void storage_init();
nvs_handle_t storage_open(const char *ns);

// saved_list
#ifndef LIST_HEAD_DEFINED
#define LIST_HEAD_DEFINED
typedef struct saved_list_head{
    struct saved_list_head *prev, *next; 
    char name[16];
}saved_list_head_t;
#endif

typedef struct{
	struct saved_list_head *prev, *next; 
	nvs_handle_t handle;
	const char* ns;
	int save_offset;
	size_t save_length;
}saved_list_t;

extern size_t _saved_list_load(saved_list_head_t *list_out, int save_offset, nvs_handle_t handle,\
                         const char *ns, saved_list_head_t *(*alloc_new_item)(size_t));
extern void _saved_list_add(saved_list_head_t *new, saved_list_head_t* list, int save_offset, size_t save_length, nvs_handle_t handle);
extern void _saved_list_add_tail(saved_list_head_t *new, saved_list_head_t* list, int save_offset, size_t save_length, nvs_handle_t handle);
extern void _saved_list_del(saved_list_head_t *entry, saved_list_head_t* list, nvs_handle_t handle);


inline static void saved_list_create(saved_list_t *list,char *ns, int save_offset, size_t save_length){
	list->prev = list->next = (saved_list_head_t *)list;
	list->handle = storage_open(ns);
	list->ns = ns;
	list->save_offset = save_offset;
	list->save_length = save_length;
}

static inline void saved_list_load(saved_list_t *list_out, saved_list_head_t *(*alloc_new_item)(size_t)){
	_saved_list_load((saved_list_head_t *)list_out, list_out->save_offset, list_out->handle, list_out->ns, alloc_new_item);
}

static inline void saved_list_add(saved_list_head_t *new, saved_list_t* list){
	_saved_list_add(new, (saved_list_head_t *)list, list->save_offset, list->save_length, list->handle);
}

static inline void saved_list_add_tail(saved_list_head_t *new, saved_list_t* list){
	_saved_list_add_tail(new, (saved_list_head_t *)list, list->save_offset, list->save_length, list->handle);
}

static inline void _saved_list_save_item(saved_list_head_t *node, saved_list_head_t *list, int save_offset, size_t save_length, nvs_handle_t handle){
    nvs_set_blob(handle, node->name, ((void *)node) + save_offset, save_length);
}

static inline void saved_list_item_changed(saved_list_head_t *item, saved_list_t* list){
	_saved_list_save_item(item, (saved_list_head_t *)list, list->save_offset, list->save_length, list->handle);
}

static inline void saved_list_delete_item(saved_list_head_t *item, saved_list_t* list){
	_saved_list_del(item, (saved_list_head_t *)list, list->handle);
}

static inline esp_err_t saved_list_commit(saved_list_t *list){
	return nvs_commit(list->handle);
}

static inline void saved_list_replace(saved_list_head_t *old, saved_list_head_t *new){
	new->next = old->next;
	new->next->prev = new;
	new->prev = old->prev;
	new->prev->next = new;
}

static inline int saved_list_is_last(saved_list_t *list, saved_list_head_t *node){
	return list->next == node;
}

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#ifndef container_of
/**
* container_of - cast a member of a structure out to the containing structu
* @ptr:        the pointer to the member.
* @type:       the type of the container struct this is embedded in.
* @member:     the name of the member within the struct.
*
*/

#define  container_of(ptr, type, member) ({                      \
		const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
		(type *)( (char *)__mptr - offsetof(type,member) );})
#endif
/**
 * list_entry - get the struct for this entry
 * @ptr:	the &struct list_head pointer.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

/**
 * list_first_entry - get the first element from a list
 * @ptr:	the list head to take the element from.
 * @type:	the type of the struct this is embedded in.
 * @member:	the name of the list_struct within the struct.
 *
 * Note, that list is expected to be not empty.
 */
#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)

/**
 * list_for_each	-	iterate over a list
 * @pos:	the &struct list_head to use as a loop cursor.
 * @head:	the head for your list.
 */
#define list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

inline saved_list_head_t *saved_list_search(saved_list_t *list, const char *name) {
  saved_list_head_t *pos;
  list_for_each(pos, (saved_list_head_t *)list) {
    if (strcmp(pos->name, name) == 0) {
      return pos;
    }
  }
  return NULL;
}


/**
 * __list_for_each	-	iterate over a list
 * @pos:	the &struct list_head to use as a loop cursor.
 * @head:	the head for your list.
 *
 * This variant doesn't differ from list_for_each() any more.
 * We don't do prefetching in either case.
 */
#define __list_for_each(pos, head) \
	for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * list_for_each_prev	-	iterate over a list backwards
 * @pos:	the &struct list_head to use as a loop cursor.
 * @head:	the head for your list.
 */
#define list_for_each_prev(pos, head) \
	for (pos = (head)->prev; pos != (head); pos = pos->prev)

/**
 * list_for_each_safe - iterate over a list safe against removal of list entry
 * @pos:	the &struct list_head to use as a loop cursor.
 * @n:		another &struct list_head to use as temporary storage
 * @head:	the head for your list.
 */
#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

/**
 * list_for_each_prev_safe - iterate over a list backwards safe against removal of list entry
 * @pos:	the &struct list_head to use as a loop cursor.
 * @n:		another &struct list_head to use as temporary storage
 * @head:	the head for your list.
 */
#define list_for_each_prev_safe(pos, n, head) \
	for (pos = (head)->prev, n = pos->prev; \
	     pos != (head); \
	     pos = n, n = pos->prev)

/**
 * list_for_each_entry	-	iterate over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head); 	\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

/**
 * list_for_each_entry_reverse - iterate backwards over list of given type.
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry_reverse(pos, head, member)			\
	for (pos = list_entry((head)->prev, typeof(*pos), member);	\
	     &pos->member != (head); 	\
	     pos = list_entry(pos->member.prev, typeof(*pos), member))

/**
 * list_prepare_entry - prepare a pos entry for use in list_for_each_entry_continue()
 * @pos:	the type * to use as a start point
 * @head:	the head of the list
 * @member:	the name of the list_struct within the struct.
 *
 * Prepares a pos entry for use as a start point in list_for_each_entry_continue().
 */
#define list_prepare_entry(pos, head, member) \
	((pos) ? : list_entry(head, typeof(*pos), member))

/**
 * list_for_each_entry_continue - continue iteration over list of given type
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 *
 * Continue to iterate over list of given type, continuing after
 * the current position.
 */
#define list_for_each_entry_continue(pos, head, member) 		\
	for (pos = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head);	\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

/**
 * list_for_each_entry_continue_reverse - iterate backwards from the given point
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 *
 * Start to iterate over list of given type backwards, continuing after
 * the current position.
 */
#define list_for_each_entry_continue_reverse(pos, head, member)		\
	for (pos = list_entry(pos->member.prev, typeof(*pos), member);	\
	     &pos->member != (head);	\
	     pos = list_entry(pos->member.prev, typeof(*pos), member))

/**
 * list_for_each_entry_from - iterate over list of given type from the current point
 * @pos:	the type * to use as a loop cursor.
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 *
 * Iterate over list of given type, continuing from current position.
 */
#define list_for_each_entry_from(pos, head, member) 			\
	for (; &pos->member != (head);	\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		n = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

/**
 * list_for_each_entry_safe_continue - continue list iteration safe against removal
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 *
 * Iterate over list of given type, continuing after current point,
 * safe against removal of list entry.
 */
#define list_for_each_entry_safe_continue(pos, n, head, member) 		\
	for (pos = list_entry(pos->member.next, typeof(*pos), member), 		\
		n = list_entry(pos->member.next, typeof(*pos), member);		\
	     &pos->member != (head);						\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

/**
 * list_for_each_entry_safe_from - iterate over list from current point safe against removal
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 *
 * Iterate over list of given type from current point, safe against
 * removal of list entry.
 */
#define list_for_each_entry_safe_from(pos, n, head, member) 			\
	for (n = list_entry(pos->member.next, typeof(*pos), member);		\
	     &pos->member != (head);						\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

/**
 * list_for_each_entry_safe_reverse - iterate backwards over list safe against removal
 * @pos:	the type * to use as a loop cursor.
 * @n:		another type * to use as temporary storage
 * @head:	the head for your list.
 * @member:	the name of the list_struct within the struct.
 *
 * Iterate backwards over list of given type, safe against removal
 * of list entry.
 */
#define list_for_each_entry_safe_reverse(pos, n, head, member)		\
	for (pos = list_entry((head)->prev, typeof(*pos), member),	\
		n = list_entry(pos->member.prev, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.prev, typeof(*n), member))

/**
 * list_safe_reset_next - reset a stale list_for_each_entry_safe loop
 * @pos:	the loop cursor used in the list_for_each_entry_safe loop
 * @n:		temporary storage used in list_for_each_entry_safe
 * @member:	the name of the list_struct within the struct.
 *
 * list_safe_reset_next is not safe to use in general if the list may be
 * modified concurrently (eg. the lock is dropped in the loop body). An
 * exception to this is if the cursor element (pos) is pinned in the list,
 * and list_safe_reset_next is called after re-taking the lock and before
 * completing the current iteration of the loop body.
 */
#define list_safe_reset_next(pos, n, member)				\
	n = list_entry(pos->member.next, typeof(*pos), member)

