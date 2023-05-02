#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define bignum	100
#define DISCONNECT_MESSAGE "!DISCONNECT"

// compile with gcc -std=c11 -Wall -pedantic -Werror -o rake-c rake-c.c or use make

extern char **strsplit(const char *line, int *nwords);
extern void free_words(char **words);
void delete(char string[], char substr[]);

char *DEFAULT_PORT;
char *GIVEN_PORT;
char *HOSTS[bignum];
char *ACTIONS[bignum];
char *SETNAMES[bignum];

char *filecontents[bignum];
int linecount = 0;
ssize_t line_size;

typedef struct ACTION {

	char	*cmd;
	bool	remote;
	int		filecount;
	char	*filenames[];

} action_t;

typedef struct COMMANDS {

	int	actioncount;
	action_t	*action;

} commands_t;

typedef struct SETNAME {

	int	actionsetcount;
	commands_t		*commands;

} setname_t;

// Function to delete substr[] from a given string[].
void delete (char string[], char substr[]){
	int i = 0;
	int string_length = strlen(string);
	int substr_length = strlen(substr);

	while(i < string_length){
		if(strstr(&string[i], substr) == &string[i]){
			string_length -= substr_length;
			for(int j = i; j < string_length; ++j){
				string[j] = string[j+substr_length];
			}
		}
		else i++;
	}
	string[i] = '\0';
}

// Function to read rakefile and store lines in character array filecontents[].
int readline(){
	char *line_buf = NULL;
	size_t line_buf_size = 0;
	ssize_t line_size;

	FILE *fp = fopen("Rakefile", "r");
	if (!fp){
		return EXIT_FAILURE;
	}

	/* Loop through until we are done with the file. */
	while ((line_size = getline(&line_buf, &line_buf_size, fp)) != -1){
		if(line_buf[0] != '#'){
			/* Increment our line count */
			filecontents[linecount] = malloc(strlen(line_buf) + 1);
			strcpy(filecontents[linecount], line_buf);
			linecount++;
		}
	}

	for(int i = 0; i < linecount; ++i){
		printf("%s\n", filecontents[i]);
	}

	/* Free the allocated line buffer */
	free(line_buf);
	line_buf = NULL;

	/* Close the file now that we are done with it */
	fclose(fp);

	return EXIT_SUCCESS;
}

// Function to connect c client to server.
int tcp_connect(){
	int portInt = strtol(DEFAULT_PORT, NULL, 10);
	int client_fd;
	struct sockaddr_in server;
	char buffer[1024] = "echo hello";			// data to be sent from client
	char server_buffer[1024];	                // data to be received from server

	client_fd = socket(AF_INET, SOCK_STREAM, 0);

	server.sin_addr.s_addr = inet_addr("127.0.0.1");		
	server.sin_family = AF_INET;
	server.sin_port = htons(portInt);

	// assign buffer to parsed rakefile

	if(connect(client_fd, (struct sockaddr*)&server, sizeof(server)) < 0){
		perror("[CLIENT] CONNECTION FAILED!\n");
		return 1;
	}
	else{
		printf("[CLIENT] CONNECTED!\n");

		if( send(client_fd, buffer, strlen(buffer), 0) < 0){
			puts("[CLIENT] DATA FAILED TO SEND!\n");
		}

		memset(server_buffer, '\0', sizeof(server_buffer));
		recv(client_fd, server_buffer, sizeof(server_buffer), 0);
		printf("[CLIENT] Message from server: %s\n", server_buffer);

		if( send(client_fd, DISCONNECT_MESSAGE, strlen(DISCONNECT_MESSAGE), 0) < 0){
			puts("[CLIENT] DATA FAILED TO SEND!\n");
		}
		printf("[CLIENT] CONNECTION CLOSED, GOODBYE!\n");
	}
	close(client_fd);
	return 0;
}

int main(void)
{   
	FILE * fp;
    char * line = NULL;
    size_t len = 0;
    ssize_t read;

	// int reading = readline();
	// parse_file(filecontents);
	// return reading;

	int status = 0;

	action_t *reqfiles = (action_t*)malloc(sizeof(action_t));

	commands_t *commands = (commands_t*)malloc(sizeof(commands_t));
	commands->action = reqfiles;

	setname_t *setnames = (setname_t*)malloc(sizeof(setname_t));
	setnames->commands = commands;

    fp = fopen("Rakefile", "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);
    
    while ((read = getline(&line, &len, fp)) != -1) {
        int nwords;
    	char **words = strsplit(line, &nwords);

		// Ignore any line that starts with #.
    	if(strchr(line, '#') != NULL){
    		continue;
    	}

		// Stores port number in variable PORT
		if(strstr(line, "PORT")){
			DEFAULT_PORT = words[2];		// Assuming every port number is found at the end of the port line.
			printf("%s\n", DEFAULT_PORT);
			continue;
		}

		// Stores hosts in variable HOSTS
		if(strstr(line, "HOSTS")){
			for(int i = 2; i < nwords; ++i){
				HOSTS[i] = words[i];
				if(strstr(HOSTS[i], ":") == NULL){	// Adds PORT to the end of HOSTS that do NOT have a specified port number.
					strcat(HOSTS[i], ":");
					strcat(HOSTS[i], DEFAULT_PORT);
				}
				printf("%s\n", HOSTS[i]);
			}
			continue;
		}

		printf("%s", line);

		// Separate lines into one tab or two-tabbed lines.
		if(strstr(line, "\t\t") != NULL){
			for(int i = 1; i < nwords; ++i){
				reqfiles->filenames[i] = words[i];
				reqfiles->filecount += 1;
				printf("\t[%i] %s \n", i, reqfiles->filenames[i]);
			}
			printf("%i files needed\n", reqfiles->filecount);
		}
		else if(strstr(line, "\t") != NULL){
			reqfiles->filecount = 0;
			
			if(strstr(line, "remote") != NULL){
				reqfiles->remote = true;
				delete(line, "remote-");
				printf("%s\n", line);
			}
			if(strstr(line, "requires") == NULL){
					if(strstr(line, "remote") == NULL){
						status = system(line);		        // Try to run non-remote commands in client.	
					}
					int exitstat = status/256;
					reqfiles->cmd = line;
					printf("%s completed with exit status %i\n", reqfiles->cmd, exitstat);
					//printf("cmd is %s\n", reqfiles->cmd);
			}

            // Realloc memory to create enough memory for new size.
			commands->action->cmd = realloc(&commands->action->cmd,sizeof(line));
			commands->action->cmd = line;
			printf("action is cmd: %s\n", commands->action->cmd);
			for(int i = 0; i < nwords; ++i){
				commands->actioncount += 1;
				// ACTIONSET[i] = words[i];
				// printf("\tactions are: [%i] %s \n", i, ACTIONSET[i]);
			}
			printf("num of actionsets: %i\n", commands->actioncount);
			printf("true or false: %d\n\n", reqfiles->remote);

		}
		else{
			reqfiles->remote = false;
			char *SETNAME[nwords];
			for(int i = 0; i < nwords; ++i){
				if(strstr(line, "actionset") != NULL){
					setnames->actionsetcount += 1;
				}
				SETNAME[i] = words[i];
				printf("\t[%i] %s \n", i, SETNAME[i]);
			}
		}
		commands->actioncount = 0;
		free_words(words);
    }

	printf("num of set names: %i\n", setnames->actionsetcount);
	printf("cmd is %s\n", reqfiles->cmd);
    
	// need to free up the malloc-ed structures somehow
	// free(reqfiles->cmd);
	// free(reqfiles);

	int connected = 0;
	connected = tcp_connect();
	return connected;

    fclose(fp);
    if (line)
        free(line);
    exit(EXIT_SUCCESS);
}