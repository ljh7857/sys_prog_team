#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<fcntl.h>
#include<dirent.h>
#include<termios.h>
#include<signal.h>
#include<ctype.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/wait.h>

#define SLEEPTIME 2
#define MAXTRIES 3
#define CAMPERPATH "/home/camper/teamp"			//camper
#define TRASHPATH "/home/camper/teamp/trash"		//camper/trash
#define oops(message,num) {	perror(message); exit(num);	} 

int check_the_trash(void);
void trash_exec(void);
void make_pwd(void);

int check_the_file(void);
void initial_arglist(void);
void make_arglist(char *);

void tty_mode(int);
void set_terminal(void);
char get_response(int);
void get_decision(int);
int get_char(int);

void handler(int);

ino_t getInode(char *);
void dirPath(ino_t);
void subdirPath(ino_t, char *, int);

void printNowLocat();	//쉘처럼 현재 위치를 출력

void recover_trash_file();	//복구하기

char *path;
char *arglist[BUFSIZ];
char file_path[BUFSIZ];
int idx;
int fd;
int flag;

char now_locate[BUFSIZ];
char before_locate[BUFSIZ];

/*
	- pwd issue : pwd file 생성, 어떤 디렉토리에 있는 file이라도 path와 filename을 pwd.txt file에 저장.
				- execvp사용으로, trash 디렉토리로 move.
				- but 둘다 같이 안; 이유는 잘 모르겠음.. 각각의 동작은 잘 수행됨.
	- Recover : 복원 구현
	- 코드 합치기..
*/
int main(int argc, char *argv[]) {
	tty_mode(0);
	printf("--------------------------------------------------------------------------------\n");
	puts("| Instructions");
	printf("| You can use the [ ~@ ] command to verify that the Recycle Bin is working.\n|  - If there is no recycle bin, you can create it.\n|  - There are no additional options\n");
	puts("| ");
	printf("| You can use the [ ~b ] command to flush that all contents in Recycle Bin.\n| - there are no additional options. \n");
	puts("| ");
	printf("| You can use the [ ~~ ] command to go to the Recycle Bin shortcut.\n| - there are no additional options. \n");
	puts("| ");
	printf("| You can use the [ ~! ] command to Return to the path before the Recycle Bin shortcut.\n| - there are no additional options. \n");
	puts("| ");
	printf("| You can use the [ rm ] command to decide whether to use the Recycle Bin or not.\n|  - There are additional options. \n");
	puts("| ");
	printf("| You can use the [ re ] command to restore files from the Recycle Bin. \n|  - There are no additional options.\n");
	puts("| ");
	printf("| Other commands and options are also supported. \n");
	puts("| ");
	printf("| You can end the process using SIGNAL.");
	puts("");

	printf("--------------------------------------------------------------------------------\n\n");

	signal(SIGINT, handler);
	signal(SIGKILL, SIG_DFL);
	while (1) {
		char argString[BUFSIZ];

		//pwd
		printNowLocat();

		if (strcmp(now_locate, TRASHPATH))
			strcpy(before_locate, now_locate);		//Save directory patn that before shortcut

		fgets(argString, BUFSIZ, stdin);
		argString[strlen(argString) - 1] = '\0';

		if (!strcmp(argString, "~@")) {
			flag = check_the_trash();
			char ch;
			if (flag == -1) {
				tty_mode(0);
				set_terminal();
				ch = get_response(MAXTRIES);
				tty_mode(1);

				if (ch == 'n')
					puts("If you don't create a Recycle Bin, you can't use the -t option");
				else
					puts("Now you can send files to the Recycle Bin.");
			}
		}
		else if (!strcmp(argString, "~~")) {	 //Options for the Trashbin dir Shortcut
			flag = check_the_trash();
			if (flag == -1) {
				puts("You must first create a recycle bin");

				continue;
			}
			else
				chdir(TRASHPATH);
		}		//shortcut
		else if (!strcmp(argString, "~!")) {	//Return to the path before the shortcut
			if (before_locate != NULL)
				chdir(before_locate);
		}
		else if (!strcmp(argString, "~b")) { // Option for flush Trashbin
			flag = check_the_trash();

			if (flag == -1) {
				puts("You must first create a recycle bin");

				continue;
			}
			else {
				int pid_b;
				pid_b = fork();
				if (pid_b > 0) {
					wait(NULL);
					mkdir(TRASHPATH, 0755);
					puts("You have successfully flushed Recycle Bin.");
				}
				else {
					execlp("rm", "rm", "-r", TRASHPATH, NULL);
				}
			}
		}
		else {
			initial_arglist();
			make_arglist(argString);

			if (!strcmp(*arglist, "rm")) {		//delete or trash
				int pid;

				getcwd(file_path, BUFSIZ);
				if (!strcmp(file_path, TRASHPATH)) {
					for (int i = 1; i < idx; i++)
						unlink(arglist[i]);
				}
				else {

					if ((pid = fork()) == -1)
						oops("cannot fork", 1);

					if (pid > 0) {
						wait(NULL);

						continue;
					}
					else {
						tty_mode(0);
						set_terminal();
						get_decision(MAXTRIES);
						tty_mode(1);
					}
				}
			}
			///Do we have to deal with rmdir too???
			else if (!strcmp(*arglist, "re")) {		//recover
				//puts(argString);
				if (check_the_trash() == -1) {
					printf("No Recycle Bin\n");
					continue;
				}

				recover_trash_file();
				/*
				pwd file에서 복원할 file의 이름을 file structure array에서 비교하여 해당 file이 있을 시 pwd file에서
				filecount 숫자번째 줄의 경로를 받아옴.
				해당 경로에 file mv! (exec(mv))
				*/
			}
			else if (!strcmp(*arglist, "cd")) {
				getcwd(file_path, BUFSIZ);
				strcpy(file_path, arglist[1]);
				chdir(file_path);
			}
			else {									//other options
				int pid;

				if ((pid = fork()) == -1)
					oops("cannot fork", 1);

				if (pid > 0) {
					wait(NULL);

					continue;
				}
				else {
					execvp(*arglist, arglist);
					break;
				}
			}
		}
	}


	return 0;
}

void recover_trash_file() {
	//int i;

	char file_read[BUFSIZ];
	char file_name[BUFSIZ];
	char dir_path[BUFSIZ];
	char trash_path[BUFSIZ];

	char *token;
	char *delimiter = " ";

	//re file1 file2
	//pwd.txt
	//ssize_t size;

	make_pwd();
	FILE *fp = fdopen(fd, "r");

	if (fp == NULL)
		oops("file open", 1);

	for (int i = 1; i < idx; i++) {
		while (!feof(fp)) {		//handling
			if (fgets(file_read, BUFSIZ, fp) == NULL) {
				printf("there is no such file : %s\n", arglist[i]);
				break;
			}
			file_read[strlen(file_read) - 1] = '\0';

			token = strtok(file_read, delimiter);
			strcpy(file_name, token);

			token = strtok(NULL, delimiter);
			strcpy(dir_path, token);
			strcat(dir_path, "/");
			strcat(dir_path, file_name);

			strcpy(trash_path, TRASHPATH);
			strcat(trash_path, "/");
			strcat(trash_path, file_name);

			if (!strcmp(arglist[i], file_name)) {
				rename(trash_path, dir_path);
				puts("recover successfully!");
				break;
			}
		}
		fseek(fp, 0, SEEK_SET);
	}
}

void make_arglist(char *argString) {
	char *token;
	char *delimiter = " ";

	token = strtok(argString, delimiter);
	while (token != NULL) {
		arglist[idx++] = token;
		token = strtok(NULL, delimiter);
	}
	arglist[idx] = NULL;

	/*
	printf("idx : %d\n", idx);
	for (int i = 0; i < idx; i++)
		printf("arglist[%d] : %s\n", i, arglist[i]);
	*/
}
void initial_arglist() {
	for (int i = 0; i < BUFSIZ; i++)
		arglist[i] = NULL;
	idx = 0;
}

int check_the_trash(void) {
	DIR *dir_info;
	struct dirent *dir_entry;

	dir_info = opendir(CAMPERPATH);

	if (dir_info == NULL)
		oops("open", 1);

	while ((dir_entry = readdir(dir_info))) {
		if (strcmp(dir_entry->d_name, "trash") == 0) {
			puts("There is Recycle Bin");

			return 0;
		}
	}

	closedir(dir_info);
	return -1;
}

int check_the_file(void) {
	DIR *dir_info;
	struct dirent *dir_entry;

	dir_info = opendir(CAMPERPATH);

	if (dir_info == NULL)
		oops("open", 1);

	while ((dir_entry = readdir(dir_info))) {
		if (strcmp(dir_entry->d_name, "pwd.txt") == 0) {
			puts("There is pwd.txt");

			closedir(dir_info);
			return 0;
		}
	}

	closedir(dir_info);
	puts("There is no pwd.txt");
	puts("Create pwd.txt");

	return -1;
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
		printf("/%s", locate);
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

void printNowLocat() {

	getcwd(now_locate, BUFSIZ);

	if (getInode(".") == getInode("..")) {
		printf("/");
	}//for root dir

	dirPath(getInode("."));
	printf("(^_^) : ");

	chdir(now_locate);
}

void tty_mode(int how) {
	static struct termios original_mode;

	if (how == 0)
		tcgetattr(0, &original_mode);
	else
		tcsetattr(0, TCSANOW, &original_mode);
}

void set_terminal(void) {
	struct termios ttystate;

	tcgetattr(0, &ttystate);
	ttystate.c_lflag &= ~ICANON;
	ttystate.c_lflag &= ~ECHO;
	ttystate.c_cc[VMIN] = 1;

	tcsetattr(0, TCSANOW, &ttystate);
}

char get_response(int tries) {
	char response;

	puts("There is no Recycle bin. Do you want to create it? y/n");

	response = tolower(get_char(0));

	switch (response) {
	case 'y':
		mkdir(TRASHPATH, 0755);
		make_pwd();

		puts("You have successfully created!");

		return response;

	case 'n':
		puts("Do not create Recycle Bin");

		return response;
	}
	return 0;
}

void get_decision(int tries) {
	char decision;
	int wr;

	puts("Do you want to delete the file? Or throw it in the trash?\n - delete : d, throw : t");
	decision = tolower(get_char(1));

	switch (decision) {
	case 't': {
		tty_mode(1);

		puts(file_path);		//file_path

		trash_exec();

		//chdir(file_path);
		char temp_string[BUFSIZ];
		printf("idx : %d\n", idx);
		for (int i = 1; i < idx - 1; i++) {
			if (*arglist[i] != '-') {
				strcpy(temp_string, arglist[i]);
				printf("arglist[%d] : %s\n", i, arglist[i]);

				strcat(temp_string, " ");
				strcat(temp_string, file_path);
				temp_string[strlen(temp_string)] = '\n';

				printf("temp_string : %s\n", temp_string);

				if ((wr = write(fd, temp_string, strlen(temp_string))) == -1) {
					tty_mode(1);
					oops("write", 1);
				}
			}
		}

		tty_mode(1);

		return;
	}
	case 'd':
		puts("delete anyway");
		tty_mode(1);

		execvp(*arglist, arglist);

		return;
	}
}

int get_char(int flag_) {
	int ch;
	int count = 0;

	if (flag_ == 0) {
		while (((ch = getchar()) != EOF)) {
			if (strchr("yYnN", ch))
				return ch;
			else
				puts("you can only enter y or n in any case\n\t-\tIt doesn't matter if it's capital letter or not.");

			if (++count >= MAXTRIES) {
				puts("Shut down for security reasons.");

				tty_mode(1);
				exit(1);
			}
		}
	}
	else {
		while (((ch = getchar()) != EOF)) {
			if (strchr("dDtT", ch))
				return ch;
			else
				puts("you can only enter d or t in any case\n\t-\tIt doesn't matter if it's capital letter or not.");

			if (++count >= MAXTRIES) {
				puts("Shut down for security reasons.");

				tty_mode(1);
				exit(1);
			}
		}
	}
	return 0;
}

void handler(int signum) {
	tty_mode(1);
	puts("");
	puts("shutdown");

	//프로세스를 종료하면 더이상 휴지통을 사용하지 못합니다.
	//- 휴지통을 삭제하시겠습니까?
	//		- yes : 휴지통 디렉토리 전체 삭제 -> 다시 프로세스 실행 시 휴지통여부 ㄴㄴ
	//		- no : 휴지통 디렉토리 보존 -> 다시 프로세스 실행 시 휴지통 여부 ㅇㅇ
	exit(1);
}


void trash_exec(void) {
	flag = check_the_trash();

	if (flag == -1) {
		tty_mode(0);
		set_terminal();
		get_response(MAXTRIES);
		tty_mode(1);
	}
	else {
		arglist[0] = "mv";
		arglist[idx++] = TRASHPATH;
		arglist[idx] = NULL;

		int pid = 0;

		if ((pid = fork()) == -1)
			oops("fork", -1);

		if (pid > 0) {
			wait(NULL);
			//exit(0);

			return;
		}
		else {
			execvp(*arglist, arglist);

			oops("execvp", 1);
		}
	}
}

void make_pwd(void) {
	chdir(CAMPERPATH);
	//pwd.txt file을 만들긴 함.

	flag = check_the_file();

	if (flag == -1) {
		//creat
		if ((fd = creat("pwd.txt", 0644)) == -1) {
			tty_mode(1);
			oops("create", 1);
		}
	}
	else {
		if ((fd = open("pwd.txt", O_RDWR | O_APPEND, S_IWUSR | S_IRUSR)) == -1) {
			tty_mode(1);
			oops("open", 1);
		}
	}
	chdir(file_path);
}
