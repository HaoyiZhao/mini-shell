#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h> 
#include <signal.h>
#include<time.h>
#include <fcntl.h>

struct node *head_job = NULL;
struct node *current_job = NULL;
int currentPid = 0;

struct node {
	int number; // the job number
	int pid; // the process id of the a specific process
	char *command; //command of job
	struct node *next; // when another process is called you add to the end of the linked list
};

/* Add a job to the list of jobs
 */
void addToJobList(pid_t newPid, char *commandArg) {

	struct node *job = malloc(sizeof(struct node));

	//If the job list is empty, create a new head
	if (head_job == NULL) {
		job->number = 1;
		job->pid = newPid;

		//the new head is also the current node
		job->next = NULL;
		head_job = job;
		current_job = head_job;
	}

	//Otherwise create a new job node and link the current node to it
	else {

		job->number = current_job->number + 1;
		job->pid = newPid;

		current_job->next = job;
		current_job = job;
		job->next = NULL;
	}
	job->command = commandArg;
}

void jobs(){
	struct node *ptr = head_job;
	struct node *previous = NULL;
	int status;
	// iterate through job nodes, finding ones that are finished and then removing and print status, otherwise just print status
	while(ptr != NULL){
		int x = waitpid(ptr->pid,&status,WNOHANG);
		if(ptr == head_job){
			if(x != 0 /*== -1 || WIFEXITED(status)*/){
				head_job = ptr->next;
				printf("[%i]\t%s\tDone\n",ptr->number,ptr->command);
				//seg fault somewhere after here
				free(ptr);
				ptr = head_job;
				continue;
			}
			else{
				printf("[%i]\t%s\tRunning\n",ptr->number,ptr->command);
			}
		}
		else{
			if(x != 0/*== -1 || WIFEXITED(status)*/){
				previous -> next = ptr -> next;
				printf("[%i]\t%s\tDone\n",ptr->number,ptr->command);
				free(ptr);
				ptr = previous -> next;
				continue;
			}
			else{
				printf("[%i]\t%s\tRunning\n",ptr->number,ptr->command);
			}
		}
		previous = ptr;
		ptr = ptr -> next;
	}
}
// moves a job to foreground given its job number
void foreground(int n){
	// iterate through job nodes until job with given job number is found, then set currentPid so task can be killed, and job will be waited on by shell
	struct node *ptr = head_job;
	while(ptr != NULL){
		if(ptr -> number == n){
			current_job = ptr;
			currentPid = ptr->pid;
			int status;
			waitpid(currentPid, &status, WUNTRACED);
			break;
		}
		else{
			ptr = ptr -> next;
		}
	}
}

// handles sigint by killing current running foreground process if it is not shell
static void 
sigHandlerInt(int signo){
	if(signo == SIGINT && currentPid != 0){
		kill(currentPid,SIGKILL);
	}
}
// parses user command from stdin into arguments used by main method
int getcmd(char *prompt, char *args[], int *background, int *redirect){

	int length, i = 0;
	char *token, *loc;
	char *line = NULL;
	*redirect = 0;

	size_t linecap = 0;
	printf("%s", prompt);
	length = getline(&line, &linecap, stdin); 
	
	if (length <= 0) {
		exit(-1);
 	}
// Check if background is specified.. 
	if ((loc = index(line, '&')) != NULL) {
	 	 *background = 1;
	 	 *loc = ' ';
	}
	else
		 *background = 0;
	while ((token = strsep(&line, " \t\n")) != NULL) {
		for (int j = 0; j < strlen(token); j++)
			if (token[j] <= 32)
				token[j] = '\0';
		if (strlen(token) > 0)
			args[i++] = token;
		// check if output redirect is specified
		if (!strcmp(token,">"))
			*redirect = 1;	
	}
	free(line);
	return i; 
}

// initialize argument array to all NULL values
void initialize(char *args[]) {
	for (int i = 0; i < 20; i++) {
		args[i] = NULL;
	}
	return;
}

int main(void){
	// used to simulate delay in shell
	time_t now;
	srand((unsigned int) (time(&now)));

	// sets functions to handle signals e.g. sigHandlerInt to SIGINT, and SIG_IGN to SIGTSTP
	if(signal(SIGINT,sigHandlerInt) == SIG_ERR || signal(SIGTSTP,SIG_IGN) == SIG_ERR ){
		printf("\nCan't catch signal");
	}

	char *args[20];
// status variables for background/redirection
	int bg;
	int redirect;
	//	start in home directory
	chdir(getenv("HOME"));
	while(1) {
		initialize(args);
		bg = 0;
		int cnt = getcmd("\n>> ", args, &bg, &redirect);
		if(cnt <= 0){
			printf("Input error, please enter another command");
		}
		else{
			//declare pid variable
			pid_t pid;
			// if cp function specified by user, create child process
				if(!strcmp(args[0],"cp")){
				pid = fork();
				if(pid == 0){	
					// in child, execute command using execvp
					int w,rem;
					w = rand() % 10;
					rem=sleep(w);
					while(rem!=0){
						rem=sleep(rem);
					}
					execvp(args[0],args);
					perror("Execvp error");
				}
				else if(pid<0){
					perror("Fork error");
				}
				else{
					// if not background task, have shell wait until finished, and set currentPid to child pid, so child can be killed by SIGINT
					int status;
					if(bg == 0){
						currentPid = pid;
						waitpid(pid, &status, WUNTRACED);
					}
					// add to job list if background task
					else{
						addToJobList(pid,args[0]);
					}
				}
			}
			// similar to cp, except now can input redirect
			else if(!strcmp(args[0],"ls") || !strcmp(args[0], "cat")){
				if(redirect == 1){
					// set appropriate file descriptors, but also keep a copy of old stdout fd to restore
					int stdout_copy = dup(1);
					close(1);
					pid = fork();
					if(pid == 0){
						int w,rem;
						w = rand() % 10;
						rem=sleep(w);
						while(rem!=0){
							rem=sleep(rem);
						}
						// open output file for redirection
						for(int i = 0;i<20;i++){
							if(!strcmp(args[i],">")){
								int output = open(args[i+1],O_RDWR|O_CREAT|O_TRUNC, 0666);
								args[i] = NULL;
								args[i+1] = NULL;
								break;
							}
						}		
						// execute command which will be directed into file
						execvp(args[0],args);
					}
					else if(pid < 0){
						perror("Fork Error");
					}
					else{
						// restore file descriptors and set currentPid 
						int status;
						if(bg == 0){
							currentPid = pid;
							waitpid(pid, &status, WUNTRACED);
							close(1);
							dup(stdout_copy);
							close(stdout_copy);
						}
						// add to job list if background specified and restore file descriptors after child process finished
						else{
							addToJobList(pid,args[0]);
							waitpid(pid,&status,WNOHANG);
							if(WIFEXITED(status)){
								close(1);
								dup(stdout_copy);
								close(stdout_copy);
							}
						}
					}
				}
				// if no input redirection, just execute command using execvp() and fork()
				else{
					pid = fork();
					if(pid == 0){
						int w,rem;
						w = rand() % 10;
						rem=sleep(w);
						while(rem!=0){
							rem=sleep(rem);
						}
						execvp(args[0], args);
						perror("Execvp error");
					}
					else if(pid<0){
						perror("Fork error");
					}
					else{
						int status;
						if(bg == 0){
							currentPid = pid;
							waitpid(pid, &status, WUNTRACED);
						}
						else{
							addToJobList(pid,args[0]);
						}
					}
				}  
			}
			// if user enters cd command, change directory using chdir() system call and print information after changing directory
			else if(!strcmp(args[0],"cd")){
					if(args[1] == NULL){
						chdir(getenv("HOME"));
						printf("Moved to home directory");
					}
					else{
						int cdReturn = chdir(args[1]);
						if(cdReturn== 0){
							printf("cd successful");
						}
						else{
							printf("cd failed with return code =%d",cdReturn);
						}
					}  
			}
			// if user enters fg command, call foreground function
			else if(!strcmp(args[0],"fg")){
				if(args[1] != NULL){
					foreground(atoi(args[1]));
				}
			}
			// if user enters jobs command, call jobs function
			else if(!strcmp(args[0],"jobs")){
				jobs();
			}
			// exit if user enters exit command
			else if(!strcmp(args[0],"exit")){
				printf("Quitting shell...\n");
				exit(0);
			}
		}
	}
}