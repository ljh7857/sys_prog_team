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

#define SLEEPTIME 1
#define SLEEPTRIES 5
#define MAXTRIES 3
#define CAMPERPATH "/home/camper/"			//camper
#define TRASHPATH "/home/camper/trash"		//camper/trash
#define oops(message,num) {	perror(message); exit(num);	} 
#define BEEP putchar('\a');

int check_the_trash(void);
void trash_exec(void);

int check_the_file(void);
void initial_arglist(void);
void make_arglist(char *);

void tty_mode(int);
void set_terminal(void);
char get_response(int,int);
void get_decision(int);
int get_char(int);
void set_nodelay_mode(void);
void handler(int);

ino_t getInode(char *);
void dirPath(ino_t);
void subdirPath(ino_t, char *, int);

void printNowLocat();	//쉘처럼 현재 위치를 출력

char *path;
char *arglist[BUFSIZ];
char file_path[BUFSIZ];
int idx;

char nowloc[256];
char befoloc[256];

/*
	- pwd issue : pwd file 생성, 어떤 디렉토리에 있는 file이라도 path와 filename을 pwd.txt file에 저장.
				- execvp사용으로, trash 디렉토리로 move.
				- but 둘다 같이 안; 이유는 잘 모르겠음.. 각각의 동작은 잘 수행됨.
	- Recover : 복원 구현
	- 코드 합치기..
*/
int main(int argc, char *argv[]) {
	int pid;
	tty_mode(0);
	printf("--------------------------------------------------------------------------------\n");
	puts("| Instructions");
	printf("| You can use the [ ~@ ] command to verify that the Recycle Bin is working.\n|  - If there is no recycle bin, you can create it.\n|  - There are no additional options\n");
	puts("| ");
	printf("| You can use the [ ~b ] command to flush that all contents in Recycle Bin.\n|  - there are no additional options. \n");
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
		//char wannago[256];	//안써서 주석처리

		//pwd
		printNowLocat();
		if(strcmp(nowloc, TRASHPATH))
			strcpy(befoloc, nowloc);

		fgets(argString, BUFSIZ, stdin);
		if(argString[0] == '\n' || argString[0] == ' ')
			continue;

		argString[strlen(argString) - 1] = '\0';

		if (!strcmp(argString, "~@")) {
			int flag = check_the_trash();
			char ch;
			if (flag == -1) {
				tty_mode(0);
				set_terminal();
				//set_nodelay_mode();
				ch = get_response(MAXTRIES,SLEEPTRIES);
				tty_mode(1);

				if (ch == 'n') 
					puts("If you don't create a Recycle Bin, you can't use the -t option");
				else if(ch == 'y')
					puts("Now you can send files to the Recycle Bin.");
			}
		}
		else if(!strcmp(argString, "~~")){
			chdir(TRASHPATH);
		}//shortcut
		else if(!strcmp(argString, "~!")){
			if(befoloc != NULL)
				chdir(befoloc);
		}
		else if (!strcmp(argString,"~b")){
			pid = fork();
			if(pid>0){
				wait(NULL);
				mkdir(TRASHPATH, 0755);
				puts("You have successfully flushed Recycle Bin.");
			}
			else{
				execlp("rm","rm","-r",TRASHPATH,NULL);
		                
			}
		
		}//shortcut
		else {
			initial_arglist();
			make_arglist(argString);

			if (!strcmp(*arglist, "rm")) {		//delete or trash
				int pid;

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
			///Do we have to deal with rmdir too???
			else if (!strcmp(*arglist, "re")) {		//recover
				//puts(argString);
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
					break;//to kill the error execvp
				}
			}
		}
	}


	return 0;
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
	//char nowloc[256];//전역

	getcwd(nowloc, 256);

	if (getInode(".") == getInode("..")) {
		printf("/");
	}//for root dir

	dirPath(getInode("."));
	printf("(^_^) : ");

	chdir(nowloc);
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

char get_response(int tries,int sleeptry) {
	char response;
	puts("There is no Recycle bin. Do you want to create it? y/n");

	while(1){
	//	sleep(SLEEPTIME);
		response = tolower(get_char(0));
	
       		 if(response == 'y'){
                mkdir(TRASHPATH, 0755);
                puts("You have successfully created!");
                puts("Now you can send files to the Recycle Bin.");
                return response;
		}
      		  if(response== 'n'){
//                puts("Do not create Recycle Bin");

                return response;
       		 }
		
	/*	if(sleeptry--==0){
			printf("timeout\n");
			return 'p';
		}
		BEEP;*/
	}
}



void set_nodelay_mode(void){
	int termflags;
	termflags=fcntl(0,F_GETFL);
	termflags |= O_NDELAY;
	fcntl(0,F_SETFL,termflags);
}
void get_decision(int tries) {
	char decision;
	int fd;
	//int wr;	//안써서 주석처리

	puts("Do you want to delete the file? Or throw it in the trash?\n - delete : d, throw : t");

	decision = tolower(get_char(1));

	switch (decision) {
	case 't': {
		getcwd(file_path, BUFSIZ);
		puts(file_path);		//file_path
		
		//chdir을 쓰지않고, file 만들기..?
		chdir(CAMPERPATH);
		//pwd.txt file을 만들긴 함.

		tty_mode(1);

		int flag = check_the_file();
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

		/*
		char temp_string[BUFSIZ];
		for (int i = 1; i <= idx; i++) {
			if (*arglist[i] == '-')
				continue;
			else{
				strcpy(temp_string, arglist[i]);
				//printf("arglist[%d] : %s\n", i, arglist[i]);
				strcat(temp_string, " ");
				strcat(temp_string, file_path);
				temp_string[strlen(temp_string)] = '\n';
				//printf("temp_string : %s\n", temp_string);
				if ((wr = write(fd, temp_string, strlen(temp_string))) == -1) {
					tty_mode(1);
					oops("write", 1);
				}
			}
		}
		*/
		
		///이거 이후에는 왜 실행안되냐..
		trash_exec();

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

int get_char(int flag) {
	int ch;
	int count = 0;

	if (flag == 0) {
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

	int flag = check_the_trash();

	if (flag == -1) {
		tty_mode(0);
		set_terminal();
		get_response(MAXTRIES,SLEEPTRIES);
		tty_mode(1);
	}
	else {
		arglist[0] = "mv";
		arglist[idx++] = TRASHPATH;
		arglist[idx] = NULL;

		execvp(*arglist, arglist);
	}
}
