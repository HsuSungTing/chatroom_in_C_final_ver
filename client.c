#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/time.h>
#include "account.h"

#define LENGTH 2048
#define INPUT_SIZE 50
#define REQUEST_SIZE 100
#define RESPONSE_SIZE 100

// Global variables
volatile sig_atomic_t flag = 0;
int sockfd = 0;
char password[PASSWORD_SIZE];
char name[NAME_SIZE];
char input[INPUT_SIZE];

void str_overwrite_stdout() {
  printf("%s", "> ");
  fflush(stdout);
}

void str_trim_lf (char* arr, int length) {
  int i;
  for (i = 0; i < length; i++) { // trim \n
    if (arr[i] == '\n') {
      arr[i] = '\0';
      break;
    }
  }
}

void catch_ctrl_c_and_exit(int sig) {   //當用戶按下 Ctrl+C 時，flag = 1，退出程式。
    flag = 1;
}

//等三秒再判斷
int wait_for_string_input(const char *target) {
    char input[LENGTH];            // 定義一個緩沖區來存儲用戶輸入的字符串
    fd_set rfds;                   // 定義文件描述符集合，用於 select 函數
    struct timeval tv;             // 定義時間結構體，用於設置 select 的等待時間
    int retval;                    // 用於存儲 select 函數的返回值

    FD_ZERO(&rfds);                // 清空文件描述符集合
    FD_SET(STDIN_FILENO, &rfds);   // 將標準輸入文件描述符添加到集合中

    tv.tv_sec = 5;                 // 設置等待時間為5秒
    tv.tv_usec = 0;

    retval = select(STDIN_FILENO + 1, &rfds, NULL, NULL, &tv);
    // 使用 select 函數等待標準輸入是否有可讀事件

    if (retval == -1) {
        perror("select()");         // 如果 select 返回 -1，表示出錯，打印錯誤信息
        return 0;                   // 返回 0，表示出錯
    } else if (retval) {
        // 如果 retval 大於 0，表示標準輸入有可讀事件
        fgets(input, LENGTH, stdin);  // 從標準輸入讀取用戶輸入的字符串
        str_trim_lf(input, LENGTH);   // 移除可能的換行符

        return strcmp(input, target) == 0; // 檢查輸入是否與目標字符串相同
    } else {
        // 如果 retval 等於 0，表示超時，沒有輸入
        return 0; // 返回 0，表示沒有輸入
    }
}



void send_msg_handler(){
    char message[LENGTH] = {};
    char buffer[LENGTH + 64] = {}; // 增加缓冲区大小
    //printf("in send_msg\n");
    while (1) {
        str_overwrite_stdout();
        fgets(message, LENGTH, stdin);
        str_trim_lf(message, LENGTH);
        //printf("in while\n");
        if (strcmp(message, "exit") == 0) {
            break;
        } 
        else {
            if (wait_for_string_input("return")) {
              continue;
            }
            else{
              snprintf(buffer, sizeof(buffer), "%s: %s\n", name, message);
              send(sockfd, buffer, strlen(buffer), 0);
            } 
        }

        bzero(message, LENGTH);
        bzero(buffer, sizeof(buffer)); // 清空整个缓冲区
    }
    catch_ctrl_c_and_exit(2);
}

void recv_msg_handler() {   //處理從伺服器接收到的消息
	char message[LENGTH] = {};
  while (1) {
		int receive = recv(sockfd, message, LENGTH, 0);   //recv如果返回值是 0，表示對方已經關閉連接。如果返回值是 -1，表示發生錯誤
    if (receive > 0) {  //成功接收了指定數量的字節。接收到的字節數量是返回值
      if (strcmp(message, "blacklist") == 0) {
          printf("You are in the blacklist. Exiting...\n");
          catch_ctrl_c_and_exit(2);  // 结束客户端
      } 
      else {
          printf("%s", message);
          str_overwrite_stdout();
      }
    } 
    else if (receive == 0) {
			break;
    } 
    else {
			// -1
		}
		memset(message, 0, sizeof(message));
  }
}

int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	char *ip = "127.0.0.1";
	int port = atoi(argv[1]);

	signal(SIGINT, catch_ctrl_c_and_exit);

  struct sockaddr_in server_addr;

	/* Socket settings */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = inet_addr(ip);
  server_addr.sin_port = htons(port);

  // Connect to Server
  int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
  if (err == -1) {
		printf("ERROR: connect\n");
		return EXIT_FAILURE;
	}


  int haveAccount;
  int wantCreateAccount;

  printf("Log in enter 1, Sign in enter 0: ");
  fgets(input, INPUT_SIZE, stdin);
  haveAccount = atoi(input);


  while(1){
    // 有帳號
    if(haveAccount){
      printf("Enter your name: ");
      fgets(name, NAME_SIZE, stdin);
      str_trim_lf(name, strlen(name));
      printf("Enter your password: ");
      fgets(password, PASSWORD_SIZE, stdin);
      str_trim_lf(password, strlen(password));

      // 格式為"login account password"
      char request[REQUEST_SIZE]; 
      snprintf(request, sizeof(request), "login %s %s", name, password);
      send(sockfd, request, strlen(request), 0);

      // 接收伺服器的回應
      char response[RESPONSE_SIZE];
      int receive = recv(sockfd, response, REQUEST_SIZE, 0);
	    if(receive > 0){ 	//成功接收
		    printf("%s\n", response); 
        if(strcmp(response, "Login Success") == 0){
          printf("Hello! %s\n", name);
          break;
        }
        else if(strcmp(response, "Login Fail") == 0){
          printf("Login Fail, can't find your account.\n");
          printf("Goodbye...\n");
          return EXIT_FAILURE;
          wantCreateAccount = atoi(input);
          haveAccount = !wantCreateAccount;
        }
        else{
          printf("Cannot read response.\n");
        }
	    }
	    else{
		    printf("Server connect Fail.\n");
	    }
    }
    // 沒帳號
    else{
      printf("Create your account. Please enter your name: ");
      fgets(name, NAME_SIZE, stdin);
      str_trim_lf(name, strlen(name));
      printf("Let's set up your password: ");
      fgets(password, PASSWORD_SIZE, stdin);
      str_trim_lf(password, strlen(password));

      // 格式為"create name password"
      char request[REQUEST_SIZE];
      snprintf(request, sizeof(request), "create %s %s", name, password);
      send(sockfd, request, strlen(request), 0);

        char response[RESPONSE_SIZE];
        int receive = recv(sockfd, response, REQUEST_SIZE, 0);
        if(receive > 0){ 	//成功接收
          printf("%s\n", response);
        }
        else{
          printf("Server connect Fail.\n");
        }
      break;
    }
  }

  // char response[RESPONSE_SIZE];
  // int receive = recv(sockfd, response, REQUEST_SIZE, 0);
	// if(receive > 0){ 	//成功接收
	// 	printf("%s\n", response);
	// }
	// else{
	// 	printf("Server connect Fail.\n");
	// }
    
  // str_trim_lf(name, strlen(name));


	// if (strlen(name) > 32 || strlen(name) < 2){
	// 	printf("Name must be less than 30 and more than 2 characters.\n");
	// 	return EXIT_FAILURE;
	// }


	// Send name
	send(sockfd, name, 32, 0);

	printf("=== WELCOME TO THE CHATROOM ===\n");

	pthread_t send_msg_thread;
  if(pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, NULL) != 0){
		printf("ERROR: pthread\n");
    return EXIT_FAILURE;
	}

	pthread_t recv_msg_thread;
  if(pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL) != 0){
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	while (1){
		if(flag){
			printf("\nBye\n");
			break;
    }
	}

	close(sockfd);

	return EXIT_SUCCESS;
}

