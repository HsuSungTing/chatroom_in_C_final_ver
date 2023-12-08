#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "account.h"


// 創建一個新的節點
USER *createUser(const char *name, const char *pw) {
    USER *newNode = (USER *)malloc(sizeof(USER));
    if (newNode != NULL) {
        strcpy(newNode->username, name);
        strcpy(newNode->passward, pw);
        newNode->next = NULL;
    }
    return newNode;
}

void initHashTable(HashTable *hashTable) {
    for (int i = 0; i < TABLE_SIZE; ++i) {
        hashTable->table[i] = NULL;
    }
}

//HashFunction
int hashFunction(const char *name) {
    int hash = 0;
    for (int i = 0; name[i] != '\0'; ++i) {
        hash += name[i];
    }
    return hash % TABLE_SIZE;
}

void insertItem(HashTable *hashTable, const char *name, const char *password) {
    int index = hashFunction(name);
    USER *newNode = createUser(name, password);
    
    //Linked List
    newNode->next = hashTable->table[index];
    hashTable->table[index] = newNode;
}


int loginUser (const HashTable *hashTable, const char *name, const char *password){
    int index = hashFunction(name);
    
    USER *pointer = hashTable->table[index];
    
    // 在Linked List中查找
    while (pointer != NULL) {
        if (strcmp(pointer->username, name) == 0) {
            if(strcmp(pointer->passward, password) == 0){
                return 1;
            }
            // 密碼錯了
            return 0;
        }
        pointer = pointer->next;
    }
    //找不到
    return 0;  
}

void freeHashTable(HashTable *hashTable) {
    for (int i = 0; i < TABLE_SIZE; ++i) {
        USER *current = hashTable->table[i];
        while (current != NULL) {
            USER *next = current->next;
            free(current);
            current = next;
        }
        hashTable->table[i] = NULL;
    }
}