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
#define CAMPER "/home/camper"
#define TRASHPATH "/home/camper/trash"
#define oops(message,num) {	perror(message); exit(num);	} 

ino_t getInode(char *);
void dirPath(ino_t);
void subdirPath(ino_t, char *, int);

void check_the_trash(void);
void make_arglist(char *);

void tty_mode(int);
void set_terminal(void);
void get_response(int);
int get_char(void);

void handler(int);

char *arglist[BUFSIZ];
char *filelist[BUFSIZ];		//삭제 file들의 이름을 저장할 배열
char *path;


//현위치 정보를 출력
void printNowLocat();	//쉘처럼 현재 위치를 출력
ino_t get_inode(char*);
void printpathto(ino_t);
void inum_to_name(ino_t, char*, int);


/*	Topics to cover	
		- chdir issue : Can not chdir with exec function. we should handle this 
		- pwd file issue : Should we Create pwd file to save path of current file? or there are other ways?
		- Recycle Bin issue : We need to handle emptying the Recycle Bin and deleting files in the Recycle Bin.
		- empty issue : If an empty command is entered, segmentation fault is triggered.
*/
int main(int argc, char *argv[]) {


	printf("####################################################################\n");
	puts("| Instructions");
	printf("| You can use the [ ~@ ] command to verify that the Recycle Bin is working.\n|  - If there is no recycle bin, you can create it.\n|  - There are no additional options\n");
	puts("| ");
	printf("| You can use the [ rm ] command to decide whether to use the Recycle Bin or not.\n|  - There are additional options. \n");
	puts("| ");
	printf("| You can use the [ re ] command to restore files from the Recycle Bin.\n|  - There are no additional options.\n");
	puts("| ");
	printf("| Other commands and options are also supported.\n");
	puts("| ");
	printf("| You can end the process using SIGNAL");
	puts("");

	printf("####################################################################\n\n");


	signal(SIGINT, handler);
	signal(SIGKILL, SIG_DFL);
	while (1) {
		char argString[BUFSIZ];
		int idx = 0;
		
		//pwd
		printNowLocat();

		fgets(argString, BUFSIZ, stdin);
		argString[strlen(argString) - 1] = '\0';

		if (!strcmp(argString, "~@")) 		//is exist the trash directory?
			check_the_trash();
		
		else {
			make_arglist(argString);

			if (!strcmp(*arglist, "rm")) {		//delete or trash
				puts("Do you want to delete the file? Or throw it in the trash?\n - delete : d, throw : t");

				char decision;
				scanf("%c", &decision);

				if (decision == 't') {
					dirPath(getInode("."));		//path

					//puts(path);			
					/*
					path를 pwd를 저장하는 file로 전달{
						file로는 한줄에 하나씩 보냄.
						삭제한 file 정보는 file structure array를 만들어 저장. 
							- filename, filenumber
						}
					
					fork()
					child - move to trash directory.(exec(mv)) if not exist trash directory, print error message and exit
					parent - wait until child process exit(), delete the file that in current directory(exec(rm))
					*/
				}
				else 
					execvp(*arglist, arglist);		//delete anyway	
			}
			else if (!strcmp(*arglist, "re")) {		//recover
				//puts(argString);
				/*
				pwd file에서 복원할 file의 이름을 file structure array에서 비교하여 해당 file이 있을 시 pwd file에서
				filecount 숫자번째 줄의 경로를 받아옴.
				해당 경로에 file mv! (exec(mv))		
				*/
			}
			else									//other options
				execvp(*arglist, arglist);
		}
	}


	return 0;
}

void make_arglist(char *argString) {
	char *token;
	char *delimiter = " ";
	int idx = 0;

	token = strtok(argString, delimiter);
	while (token != NULL) {
		arglist[idx++] = token;
		token = strtok(NULL, delimiter);
	}
	arglist[idx] = NULL;
}

void check_the_trash(void) {
	DIR *dir_info;
	struct dirent *dir_entry;

	dir_info = opendir(CAMPER);

	if (dir_info == NULL)
		oops("open", 1);

	while (dir_entry = readdir(dir_info)) {
		if (strcmp(dir_entry->d_name, "trash") == 0) {
			puts("There is Recycle Bin");

			return;
		}
	}

	tty_mode(0);
	set_terminal();
	get_response(MAXTRIES);
	tty_mode(1);

	closedir(dir_info);
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

void tty_mode(int how) {
	static struct termios original_mode;
	static int original_flags;

	if (how == 0) {
		tcgetattr(0, &original_mode);
		original_flags = fcntl(0, F_GETFL);
	}
	else {
		tcsetattr(0, TCSANOW, &original_mode);
		original_flags = fcntl(0, F_SETFL, original_flags);
	}
}

void set_terminal(void) {
	struct termios ttystate;

	tcgetattr(0, &ttystate);
	ttystate.c_lflag &= ~ICANON;
	ttystate.c_lflag &= ~ECHO;
	ttystate.c_cc[VMIN] = 1;

	tcsetattr(0, TCSANOW, &ttystate);
}

void get_response(int tries) {
	char response;

	puts("There is no Recycle bin. Do you want to create it? y/n");
	response = tolower(get_char());

	switch (response) {
	case 'y':
		mkdir(TRASHPATH, 0755);
		puts("You have successfully created Recycle Bin.");

		return;

	case 'n':
		puts("Do not create Recycle Bin");

		return;
	}
	//시간에 따른 보안상의 이유로 종료도 추가하면 좋을 듯.
}

int get_char(void) {
	int ch;
	int count = 0;

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

void handler(int signum) {
	//tty_mode(1);
	puts("shutdown");
	//프로세스를 종료하면 더이상 휴지통을 사용하지 못합니다.
	//- 휴지통을 삭제하시겠습니까?
	//		- yes : 휴지통 디렉토리 전체 삭제 -> 다시 프로세스 실행 시 휴지통여부 ㄴㄴ
	//		- no : 휴지통 디렉토리 보존 -> 다시 프로세스 실행 시 휴지통 여부 ㅇㅇ
	exit(1);
}




//___________________________//
void printNowLocat()
{
	char nowloc[256];

	getcwd(nowloc,256);

        if(get_inode(".") == get_inode("..")){
                printf("/");
        }//for root dir
	else{
        	printpathto(get_inode("."));
	}
	printf("(^_^) ");
	chdir(nowloc);
}

ino_t get_inode(char *fname)
{
        struct stat info;
        if(stat(fname, &info) == -1){
                fprintf(stderr, "Cannot stat ");
                perror(fname);
                exit(1);
        }
        return info.st_ino;
}

void printpathto(ino_t this_inode)
{
        ino_t my_inode;
        char its_name[BUFSIZ];

        if(get_inode("..") != this_inode)
        {
                chdir("..");
                inum_to_name(this_inode, its_name, BUFSIZ);
                my_inode = get_inode(".");
                printpathto(my_inode);
                printf("/%s", its_name);
        }


}

void inum_to_name(ino_t inode_to_find, char *namebuf, int buflen)
{
        DIR *dir_ptr;
        struct dirent *direntp;
        dir_ptr = opendir(".");
        if(dir_ptr == NULL){
                perror(".");
                exit(1);
        }

        while((direntp = readdir(dir_ptr)) != NULL)
                if(direntp->d_ino == inode_to_find){
                        strncpy(namebuf, direntp->d_name, buflen);
                        namebuf[buflen-1] ='\0';
                        closedir(dir_ptr);
                        return;
                }
        fprintf(stderr, "error looking for inum %lu\n", inode_to_find);
        //ino_t type is long unsigned int
        exit(1);
}
//_______________________________// 
