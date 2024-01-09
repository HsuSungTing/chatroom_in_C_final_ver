#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <malloc.h>
#include "account.h"

#define MAX_CLIENTS 100
#define BUFFER_SZ 2048
#define ACCOUNT_SIZE 50
#define NAME_SIZE 32
#define PASSWORD_SIZE 32
#define TABLE_SIZE 10
#define REQUEST_SIZE 100
#define RESPONSE_SIZE 100
#define MAX_BLACKLIST_SIZE 10
#define LINE_LENGTH 100


static _Atomic unsigned int cli_count = 0;	//_Atomic原子性 不會被其他線程所打斷
static int uid = 10;
int room = 0;

/* Client structure */
typedef struct{
	struct sockaddr_in address;
	int sockfd;	
	int uid;
	char name[NAME_SIZE];
} client_t;

// 黑名單
typedef struct blacklist {
    int room;
    char blacklist_name[MAX_BLACKLIST_SIZE][NAME_SIZE];
    struct blacklist *next;
} Blacklist;

Blacklist *head = NULL;
//-------這部分是在處理訊息查找----------
struct record_node{
	char name[32];
	char message[1000];
};
struct record_arr{
	int msg_ct;
	struct record_node record_arr[500];
};
struct record_arr record_array;
//------------------------------------
void str_overwrite_stdout(){
    printf("\r%s", "> ");	//\r:回車(覆蓋該行使光標回到開頭)
    fflush(stdout);
}
//------------------------------------
void createNode(int room, char names[][NAME_SIZE], int count) {
    Blacklist *newNode = (Blacklist *)malloc(sizeof(Blacklist));
    if (newNode != NULL) {
        newNode->room = room;
        newNode->next = NULL;

        for (int i = 0; i < count; i++) {
            if (i < MAX_BLACKLIST_SIZE) {
                strcpy(newNode->blacklist_name[i], names[i]);
            } else {
                // Handle the case where the blacklist is full
                fprintf(stderr, "Blacklist is full. Some names were not added.\n");
                break;
            }
        }

        if (head == NULL) {
            head = newNode;  // If the list is empty, make the new node the head
        } 
		else {
            // Traverse to the end of the list
            Blacklist *ptr = head;
            while (ptr->next != NULL) {
                ptr = ptr->next;
            }

            // Add the new node to the end of the list
            ptr->next = newNode;
        }
    }
}

int findnode(Blacklist* head, int room, char *name) {
    int a = 0;
    Blacklist* ptr = head;
    while (ptr != NULL) {
        if (room != ptr->room) {
            ptr = ptr->next;
        } else {
            a = 1;
            break;  // Add a break statement to exit the loop when room is found
        }
    }
    if (a == 1) {
        for (int i = 0; i < 6; i++) {
            if (strlen(ptr->blacklist_name[i]) > 0 && strcmp(ptr->blacklist_name[i], name) == 0) {
                return 1;
            }
        }
    }
    return 0;
}

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;	//mutex互斥鎖

// ==============================================================
// File
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
// 全局變量，用於文件操作
FILE *logfile;
FILE *accountfile;

// 清除文件内容
void clear_file() {
	char filename[32];
	sprintf(filename, "room%d.txt", room);
    FILE *clearFile = fopen(filename, "w");
    if (clearFile == NULL) {
        perror("ERROR: Could not open file for clearing");
        exit(EXIT_FAILURE);
    }
    fclose(clearFile);
}

// 初始化文件，打開文件寫入
void init_file() {
	//清除文件
	//clear_file();
	pthread_mutex_lock(&file_mutex);  // 加鎖
	char filename[32];
	sprintf(filename, "room%d.txt", room);
    logfile = fopen(filename, "a");
	accountfile = fopen("account.txt", "a+");
    if (logfile == NULL || accountfile == NULL) {
        perror("ERROR: Could not open file");
        exit(EXIT_FAILURE);
    }
	//----------------------------------------------
	sprintf(filename, "room%d.txt", room);
    logfile = fopen(filename, "a+"); // 以讀寫模式開啟檔案
    if (logfile == NULL) {
        perror("ERROR: Could not open file");
        exit(EXIT_FAILURE);
    }
	//-----初始化record_array-------
    fseek(logfile, 0, SEEK_SET);// 將檔案指標移到檔案開頭
    char line[1000]; // 假設一行最多1000個字元
    while (fgets(line, sizeof(line), logfile) != NULL) {
		char *result = strchr(line, ':');
		char name[100], value[100]; // 使用字符陣列來存儲字符串
		if (result != NULL) {
			sscanf(line, "%[^:]:%[^\n]", name, value);
			strcpy(record_array.record_arr[record_array.msg_ct].message,value);//存到server.c的紀錄中
			strcpy(record_array.record_arr[record_array.msg_ct].name,name);//存到server.c的紀錄中
			record_array.msg_ct=(record_array.msg_ct+1)%500;
			//printf("name:%s value:%s\n", name, value);
		} 
	}
	//-----------------------------------------------
	printf("in init file\n");
	for(int i=0;i<record_array.msg_ct;i++){
		printf("%s %s\n",record_array.record_arr[i].name,record_array.record_arr[i].message);
	}
	printf("end of init file\n");
	pthread_mutex_unlock(&file_mutex);  // 解鎖
}
void send_message_to_specific_user(char *s, int uid);

//-----讀取聊天室訊息-------
void read_record_file(int target_user_uid) {
    pthread_mutex_lock(&file_mutex);  // 加鎖
    char filename[32];
    sprintf(filename, "room%d.txt", room);
    logfile = fopen(filename, "a+"); // 以讀寫模式開啟檔案
    if (logfile == NULL) {
        perror("ERROR: Could not open file");
        exit(EXIT_FAILURE);
    }
    // 將檔案指標移到檔案開頭
    fseek(logfile, 0, SEEK_SET);
	// 讀取並顯示檔案內容
    char line[1000]; // 假設一行最多1000個字元
	send_message_to_specific_user("///---Chat Log---///\n", target_user_uid);
	int line_ct=0;
    while (fgets(line, sizeof(line), logfile) != NULL) {
		char line_to_print[1002];

		strcpy(line_to_print,"> ");
		strcat(line_to_print, line); // 將 str2 附加到 str1 的末尾
		
		printf("%s",line_to_print);
		send_message_to_specific_user(line_to_print, target_user_uid);
		line_ct++;
        //printf("Hi\n"); // 每次讀取一行，直到達到檔案結尾
    }
	if(line_ct==0)send_message_to_specific_user("> ///-No Chat Log-///\n", target_user_uid);
	send_message_to_specific_user("> ///End of Chat Log///\n", target_user_uid);
    //fclose(logfile); // 關閉檔案 這邊不能關閉的原因是因為他是全域變數
    pthread_mutex_unlock(&file_mutex);  // 解鎖
}

// 寫入消息到文件
void write_to_file(const char *message) {
	printf("is writing to file now\n");
	pthread_mutex_lock(&file_mutex);  // 加鎖
    if (logfile != NULL) {
        fprintf(logfile, "%s", message);
        fflush(logfile); // 立即寫入文件
    }
	pthread_mutex_unlock(&file_mutex);  // 解鎖
}

void read_accountfile(HashTable *hashTable){
	char line[LINE_LENGTH];
	while (fgets(line, LINE_LENGTH, accountfile) != NULL) {
        char *name = strtok(line, " \t\n");
		char *password = strtok(NULL, " \t\n");
		insertItem(hashTable, name, password);
    }
}

void add_accountfile(const char *name, const char *password){
	pthread_mutex_lock(&file_mutex);  // 加鎖
	if(accountfile == NULL){
		perror("Error opening file");
		printf("accountfile == NULL\n");
		return;
	}
	fprintf(accountfile, "%s %s\n", name, password);
	fflush(accountfile);
	pthread_mutex_unlock(&file_mutex);  // 解鎖
}
// ==============================================================

//Hashtable
typedef struct {
    HashTable *hashTable;
    client_t *client;
} ThreadArgs;


// void str_overwrite_stdout(){
//     printf("\r%s", "> ");	//\r:回車(覆蓋該行使光標回到開頭)
//     fflush(stdout);
// }

void str_trim_lf (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // 去除字符串末尾的換行符(\n)
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}
//----------------------------------------

void print_client_addr(struct sockaddr_in addr){
    printf("%d.%d.%d.%d",
        addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

/* Add clients to queue */
void queue_add(client_t *cl){
	pthread_mutex_lock(&clients_mutex);		//pthread.h提供的函數
	for(int i=0; i < MAX_CLIENTS; ++i){
		if(!clients[i]){
			clients[i] = cl;
			break;
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

/* Remove clients to queue */
void queue_remove(int uid){
	pthread_mutex_lock(&clients_mutex);
	for(int i=0; i < MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				clients[i] = NULL;
				break;
			}
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

/* Send message to all clients except sender寄件人 */
void send_message(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);
	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid != uid){	//如果不是寄件人的話
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){	//write為<unistd.h>的函數
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

// Account request
void handle_account_request(HashTable *hashTable, char *request, int sockfd, int uid){
	char *token = strtok(request, " ");
	if (token == NULL) {
        char message[] = "Request Fail.";
		send(sockfd, message, strlen(message), 0);
        return;
    }
	if (strcmp(token, "login") == 0) {
        // 登錄請求
        char *name = strtok(NULL, " ");
        char *password = strtok(NULL, " ");
        if (name != NULL && password != NULL) {
            if (loginUser(hashTable, name, password)) {		//存在這帳號
				char message[RESPONSE_SIZE] = "Login Success";
				send(sockfd, message, sizeof(message), 0);
				read_record_file(uid);
            } 
			else {
                char message[RESPONSE_SIZE] = "Login Fail";
				send(sockfd, message, sizeof(message), 0);
            }
        } 
		else {
            char message[RESPONSE_SIZE] = "Request Fail.";
			send(sockfd, message, sizeof(message), 0);
        }
    }
	else if(strcmp(token, "create") == 0){
		char *name = strtok(NULL, " ");
        char *password = strtok(NULL, " ");

		if (name != NULL && password != NULL) {
			insertItem(hashTable, name, password);
			printf("An account has been created.\n[Name:%s Password:%s]\n", name, password);
			// ================================================================================
			fprintf(accountfile, "%s %s\n", name, password);
			fflush(accountfile);
			// ================================================================================
			char response[RESPONSE_SIZE];
            snprintf(response, RESPONSE_SIZE, "Hello!, %s\n", name);
			send(sockfd, response, sizeof(response), 0);
			return;
		}
		else {
            char message[] = "Create Fail";
			send(sockfd, message, sizeof(message), 0);
        }

	}
}
void send_message_to_specific_user(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);
	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid == uid){
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

void computeLPSArray(const char *pat, int M, int *lps) {
    int len = 0;
    lps[0] = 0;
    int i = 1;

    while (i < M) {
        if (pat[i] == pat[len]) {
            len++;
            lps[i] = len;
            i++;
        } else {
            if (len != 0) {
                len = lps[len - 1];
            } else {
                lps[i] = 0;
                i++;
            }
        }
    }
}

// KMP演算法的主函數，用於在單個文本str中搜索模式pat
void KMPSearch(const char *pat, const struct record_node node,int uid,int *found_bool) {
    int M = strlen(pat);
    int N = strlen(node.message);

    int *lps = (int *)malloc(sizeof(int) * M);
    int j = 0;

    computeLPSArray(pat, M, lps);

    int i = 0;
    while (i < N) {
        if (pat[j] == node.message[i]) {
            j++;
            i++;
        }

        if (j == M) {
            //printf("%s\n",node.message);
			*found_bool=1;
			char to_user[100]= {'\0'};
			strcat(to_user,node.name);
			strcat(to_user,": ");
			strcat(to_user,node.message);
			strcat(to_user,"\n");
			//printf("to_user : %s",to_user);
			send_message_to_specific_user(to_user, uid);
            break;
        } else if (i < N && pat[j] != node.message[i]) {
            if (j != 0)
                j = lps[j - 1];
            else
                i++;
        }
    }

    free(lps);
}

// 在字串陣列中進行KMP演算法的查找
void findSubstringInArray(struct record_arr obj, int size, const char *substr,int uid) {
	// printf("in KMP\n");
	// for(int i=0;i<obj.msg_ct;i++){
	// 	printf("%s %s\n",obj.record_arr[i].name,obj.record_arr[i].message);
	// }
    int found_bool=0;
	send_message_to_specific_user("The result:\n", uid);
	for (int i = 0; i < size; ++i) {
		KMPSearch(substr, obj.record_arr[i],uid,&found_bool);
    }
	if(found_bool==0)send_message_to_specific_user("the string is not found...\n", uid);
}

void send_private_message(char *s, int receiver_uid){
	pthread_mutex_lock(&clients_mutex);
	for(int i=0; i<MAX_CLIENTS; ++i){
		if(clients[i]){
			if(clients[i]->uid==receiver_uid){
				if(write(clients[i]->sockfd, s, strlen(s)) < 0){
					perror("ERROR: write to descriptor failed");
					break;
				}
			}
		}
	}
	pthread_mutex_unlock(&clients_mutex);
}

int checkColonBeforeGreater(char* str) {
	//printf("str in checkColonBeforeGreater %s\n",str);
    int foundColon = 0;
    int foundGreater = 0;

    for (int i = 0; str[i] != '\0'; ++i) {
        if (str[i] == ':') {
            foundColon = 1;
        } else if (str[i] == '>') {
            foundGreater = 1;
            if (foundColon==0) return 0; // 如果 '>' 出現在 ':' 前面，回傳0
        }
    }
    if (foundColon==1&&foundGreater==1)return 1;
	else return 0;
}

void removeLeadingSpaces(char* str) {
    int i, j = 0, len;
    len = strlen(str);
    for (i = 0; i < len; i++) {// 遍歷字串找到第一個非空格字符的位置
        if (str[i] != ' ' && str[i] != '\t' && str[i] != '\n' && str[i] != '\r') {
            break;
        }
    }
    for (j = 0; j < len - i; j++) {// 將非空格字符移到字串開頭
        str[j] = str[i + j];
    }
    str[j] = '\0'; // 在新的開頭位置後添加結尾符號
}
//--------------------------------------------
/* Handle all communication with the client */
void *handle_client(void *arg){
	char buff_out[BUFFER_SZ];
	char name[32];
	char request[REQUEST_SIZE];
	int leave_flag = 0;

	ThreadArgs *args = (ThreadArgs *)arg;
    HashTable *hashTable = args->hashTable;

	cli_count++;
	client_t *cli = args->client;

	// 接收登入資訊
	int receive = recv(cli->sockfd, request, REQUEST_SIZE, 0);
	if(receive > 0){ 	//成功接收
		handle_account_request(hashTable, request, cli->sockfd, cli->uid);
	}
	else{
		char message[] = "handle_account_request Fail.";
		send(cli->sockfd, message, strlen(message), 0);
	}

	// Name
	if(recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) <  2 || strlen(name) >= 32-1){	//recv如果返回值是 0，表示對方已經關閉連接。如果返回值是 -1，表示發生錯誤
		// printf("Didn't enter the name.\n");
		leave_flag = 1;
	} else{
		// 在黑名單中未找到
		if (findnode(head, room, name) == 0){
			strcpy(cli->name, name);
			sprintf(buff_out, "%s has joined\n", cli->name);
			printf("%s", buff_out);	 //在server上印出誰加入了
			send_message(buff_out, cli->uid);	//在client上印出誰加入了

			// Write the joined message to the file
			fprintf(logfile, "%s has joined\n", cli->name);
			fflush(logfile);
		}
		// 在黑名單中找到
		else{
			strcpy(cli->name, name);
			sprintf(buff_out, "%s is in blacklist, so he/she can't login in\n", cli->name);
			printf("%s", buff_out);
			send_message(buff_out, cli->uid);
			send(cli->sockfd, "blacklist", strlen("blacklist"), 0);
            leave_flag = 1;
		}
	}

	bzero(buff_out, BUFFER_SZ);	//清空 buff_out的內存

	while(1){
		if (leave_flag) {
			break;
		}

		int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
		if (receive > 0){
			if(strlen(buff_out) > 0){
				//printf("The current buffouts: %s\n",buff_out);
				//-------------------字串切割-----------------------
				int find_member_bool,private_msg_bool,read_bool;
				char name[100],value[1000],private_str[1000],to_whom[1000],msg[1000],private_msg[2000],find_str[1000],sub_tar[100];
				sscanf(buff_out,"%[^:]:%[^\n]", name, value);
				removeLeadingSpaces(value);
				sscanf(value,"%[^:]:%[^\n]",find_str,sub_tar);
				if(strcmp(value,"member")==0)find_member_bool=1;
				else find_member_bool=0;
				if(strcmp(value,"read")==0)read_bool=1;
				else read_bool=0;
				if(checkColonBeforeGreater(value)){
					// printf("here\n");
					sscanf(value, "%[^:]:%[^>]>%[^:]s",private_str, to_whom,msg);
					snprintf(private_msg, 1000, "Private message from ");
					strncat(private_msg, cli->name, 2000-strlen(private_msg)-strlen(cli->name));
					strncat(private_msg,":", 2000-strlen(private_msg)-1);
					strncat(private_msg,msg, 2000-strlen(private_msg)-strlen(msg));
					printf("private_msg: %s\n",private_msg);
					if(strcmp(private_str,"private")==0)private_msg_bool=1;
					else private_msg_bool=0;
				}
				else private_msg_bool=0;
				//----------------------------------------------
				if(strcmp(find_str,"find")==0){
					printf("in find_msg\n");
					removeLeadingSpaces(sub_tar);
					findSubstringInArray(record_array,record_array.msg_ct ,sub_tar, cli->uid);
				}
				else if(private_msg_bool==1){
					int receiver_uid=-1;
					for (int i = 0; i < cli_count; i++) {
						if(strcmp(clients[i]->name,to_whom)==0){
							receiver_uid=clients[i]->uid;
							break;
						}
					}
					int length = strlen(private_msg);
					sprintf(private_msg + length, "\n"); // 在字串尾端加上換行符號
					if(receiver_uid==-1)send_message_to_specific_user("the user not online\n", cli->uid);
					else send_private_message(private_msg,receiver_uid);
				}
				else if (find_member_bool==1) {//查詢當前上線的成員
					char temp[BUFFER_SZ];
					snprintf(temp, sizeof(temp), "Online member: ");
					for (int i = 0; i < cli_count; i++) {// 這裡會先複製 "Online member: "，然後進行串接
						strncat(temp, clients[i]->name, sizeof(temp) - strlen(temp) - 1);
						if (i < cli_count - 1) {
							strncat(temp, ", ", sizeof(temp) - strlen(temp) - 1);
						}
						str_trim_lf(temp, strlen(temp));
					}
					int length = strlen(temp);
					if (temp[length - 1] != '\n') sprintf(temp + length, "\n"); // 在字串尾端加上換行符號
					send_message_to_specific_user(temp, cli->uid);
				}
				else if(read_bool==1){
					printf("is reading now.....\n");
					//read_record_file();
				}
				else{
					// handle_account_request(&userAccountDatabase, buff_out, cli->sockfd, cli->uid);
					send_message(buff_out, cli->uid);
					write_to_file(buff_out);
					str_trim_lf(buff_out, strlen(buff_out));
					strcpy(record_array.record_arr[record_array.msg_ct].message,value);//存到server.c的紀錄中
					strcpy(record_array.record_arr[record_array.msg_ct].name,name);//存到server.c的紀錄中
					record_array.msg_ct=(record_array.msg_ct+1)%500;
					printf("%s\n", buff_out);
					// printf("%s -> %s\n", buff_out, cli->name);
				}
			}
		}
		else if (receive == 0 || strcmp(buff_out, "exit") == 0){
			sprintf(buff_out, "%s has left\n", cli->name);
			printf("%s", buff_out);
			send_message(buff_out, cli->uid);
			leave_flag = 1;
		}
		else {	//recv回傳-1
			printf("ERROR: -1\n");
			leave_flag = 1;
		}

		bzero(buff_out, BUFFER_SZ);
	}

  	/* Delete client from queue and yield thread */
	close(cli->sockfd);
  	queue_remove(cli->uid);
  	free(cli);
  	cli_count--;
  	pthread_detach(pthread_self());

	return NULL;
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char names_1[6][NAME_SIZE] = {"John", "Doe", "Alice", "Bob", "Eve", "Charlie"};
	char names_2[6][NAME_SIZE] = {"John", "Jim", "Alice", "Kevin", "Eason", "William"};
	char names_3[6][NAME_SIZE] = {"Enderson", "Doe", "Frank", "Bob","Kevin","Eason"};
	createNode(4444, names_1, 6);
	createNode(1111, names_2, 6);
	createNode(2222, names_3, 6);

	// Accout Database
	HashTable userAccountDatabase;
	initHashTable(&userAccountDatabase);

	char *ip = "127.0.0.1";
	//char *ip="192.168.56.1";
	int port = atoi(argv[1]);
	room = port;
	int option = 1;
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	pthread_t tid;

  	/* Socket settings */
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(ip);
	serv_addr.sin_port = htons(port);

  	/* Ignore pipe signals */
	signal(SIGPIPE, SIG_IGN);	//為了防止由於向已經關閉的套接字寫入而終止程序
	if(setsockopt(listenfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0){
		perror("ERROR: setsockopt failed");
    	return EXIT_FAILURE;
	}
	/* Bind */
  	if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
    	perror("ERROR: Socket binding failed");
    	return EXIT_FAILURE;
  	}
  	/* Listen */
  	if (listen(listenfd, 10) < 0) {
    	perror("ERROR: Socket listening failed");
    	return EXIT_FAILURE;
	}
	printf("=== WELCOME TO THE CHATROOM ===\n");
	init_file();
	read_accountfile(&userAccountDatabase);
	while(1){
		socklen_t clilen = sizeof(cli_addr);
		connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);
		if((cli_count + 1) == MAX_CLIENTS){
			printf("Max clients reached. Rejected: ");
			print_client_addr(cli_addr);
			printf(":%d\n", cli_addr.sin_port);
			close(connfd);
			continue;
		}
		client_t *cli = (client_t *)malloc(sizeof(client_t));
		cli->address = cli_addr;
		cli->sockfd = connfd;
		cli->uid = uid++;
		queue_add(cli);
		ThreadArgs threadArgs = {&userAccountDatabase, cli};
		pthread_create(&tid, NULL, &handle_client, (void*)&threadArgs);
		sleep(1);
	}
	// Close the file
	fclose(logfile);
	fclose(accountfile);
	return EXIT_SUCCESS;
}