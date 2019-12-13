#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<fcntl.h>
#include<dirent.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/wait.h>

ino_t getInode(char *);
void dirPath(ino_t);
void subdirPath(ino_t, char *, int);

char path[BUFSIZ];

int main(int argc, char *argv[]) {

	DIR *dir_info;
	struct dirent *dir_entry;
	int flag = -1;

	dir_info = opendir("/home/camper");

	if (dir_info == NULL)
		perror("dir open");

	while (dir_entry = readdir(dir_info)) {
		if (strcmp(dir_entry->d_name, "trash") == 0) {
			flag = 1;

			break;
		}
	}

	if (flag == -1) {
		if (mkdir("/home/camper/trash", 0755) == -1) {
			perror("create error");

			exit(1);
		}
	}
	closedir(dir_info);

	int pid;

	if ((pid = fork()) == -1)
		perror("fork()");

	if (pid > 0) {
		wait(NULL);
		//pwd
		dirPath(getInode("."));
		printf("current directory path : ");
		puts(path);

	}


	else {
		//execvp
		char *token;
		char *delimiter = " ";
		char *arglist[BUFSIZ];
		char argString[BUFSIZ];

		int idx = 0;

		printf(" Use the rm command with the files to delete : ");
		fgets(argString, BUFSIZ, stdin);
		argString[strlen(argString) - 1] = '\0';
		token = strtok(argString, delimiter);
		while (token != NULL) {

			arglist[idx++] = token;

			token = strtok(NULL, delimiter);
		}
		arglist[idx] = NULL;

		/*
		for (int i = 0; i < idx; i++)
				printf("arglist[%d] : %s\n", i, arglist[i]);

		*/

		execvp(*arglist, arglist);

	}
	return 0;
}



ino_t getInode(char *filename) {
	struct stat info;

	if (stat(filename, &info) == -1) {
		perror(filename);

		exit(1);
	}

	return info.st_ino;
}

void dirPath(ino_t st_ino) {
	ino_t inode;
	char locate[BUFSIZ];

	if (getInode("..") != st_ino) {
		chdir("..");
		subdirPath(st_ino, locate, BUFSIZ);

		inode = getInode(".");

		dirPath(inode);

		strcat(path, "/");
		strcat(path, locate);
	}
}

void subdirPath(ino_t st_ino, char *string, int length) {
	DIR *dir_ptr;
	struct dirent *direntp;

	dir_ptr = opendir(".");

	if (dir_ptr == NULL) {
		perror(".");

		exit(1);
	}

	while ((direntp = readdir(dir_ptr)) != NULL) {
		if (direntp->d_ino == st_ino) {
			strncpy(string, direntp->d_name, length);
			string[length - 1] = '\0';

			closedir(dir_ptr);

			return;
		}
	}

	printf("dont found");
	exit(1);
}
