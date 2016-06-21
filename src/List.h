#ifndef LIST_H
#define LIST_H

#include "Types.h"

//
// The elements of the list are structures whose first member is of
// type Node:
//
// struct {
//     Node  link;
//     ...
// }
//
// This structure is referred to in the following as an 'element' when not
// in the list, and as a 'node' when linked into the list.
//

typedef struct Node {
    struct Node *next; // Address of next node in list
    struct Node *prev; // Address of previous node in list
} Node;

typedef struct KeyedNode {
    Node     node;
    intptr_t key;
} KeyedNode;

typedef struct List {
    Node *head; // Address of head of list
    Node *tail; // Address of tail of list
} List;

// Function definition codes for kernel call ListOps
#define LEachElementDo 1
#define LFirstTrue     2
#define LAllTrue       3

#define ToNode(elem)         (&((elem)->link))
#define FromNode(node, type) (CONTAINING_RECORD(node, type, link))
#define NullNode(type)       ((type *)offsetof(type, link))

#define LIST_INITIALIZER { NULL, NULL }

// void InitList(List *list);
//
// Initialize the list as an empty list.  Note that calling this
// for a list which is not empty will render the elements
// inaccessible (at least through the list).
#define InitList(list) FirstNode(list) = LastNode(list) = NULL

// bool EmptyList(List *list);
//
// Returns TRUE if the list is empty, FALSE otherwise.
#define EmptyList(list) (FirstNode(list) == NULL)

// Node *FirstNode(List *list);
//
// Returns a pointer to the first node of the list.
#define FirstNode(list) ((list)->head)

// Node *LastNode(List *list);
//
// Returns a pointer to the last node in a list.
#define LastNode(list) ((list)->tail)

// Node *NextNode(Node *node);
//
// Returns a pointer to the next node in the list.
#define NextNode(node) ((node)->next)

// Node *PrevNode(Node *node);
//
// Returns a pointer to the previous node in the list.
#define PrevNode(node) ((node)->prev)

// bool IsFirstNode(Node *node);
//
// Returns TRUE if the node is the first node in the list.
#define IsFirstNode(node) (PrevNode(node) == NULL)

// bool IsLastNode(Node *node);
//
// Returns TRUE if the node is the last element in the list.
#define IsLastNode(node) (NextNode(node) == NULL)

// void SetKey(Node *node, int key);
//
// Sets the key for the node.
#define SetKey(node, aKey) (((KeyedNode *)(node))->key = ((intptr_t)(aKey)))

// int GetKey(Node *node);
//
// Returns the key for the node.
#define GetKey(node) (((KeyedNode *)(node))->key)

// Deletes the node from the list.
// Returns (!EmptyList(list)).
bool DeleteNode(List *list, Node *node);

// Add the element to the list after the node.
// Returns pointer to the inserted node.
Node *AddAfter(List *list, Node *after, Node *node);

// Add the element to the list before the node.
// Returns pointer to the inserted node.
Node *AddBefore(List *list, Node *before, Node *node);

// Add the element to the beginning of the list.
// Returns pointer to the inserted node.
Node *AddToFront(List *list, Node *node);

// Moves the node to the end of the list.
// Returns pointer to the inserted node.
Node *MoveToFront(List *list, Node *node);

// Add the element to the end of the list.
// Returns pointer to the inserted node.
Node *AddToEnd(List *list, Node *node);

// Moves the node to the end of the list.
// Returns pointer to the inserted node.
Node *MoveToEnd(List *list, Node *node);

// Returns the first node in the list with the given key, or NULL
// if there is no node with that key.
Node *FindKey(List *list, intptr_t key);

// Delete the first node in the list with the given key.
// Return TRUE if a node was found and deleted, FALSE otherwise.
Node *DeleteKey(List *list, intptr_t key);

// Node *AddKeyAfter(List *list, Node *after, Node *node, int key);
//
// Add the element to the list after the node, and sets the key.
// Returns pointer to the inserted node.
#define AddKeyAfter(list, after, node, aKey)                                   \
    AddAfter((list), (after), (node));                                         \
    SetKey(node, aKey)

// Node *AddKeyBefore(List *list, Node *before, Node *node, int key);
//
// Add the element to the list before the node, and sets the key.
// Returns pointer to the inserted node.
#define AddKeyBefore(list, before, node, aKey)                                 \
    AddBefore((list), (before), (node));                                       \
    SetKey(node, aKey)

// Node *AddKeyToFront(List *list, Node *node, int key);
//
// Add the element to the beginning of the list, and sets the key.
// Returns pointer to the inserted node.
#define AddKeyToFront(list, node, aKey)                                        \
    AddToFront((list), (node));                                                \
    SetKey(node, aKey)

// Node *AddKeyToFront(List *list, Node *node, int key);
//
// Add the element to the end of the list, and sets the key.
// Returns pointer to the inserted node.
#define AddKeyToEnd(list, node, aKey)                                          \
    AddToEnd((list), (node));                                                  \
    SetKey(node, aKey)

#endif // LIST_H
