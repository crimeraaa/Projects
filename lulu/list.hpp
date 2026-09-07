#pragma once

#include "mem.hpp"

/*
 Description:
    A singly-linked, intrusive list. This is just a wrapper for nodes
    that handles the dirty work of iteration for us.
 */
template<class T>
struct List {
    struct Node {
        T     data{};
        Node *next = nullptr;
    };

    Node *node  = nullptr;
    int   count = 0;

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
        // out of nodes. However for the most part we don't care about the
        // number of list members.
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

/*
 Description:
    Adds the given data to the end of the list, making it the new tail.
 */
template<class T>
static inline void
list_append(lulu_State *L, List<T> *list, Scratch *x, T const &data)
{
    using Node = typename List<T>::Node;

    /*
     In order to mutate the list in place, we need a reference to the tail's
     `next` member. When we start with an empty list, the tail is the head
     and, there is no next node. Otherwise we have to traverse the entire list
     until we hit the last non-null node, at which point we can reference their
     `next` member in order to update the list.
     */
    Node **tail = &list->node;
    for (T &elem : *list) {
        /*
         This is safe because `Node`s have a `T` as their first member.
         So a pointer to a `Node` can be treated as a mere pointer to `T`.
         Likeise, instaces of `T` that are actually part of `Node`s can be
         similarly casted.
         */
        tail = &(cast(Node *)&elem)->next;
    }

    *tail         = mem_scratch_alloc<Node>(L, x);
    (*tail)->data = data;
    (*tail)->next = nullptr;
    list->count++;
}


/*
 Description:
    Retrieves a pointer to the data of the last node. Note that, if the list
    is empty, thence the data will be null- hence we use pointers rather than
    references.
 */
template<class T>
static inline T *
list_last_elem(List<T> list)
{
    // This is a valid reinterpret cast, see above.
    T *last = cast(T *)list.node;
    for (T &elem : list) {
        last = &elem;
    }
    return last;
}
