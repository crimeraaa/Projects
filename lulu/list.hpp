#pragma once

#include "mem.hpp"

template<class T>
struct ListNode {
    T         data = {};
    ListNode *next = nullptr;
};

/*
 Description:
    A singly-linked, intrusive list. This is just a wrapper for nodes
    that handles the dirty work of iteration for us.
 */
template<class T>
struct List {
    ListNode<T> *node  = nullptr;
    int          count = 0;

    bool     operator==(List other) { return this->node == other.node; }
    bool     operator!=(List other) { return !(*this == other); }
    List<T>  begin()                { return *this; }
    List<T>  end()                  { return {nullptr}; }


    /*
     Description:
        Dereference operator. Provides mutable access to the underlying node
        data. It's unfortunate that we *have* to use references, but it's a
        necessary evil especially if we decide to reuse this implementation
        to store lists of pointers.
     */
    T &
    operator*()
    {
        return this->node->data;
    }

    /*
     Description:
        Member access operator. allows us to bypass the constant need to use
        `list.node->field`, instead use `list->field` directly. This makes it
        seem we *are* the underyling object.
     */
    T *
    operator->()
    {
        return &this->node->data;
    }

    /*
     Description:
         Pre-increment operator, i.e. `++list`.
     */
    inline List<T> &
    operator++()
    {
        // Remember that as we traverse the list, we are actually running
        // out of nodes.
        if (this->node) {
            this->node = this->node->next;
            this->count--;
        }
        return *this;
    }

    /*
     Description:
        Post-increment operator, i.e. `list++`.
     */
    inline List<T>
    operator++(int)
    {
        auto next = *this;
        ++(*this);
        return next;
    }
};

template<class T>
static inline void
list_append(lulu_State *L, List<T> *list, Scratch *x, T const &data)
{
    // Find the address of the last *next* node we can append to. We start at
    // the head. Once the head is added, the next one we can append to is the
    // head's next node, so on and so forth.
    auto *(*tail) = &list->node;
    for (T &elem : *list) {
        /*
         This is safe because both types have `T` as their first member.
         All `T` in the list are guaranteed to be part of a node, so they
         always have a `next` member.
         */
        tail = &(cast(ListNode<T> *)&elem)->next;
    }

    *tail         = mem_scratch_alloc<ListNode<T>>(L, x);
    (*tail)->data = data;
    (*tail)->next = nullptr;
    list->count++;
}

template<class T>
static inline T *
list_last_elem(List<T> list)
{
    T *last = nullptr;
    for (T &elem : list) {
        last = &elem;
    }
    return last;
}
