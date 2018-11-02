#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAXARGS   128
#define MAXLINE   128
#define MAXPIPE   32

void eval(char *cmdline);
int commandType(char *command);
int parsePipe(char *buf, char **lines, char **pipe_in_file_p, char **pipe_out_file_p, int *pipe_out_file_mode_p);
void parseLine(char *buf, char **argv);
char *readLineAndStore(int fd);

char *command_type[] = { NULL, "exit", "cd", "pwd", "rm", "mv", "cp", "cat", "head", "tail", NULL };
void btin_exit(char *num);
int btin_cd(char *dir);
void ext_pwd();
int ext_rm(char *file);
int ext_mv(char *file1, char *file2);
int ext_cp(char *file1, char *file2);
int ext_cat(char *file);
int ext_head(char *argv1, char *argv2, char *argv3);
int ext_tail(char *argv1, char *argv2, char *argv3);

int main() {
    char cmdline[MAXLINE];
    int cmdlen;

    signal(SIGINT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);

    while (1) {
	    printf("> ");                   
	    fgets(cmdline, MAXLINE, stdin); 
	    if (feof(stdin))
	        exit(0);
        cmdlen = strlen(cmdline);
        if(cmdline[cmdlen-1] == '\n')
            cmdline[cmdlen-1] = '\0';
	    eval(cmdline);
    } 
}
  
void eval(char *cmdline) {
    char *pipe_in_file;
    char *pipe_out_file;
    int pipe_out_file_mode;
    char *argvs[MAXPIPE][MAXARGS];
    char *lines[MAXPIPE];
    int pipe_num;
    int result;
    pid_t pid;
    int status;
    int out_file_fd;
    int in_file_fd;
    int pipe_fd[MAXPIPE][2];
    int pipe_cur;

    pipe_num = parsePipe(cmdline, lines, &pipe_in_file, &pipe_out_file, &pipe_out_file_mode);
    for(pipe_cur = 0; pipe_cur <= pipe_num; pipe_cur++)
        parseLine(lines[pipe_cur], argvs[pipe_cur]);
    
    /*DEBUG*/
    /*printf("pipe_num = %d\n", pipe_num);
    printf("pipe_in_file = \"%s\"\n", pipe_in_file);
    printf("pipe_out_file = \"%s\"\n", pipe_out_file);
    printf("pipe_out_file_mode = %d\n", pipe_out_file_mode);
    for(int i = 0; i <= pipe_num; i++) {
        printf("pipe[%d]: ", i);
        for(int j = 0; argvs[i][j] != NULL; j++) {
            printf("argv[%d] = %s, ", j, argvs[i][j]);
        }
        printf("\n");
    }*/

    if(argvs[0][0] == NULL)
        return;

    switch(commandType(argvs[0][0])) {
        case 1: btin_exit(argvs[0][1]); return;
        case 2: btin_cd(argvs[0][1]); return;
    }

    for(pipe_cur = 0; pipe_cur <= pipe_num; pipe_cur++) {
        if(pipe_cur+1 <= pipe_num) {
            pipe(pipe_fd[pipe_cur]);
        }
        if((pid = fork()) == 0) {
            if(pipe_cur == 0 && pipe_in_file != NULL) {
                in_file_fd = open(pipe_in_file, O_RDONLY);
                dup2(in_file_fd, STDIN_FILENO);
                close(in_file_fd);
            }
            if(pipe_cur == pipe_num && pipe_out_file != NULL) {
                if(pipe_out_file_mode == 1)
                    out_file_fd = open(pipe_out_file, O_WRONLY | O_TRUNC | O_CREAT, 0644);
                else
                    out_file_fd = open(pipe_out_file, O_WRONLY | O_APPEND| O_CREAT, 0644);
                dup2(out_file_fd, STDOUT_FILENO);
                close(out_file_fd);
            }
            if(pipe_cur < pipe_num) {
                close(pipe_fd[pipe_cur][0]);
                dup2(pipe_fd[pipe_cur][1], 1);
            }
            if(pipe_cur > 0) {
                dup2(pipe_fd[pipe_cur-1][0], 0);
            }

            switch(commandType(argvs[pipe_cur][0])) {
                case 3: result = 0; ext_pwd(); return;
                case 4: result = ext_rm(argvs[pipe_cur][1]); break;
                case 5: result = ext_mv(argvs[pipe_cur][1], argvs[pipe_cur][2]); break;
                case 6: result = ext_cp(argvs[pipe_cur][1], argvs[pipe_cur][2]); break;
                case 7: result = ext_cat(argvs[pipe_cur][1]); break;
                case 8: result = ext_head(argvs[pipe_cur][1], argvs[pipe_cur][2], argvs[pipe_cur][3]); break;
                case 9: result = ext_tail(argvs[pipe_cur][1], argvs[pipe_cur][2], argvs[pipe_cur][3]); break;
                case 0: execvp(argvs[pipe_cur][0], argvs[pipe_cur]);
            }

            exit(result);
        } else {
            wait(NULL);
            if(pipe_cur > 0)
                close(pipe_fd[pipe_cur-1][0]);
            if(pipe_cur < pipe_num)
                close(pipe_fd[pipe_cur][1]);
        }
    }
	
    return;
}

int commandType(char *command) {
    int i;

    for(i = 1; command_type[i] != NULL; i++)
        if(strcmp(command, command_type[i]) == 0)
            return i;
    
    return 0;
}

/*NEED TO CONSIDER $ < > CASE*////////////////////////////////////////////////////////////////////////////////////////////
int parsePipe(char *buf, char **lines, char **pipe_in_file_p, char **pipe_out_file_p, int *pipe_out_file_mode_p) {
    int pipe_num = 0;
    char *cur;
    int i;

    lines[0] = buf;
    for(cur = buf; *cur != '\0'; cur++) {
        if(*cur == '|') {
            pipe_num++;
            lines[pipe_num] = cur+1;
            *cur = '\0';
        }
    }

    if((cur = strchr(lines[0], '<')) != NULL) {
        *cur = '\0';
        for(cur++; *cur == ' '; cur++);
        *pipe_in_file_p = cur;
        for(; *cur != ' ' && *cur != '\0'; cur++);
        *cur = '\0';
    } else {
        *pipe_in_file_p = NULL;
    }

    if((cur = strstr(lines[pipe_num], ">>")) != NULL) {
        *pipe_out_file_mode_p = 2;
        *cur = '\0';
        *(cur+1) = '\0';
        for(cur+=2; *cur == ' '; cur++);
        *pipe_out_file_p = cur;
        for(; *cur != ' ' && *cur != '\0'; cur++);
        *cur = '\0';
    } else if((cur = strchr(lines[pipe_num], '>')) != NULL) {
        *pipe_out_file_mode_p = 1;
        *cur = '\0';
        for(cur++; *cur == ' '; cur++);
        *pipe_out_file_p = cur;
        for(; *cur != ' ' && *cur != '\0'; cur++);
        *cur = '\0';
    } else {
        *pipe_out_file_p = NULL;
    }

    return pipe_num;
}

void parseLine(char *buf, char **argv) {
    int argc = 0;
    char *cur;

    for(cur = buf; *cur == ' ' && *cur != '\0'; cur++);
    for(; *cur != '\0'; cur++) {
        if(*cur == '\"') {
            argv[argc++] = cur+1;
            for(cur++; *cur != '\"' && *cur != '\0'; cur++);
        } else if(*cur == '\'') {
            argv[argc++] = cur+1;
            for(cur++; *cur != '\'' && *cur != '\0'; cur++);
        } else {
            argv[argc++] = cur;
            for(; *cur != ' ' && *cur != '\0'; cur++);
        }
        *cur = '\0';
        for(; *cur == ' ' && *cur != '\0'; cur++);
    }
    argv[argc] = NULL;
}

char *readLineAndStore(int fd) {
    char buff[2048];
    char *line;
    int rdbyte;
    int cur;

    if((rdbyte = read(fd, buff, sizeof(buff))) > 0) {
        for(cur = 0; cur < rdbyte && buff[cur] != '\n'; cur++);
        line = (char*)malloc((cur+1) * sizeof(char));
        strncpy(line, buff, cur);
        line[cur] = '\0';
        lseek(fd, cur-rdbyte+1, SEEK_CUR);
        return line;
    } else return NULL;
}

void btin_exit(char *num) {
    fprintf(stderr, "exit\n");
    if(num != NULL)
        exit(atoi(num));
    else
        exit(0);
}

int btin_cd(char *dir) {
    return chdir(dir);
}

void ext_pwd() {
    char buf[255];
    getcwd(buf, 255);
    printf("%s\n", buf);
}

int ext_rm(char *file) {
    return unlink(file);
}

int ext_mv(char *file1, char *file2) {
    return rename(file1, file2);
}

int ext_cp(char *file1, char *file2) {
    char buffer[512];
    int srcfd;
    int destfd;
    int rdbyte;

    if((srcfd = open(file1, O_RDONLY)) < 0) {
        printf("swsh: No such file\n");
        return -1;
    }

    if((destfd = open(file2, O_WRONLY | O_TRUNC | O_CREAT, 0644)) < 0)
        return -1;

    while((rdbyte = read(srcfd, buffer, sizeof(buffer))) > 0)
        write(destfd, buffer, rdbyte);

    return 0;
}

int ext_cat(char *file) {
    char buffer[512];
    int fd;
    int rdbyte;

    if(file == NULL)
        fd = STDIN_FILENO;
    else {
        fd = open(file, O_RDONLY);
        if(fd < 0) {
            perror("Error: File doesn't exist");
            exit(1);
        }
    }

    while((rdbyte = read(fd, buffer, sizeof(buffer))) != 0)
        write(STDOUT_FILENO, buffer, rdbyte);

    return 0;
}

int ext_head(char *argv1, char *argv2, char *argv3) {
    FILE *file;
    char buf[4096];
    char *filename;
    int option = 10;
    char **lines;
    int linecnt;
    int i;

    if(argv1 == NULL) argv1 = "";
    if(argv2 == NULL) argv2 = "";
    if(argv3 == NULL) argv3 = "";

    if(strncmp(argv1, "-n", 2) == 0) {
        if(strlen(argv1) == 2) {
            option = atoi(argv2);
            filename = argv3;
        } else {
            option = atoi(&argv1[2]);
            filename = argv2;
        }
    } else if(strncmp(argv2, "-n", 2) == 0) {
        if(strlen(argv2) == 2) {
            option = atoi(argv3);
            filename = argv1;
        } else {
            option = atoi(&argv2[2]);
            filename = argv1;
        }
    } else {
        filename = argv1;
    }

    if(strlen(filename) == 0)
        file = stdin;
    else {
        if((file = fopen(filename, "r")) == NULL) {
            perror("Error: File doesn't exist");
            exit(1);
        }
    }

    lines = (char**)malloc(option * sizeof(char*));

    for(i = 0; i < option; i++) {
        if(fgets(buf, 4096, file) == NULL)
            break;
        lines[i] = (char*)malloc(strlen(buf) * sizeof(char));
        strcpy(lines[i], buf);
    }
    linecnt = i;
    fclose(file);

    for(i = 0; i < linecnt; i++) {
        fputs(lines[i], stdout);
        free(lines[i]);
    }
    
    return 0;
}

int ext_tail(char *argv1, char *argv2, char *argv3) {
    FILE *file;
    char buf[4096];
    char *filename;
    int option = 10;
    char **lines;
    int i;
    int last;

    if(argv1 == NULL) argv1 = "";
    if(argv2 == NULL) argv2 = "";
    if(argv3 == NULL) argv3 = "";

    if(strncmp(argv1, "-n", 2) == 0) {
        if(strlen(argv1) == 2) {
            option = atoi(argv2);
            filename = argv3;
        } else {
            option = atoi(&argv1[2]);
            filename = argv2;
        }
    } else if(strncmp(argv2, "-n", 2) == 0) {
        if(strlen(argv2) == 2) {
            option = atoi(argv3);
            filename = argv1;
        } else {
            option = atoi(&argv2[2]);
            filename = argv1;
        }
    } else {
        filename = argv1;
    }

    if(strlen(filename) == 0)
        file = stdin;
    else {
        if((file = fopen(filename, "r")) == NULL) {
            perror("Error: File doesn't exist");
            exit(1);
        }
    }

    lines = (char**)malloc(option * sizeof(char*));

    for(last = 0; ; last = (last+1) % option) {
        if(fgets(buf, 4096, file) == NULL)
            break;
        if(lines[last] != NULL) free(lines[last]);
        lines[last] = (char*)malloc(strlen(buf) * sizeof(char));
        strcpy(lines[last], buf);
    }
    fclose(file);

    for(i = last; i < option; i++) {
        if(lines[i] != NULL) {
            fputs(lines[i], stdout);
            free(lines[i]);
        }
    }

    for(i = 0; i < last; i++) {
        if(lines[i] != NULL) {
            fputs(lines[i], stdout);
            free(lines[i]);
        }
    }
    
    return 0;
}