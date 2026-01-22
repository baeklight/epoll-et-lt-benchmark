#include "sock_list.h"

void list_init(sock_list_t *list){
    list->head = NULL;
    list->tail = NULL;
    list->cnt = 0;
}

void list_push_back(sock_list_t *list, sock_t *node){
    node->next = NULL;

    if(list->cnt == 0){
        list->head = list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }
    list->cnt++;
}

void list_remove(sock_list_t *list, sock_t *node){
    if(list->cnt == 0 || !node) return;

    if(node == list->head && node == list->tail){
        list->head = list->tail = NULL;
    } else if (node == list->head){
        list->head = node->next;
    } else {
        sock_t *prev = list->head;
        while(prev && prev->next != node)
            prev = prev->next;
        if(!prev) return;

        prev->next = node->next;
        if(list->tail == node) list->tail = prev;
    }
    list->cnt--;
}
