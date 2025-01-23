#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <limits.h>

#define MAX_INPUT 1024
#define MAX_ARGS 100

char custom_path[MAX_INPUT] = "/usr/bin:/bin:/sbin";
char prompt[MAX_INPUT] = "\\w$ ";

void updatePrompt() {
	char cwd[PATH_MAX];
	if (getcwd(cwd, sizeof(cwd))) {
		snprintf(prompt, sizeof(prompt), "%s$ ", cwd);
	}
	else {
		perror("getcwd");
	}
}

void inputPrep(char *input, char **args) {
	char *tkn = strtok(input, " \t\n");
	int i = 0;
	while (tkn) {
		args[i++] = tkn;
		tkn = strtok(NULL, " \t\n");
	}
	args[i] = NULL;
}

char *searchPath(char *cmd) {
	static char fullPath[MAX_INPUT];
	char *path = strtok(custom_path, ":");
	while (path) {
		snprintf(fullPath, sizeof(fullPath), "%s/%s", path, cmd);
		if (access(fullPath, X_OK) == 0) {
			return fullPath;
		}
		path = strtok(NULL, ":");
	}
	return NULL;
}

void executeCommand(char *cmd, char **args) {
	pid_t pid = fork();
	if (pid == 0) {
		char *fullCMD = searchPath(cmd);
		if (fullCMD) {
			execv(fullCMD, args);
		}
		else {
			printf("Command not found: %s\n", cmd);
		}
		exit(1);
	}
	else if (pid > 0) {
		wait(NULL);
	}
	else {
		perror("fork\n");
	}
}

void handleRedirection(char *input) {
	char *infile = NULL, *outfile = NULL;
	char *cmd = strtok(input, ">");
	if (strchr(cmd, '<')) {
		cmd = strtok(cmd, "<");
		infile = strtok(NULL, "<");
	}
	outfile = strtok(NULL, ">");
	if (infile)
		infile = strtok(infile, " \t\n");
	if (outfile)
		outfile = strtok(outfile, " \t\n");
		
	int in_fd = -1, out_fd = -1;
	if (infile) {
		in_fd = open(infile, O_RDONLY);
		if (in_fd < 0) {
			printf("Error opening file: %s\n", infile);
			return;
		}
	}
	if (outfile) {
		out_fd = open(outfile, O_CREAT | O_WRONLY | O_TRUNC, 0644);
		if (out_fd < 0) {
			printf("Error opening file: %s\n", outfile);
			if (in_fd >= 0)
				close(in_fd);
			return;
		}
	}
	
	pid_t pid = fork();
	if (pid == 0) {
		if (in_fd >= 0) {
			dup2(in_fd, STDIN_FILENO);
			close(in_fd);
		}	
		if (out_fd >= 0) {
			dup2(out_fd, STDOUT_FILENO);
			close(out_fd);
		}	
		char *args[MAX_ARGS];
		inputPrep(cmd, args);
		if (args[0]) {
			char *fullCMD = searchPath(args[0]);
			if (fullCMD) {
				execv(fullCMD, args);
			}
			else {
				printf("Command not found: %s\n", args[0]);
			}
		}
		exit(1);
	}
	else if (pid > 0) {
		if (in_fd >= 0)
			close(in_fd);
		if (out_fd >= 0)
			close(out_fd);
		wait(NULL);
	}
	else {
		perror("fork");
	}
}

void handle_cd(char **args) {
	if (!args[1]) {
		fprintf(stderr, "cd: missing argument\n");
		return;
	}
	if (chdir(args[1]) != 0) {
		perror("cd");
	}
}

int main() {
	char input[MAX_INPUT];
	while(1) {
		updatePrompt();
		printf("%s",prompt);
		if (fgets(input, MAX_INPUT, stdin) == NULL) {
			printf("\n");
			break;
		}
		input[strcspn(input, "\n")] = 0;
		if (strcmp(input, "exit") == 0) {
			break;
		}
		if (strncmp(input, "PATH=", 5) == 0) {
			strncpy(custom_path, input + 5, sizeof(custom_path));
			continue;
		}
		if (strncmp(input, "PS1=", 4) == 0) {
			char *newPrompt = strchr(input, "=") + 1;
			if (newPrompt) {
				if (strcmp(newPrompt, "\"\\w$\"") == 0) {
					strcpy(prompt, "\\w$ ");
				}
				else {
					snprintf(prompt, sizeof(prompt), "%s ", newPrompt + 1);
					size_t len = strlen(prompt);
					if (len > 0 && prompt[len - 1] == '"') {
						prompt[len - 1] = '\0';
					}
				}
			}
			continue;
		}
		
		if (strchr(input, '>') || strchr(input, '<')) {
			handleRedirection(input);
		}
		else {
			char *args[MAX_ARGS];
			inputPrep(input, args);
			if (args[0]) {
				if (strcmp(args[0], "cd") == 0)
					handle_cd(args);
				else
					executeCommand(args[0], args);
			}
		}
	}
	return 0;
}
