#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<memory.h>
#include<regex.h>
#include<unistd.h>
#include<fcntl.h>
#include<sys/types.h>
#include<pwd.h>
#include<unistd.h>
#include<sys/wait.h>
#define CHAR2LINE 1024
#define MAX_CMD_COUNT 100
#define MAX_ARGS_COUNT MAX_CMD_COUNT
#define STDIN 0
#define STDOUT 1
#define NMATCH 1
#define MODE 0666 //mode for creating a new file. in this case it's rw-rw-rw
#define OUTR_A_FLAG O_WRONLY|O_CREAT|O_APPEND
#define OUTR_FLAG O_WRONLY|O_CREAT|O_TRUNC
int cmd_count; // used to out the number of commands in a single line
char in_redir_file[CHAR2LINE];// path of redirecting in file
char out_redir_file[CHAR2LINE];// path of redirecting out file
int last_pid; //last sub-process pid
int is_append; //when redirecting output file, is_append indicate whether to append to that specific file.
struct cmd{
    char **args; // args of command
    int infd;     // input file descriptor
    int outfd;   // output file descriptor
};
struct cmd command[MAX_CMD_COUNT];
void execute_command(int i){
    pid_t pid = fork();
    if(pid>0){ //indicating this is the parent process
	last_pid = pid;
	usleep(500);
    }
    else if(pid==0){
	if(command[i].infd!=0) dup2(command[i].infd,0);
	if(command[i].outfd!=1) dup2(command[i].outfd,1);
        for(int j=3;j<1000;j++) close(j);
    	execvp(command[i].args[0],command[i].args);
	exit(EXIT_FAILURE);
    }
}
void execute(){
    if(cmd_count==0) return ;
    if(strcmp(command[0].args[0],"cd")==0){
	char *path;
	if(command[0].args[1]==NULL||command[0].args[1][0]==' '){
	    struct passwd* pwd;
	    pwd = getpwuid(getuid());
	    path = pwd->pw_dir;
	}else path = command[0].args[1];
	chdir(path);
	return ;
    }
    if(in_redir_file[0]!='\0'){
	command[0].infd = open(in_redir_file,O_RDONLY);
    }
    if(out_redir_file[0]!='\0'){
	if(is_append) command[cmd_count-1].outfd = open(out_redir_file,OUTR_A_FLAG,MODE);
	else command[cmd_count-1].outfd = open(out_redir_file,OUTR_FLAG,MODE);
    }
    int fd;
    int fds[2];
    for(int i=0;i<cmd_count;i++){
	if(i!=cmd_count-1){
	    pipe(fds);
	    command[i].outfd = fds[1];
	    command[i+1].infd = fds[0];
	}
	execute_command(i);
	if((fd=command[i].infd)!=STDIN) close(fd);
	if((fd=command[i].outfd)!=STDOUT) close(fd);
    }
    while(wait(NULL)!=last_pid) ;
}
void parse(char *line){
    char* pattern = "([^ ]+|[\"\'].*[\"\'])";
    char match[CHAR2LINE];
    regex_t regex;
    regmatch_t pmatch[10]={0};
    regcomp(&regex,pattern,REG_EXTENDED);
    int args_count = 0;
    int in_redir=0,out_redir = 0, append = 0;
    while(1){
	regexec(&regex,line,NMATCH,pmatch,0);
	int len = pmatch[0].rm_eo - pmatch[0].rm_so;
	if(len>0){
	    memset(match,0,CHAR2LINE);
	    memcpy(match,line+pmatch[0].rm_so,len);
	    if(strcmp(match,"<")==0){
		in_redir = 1;
	    }else if(strcmp(match,">")==0){
		out_redir = 1;
	    }else if(strcmp(match,">>")==0){
		append = 1;
		is_append = 1;
	    }else if(strcmp(match,"|")==0){ // we'll meet a new command in the next match
		cmd_count++;
		args_count = 0;
	    }else if(in_redir){
		memcpy(in_redir_file,match,strlen(match)+1);
		in_redir = 0;
	    }else if(out_redir||append){
		memcpy(out_redir_file,match,strlen(match)+1);
		out_redir = 0;
		append = 0;
	    }else{
		command[cmd_count].args[args_count] = (char *)malloc(strlen(match)+1);
		int arg_len = strlen(match)+1;
		int arg_index = 0;
		for(int i=0;i<arg_len;i++){
		    if(match[i]!='\"') command[cmd_count].args[args_count][arg_index++] = match[i];
		}
		args_count++;
	    }
	}//end of if(len>0)
	if(pmatch[0].rm_eo < strlen(line)) line+=pmatch[0].rm_eo;
	else break;
    }
    cmd_count++;
}
void cmd_init(){ // initialize command environment
    int i;
    cmd_count = 0;
    memset(in_redir_file,0,CHAR2LINE);
    memset(out_redir_file,0,CHAR2LINE);
    last_pid = 0;
    for(i=0;i<MAX_CMD_COUNT;i++){
        command[i].args = (char **)malloc(sizeof(char *)*MAX_ARGS_COUNT);
	memset(command[i].args,0,sizeof(char *)*MAX_ARGS_COUNT);
//	for(int j=0;j<MAX_ARGS_COUNT;j++) command[i].args[j] = NULL; 
	command[i].infd = STDIN;
	command[i].outfd = STDOUT;
    }
}
int main(){
    char line[CHAR2LINE];
    while(1){
	printf("myshell$");
	cmd_init();
	fgets(line,CHAR2LINE,stdin);
	line[strlen(line)-1] = 0; //get rid of '\n' at the end of line
	if(strcmp(line,"exit")==0) break;
	parse(line);
	execute();
	printf("\n");
    }
}
