#include "List.h"

bool DeleteNode(List *list, Node *node)
{
    if (IsFirstNode(node)) {
        FirstNode(list) = NextNode(node);
    } else {
        NextNode(PrevNode(node)) = NextNode(node);
    }

    if (IsLastNode(node)) {
        LastNode(list) = PrevNode(node);
    } else {
        PrevNode(NextNode(node)) = PrevNode(node);
    }

    return !EmptyList(list);
}

Node *AddAfter(List *list, Node *after, Node *node)
{
    NextNode(node) = NextNode(after);
    PrevNode(node) = after;

    if (IsLastNode(after)) {
        LastNode(list) = node;
    } else {
        PrevNode(NextNode(after)) = node;
    }

    NextNode(after) = node;
    return node;
}

Node *AddBefore(List *list, Node *before, Node *node)
{
    NextNode(node) = before;
    PrevNode(node) = PrevNode(before);

    if (IsFirstNode(before)) {
        FirstNode(list) = node;
    } else {
        NextNode(PrevNode(before)) = node;
    }

    PrevNode(before) = node;
    return node;
}

Node *AddToFront(List *list, Node *node)
{
    NextNode(node) = FirstNode(list);
    PrevNode(node) = NULL;

    if (EmptyList(list)) {
        LastNode(list) = node;
    } else {
        PrevNode(FirstNode(list)) = node;
    }

    FirstNode(list) = node;
    return node;
}

Node *MoveToFront(List *list, Node *node)
{
    if (!IsFirstNode(node)) {
        DeleteNode(list, node);
        AddToFront(list, node);
    }
    return node;
}

Node *AddToEnd(List *list, Node *node)
{
    NextNode(node) = NULL;
    PrevNode(node) = LastNode(list);

    if (EmptyList(list)) {
        FirstNode(list) = node;
    } else {
        NextNode(LastNode(list)) = node;
    }

    LastNode(list) = node;
    return node;
}

Node *MoveToEnd(List *list, Node *node)
{
    if (!IsLastNode(node)) {
        DeleteNode(list, node);
        AddToEnd(list, node);
    }
    return node;
}

Node *FindKey(List *list, intptr_t key)
{
    Node *node = NULL;

    if (list != NULL) {
        for (node = FirstNode(list); node != NULL; node = NextNode(node)) {
            if (GetKey(node) == key) {
                break;
            }
        }
    }
    return node;
}

Node *DeleteKey(List *list, intptr_t key)
{
    Node *node;

    node = FindKey(list, key);
    if (node != NULL) {
        DeleteNode(list, node);
    }
    return node;
}
