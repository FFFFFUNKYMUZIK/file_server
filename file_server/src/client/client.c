#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "../common/config.h"
#include "../common/sock.h"
#include "../common/log.h"
#include "client_proc.h"

#define MAX_COMMAND_SIZE 10

static int cfd;

static void print_usage(char* p){
    printf("usage : %s [IP_ADDR] [PORT]\n", p);
}

static void print_command(){
    printf("\nsupported command : read write put get list quit (all lowercase or uppercase)\n\n"
           "read [filename] [offset] [size] : read file data from server\n"
           "write [filename] [offset] [buf] : write file data to server\n"
            "put [filename]  : push file to server\n"
            "get [filename]  : pull file from server\n"
            "list(ls)        : list files in server\n"
            "quit(q)         : quit program\n");
}

void sigint_handler(int signo){
    close(cfd);
    LERR("\n[SIGINT] close connection.\n");
    exit(0);
}

int main(int argc, char* argv[]){

    if (argc != 3){
        print_usage(argv[0]);
        exit(1);
    }

    log_start("CLIENT");
    set_log_level(CLIENT_LOG_LEVEL);

    signal(SIGINT, sigint_handler);

    cfd = connect_server(argv[1], argv[2]);

    char command[MAX_COMMAND_SIZE + 1];
    char format[10];

    while(1){

        fputs("Command(Q to quit) > ", stdout);

        //parsing command

        sprintf(format, "%%%ds", MAX_COMMAND_SIZE);
        scanf(format, command);
        fflush(stdin);

        if (strcmp(command, "read") == 0 || strcmp(command, "READ") == 0){
            LINFO("read processing...\n");
            read_proc(cfd);
        }
        else if (strcmp(command, "write") == 0|| strcmp(command, "WRITE") == 0){
            LINFO("write processing...\n");
            write_proc(cfd);
        }
        else if (strcmp(command, "put") == 0|| strcmp(command, "PUT") == 0){
            LINFO("put processing...\n");
            put_proc(cfd);

            //clear stdin buffer
            char c;
            while ((c = getchar()) != '\n');
        }
        else if (strcmp(command, "get") == 0|| strcmp(command, "GET") == 0){
            LINFO("get processing...\n");
            get_proc(cfd);

            //clear stdin buffer
            char c;
            while ((c = getchar()) != '\n');
        }
        else if (strcmp(command, "list") == 0 || strcmp(command, "LIST") == 0 || strcmp(command, "ls") == 0){
            LINFO("list processing...\n");
            list_proc(cfd);

            //clear stdin buffer
            char c;
            while ((c = getchar()) != '\n');
        }
        else if (strcmp(command, "q") == 0 || strcmp(command, "Q") == 0 ||
                 strcmp(command, "quit") == 0 || strcmp(command, "QUIT") == 0){
            fputs("BYE BYE!\n", stdout);
            break;
        }
        else{
            LINFO("Invalid Command...\n");
            print_command();
        }

    }

    close(cfd);
    log_end();
    return 0;
}

