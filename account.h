#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAME_SIZE 32
#define PASSWORD_SIZE 32
#define TABLE_SIZE 10

#define ACCOUNT_SIZE 50
#define NAME_SIZE 32
#define TABLE_SIZE 10
#define REQUEST_SIZE 100
#define RESPONSE_SIZE 100

typedef struct USER {
    char username[NAME_SIZE];
    char passward[PASSWORD_SIZE];
    struct USER *next;
} USER;

typedef struct HashTable {
    USER *table[TABLE_SIZE];
} HashTable;

// 創建一個新的節點
USER *createUser(const char *name, const char *pw);

void initHashTable(HashTable *hashTable);

//HashFunction
int hashFunction(const char *name);

void insertItem(HashTable *hashTable, const char *name, const char *password);


int loginUser (const HashTable *hashTable, const char *name, const char *password);

void freeHashTable(HashTable *hashTable);