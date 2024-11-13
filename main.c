#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_COMMAND_LENGTH 1024
#define MAX_ARGS 64

// >
void redirect_output(char *filename) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Ошибка открытия файла для записи");
        exit(EXIT_FAILURE);
    }
    dup2(fd, STDOUT_FILENO);
    close(fd);
}

// >>
void append_output(char *filename) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0) {
        perror("Ошибка открытия файла для дописывания");
        exit(EXIT_FAILURE);
    }
    dup2(fd, STDOUT_FILENO);
    close(fd);
}

// <
void redirect_input(char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("Ошибка открытия файла для чтения");
        exit(EXIT_FAILURE);
    }
    dup2(fd, STDIN_FILENO);
    close(fd);
}

// |
void conveyor(char *command) {
    char *pipeline_commands[MAX_ARGS];
    int num_pipeline_commands = 0;

    // Разделяем команду по `|`
    char *token = strtok(command, "|");
    while (token != NULL) {
        pipeline_commands[num_pipeline_commands++] = token;
        token = strtok(NULL, "|");
    }

    int pipefd[2];
    int fd_in = 0; 

    for (int i = 0; i < num_pipeline_commands; i++) {
        char *args[MAX_ARGS];
        int argc = 0;
        char *arg_token = strtok(pipeline_commands[i], " ");
        while (arg_token != NULL && argc < MAX_ARGS - 1) {
            args[argc++] = arg_token;
            arg_token = strtok(NULL, " ");
        }
        args[argc] = NULL;

        // Создаём пайп для следующей команды, если это не последняя команда
        if (i < num_pipeline_commands - 1) {
            if (pipe(pipefd) == -1) {
                perror("Ошибка создания пайпа");
                exit(EXIT_FAILURE);
            }
        }

        pid_t pid = fork();

        if (pid == 0) { // Дочерний процесс
            if (fd_in != 0) { // Если это не первая команда
                dup2(fd_in, STDIN_FILENO);
                close(fd_in);
            }
            if (i < num_pipeline_commands - 1) { // Если это не последняя команда
                dup2(pipefd[1], STDOUT_FILENO);
                close(pipefd[1]);
            }
            close(pipefd[0]);

            execvp(args[0], args);
            perror("Ошибка выполнения команды");
            exit(EXIT_FAILURE);
        } else { // Родительский процесс
            waitpid(pid, NULL, 0);
            close(pipefd[1]);
            fd_in = pipefd[0];
        }
    }
}

// Функция для выполнения команды
void execute_command(char *command) {
    
    if (strchr(command, '|')) {
        conveyor(command);
        return;
    }

    char *args[MAX_ARGS];
    int argc = 0;
    char *token = strtok(command, " ");

    // Разбиваем команду на аргументы
    while (token != NULL && argc < MAX_ARGS - 1) {
        args[argc++] = token;
        token = strtok(NULL, " ");
    }
    args[argc] = NULL;

    // Проверка на наличие перенаправлений
    for (int i = 0; i < argc; i++) {
        if (strcmp(args[i], ">") == 0) { 
            args[i] = NULL;
            redirect_output(args[i + 1]);
            break;
        } else if (strcmp(args[i], ">>") == 0) {
            args[i] = NULL;
            append_output(args[i + 1]);
            break;
        } else if (strcmp(args[i], "<") == 0) { 
            args[i] = NULL;
            redirect_input(args[i + 1]);
            break;
        }
    }

    pid_t pid = fork();
    if (pid == 0) { // Дочерний процесс
        execvp(args[0], args);
        perror("Ошибка выполнения команды");
        exit(EXIT_FAILURE);
    } else { // Родительский процесс
        int status;
        waitpid(pid, &status, 0);
    }
}

// Функция для разбиения строки на команды по разделителю `;`
void split_commands(char *input, char *commands[], int *num_commands) {
    char *token = strtok(input, ";");
    *num_commands = 0;

    while (token != NULL) {
        commands[(*num_commands)++] = token;
        token = strtok(NULL, ";");
    }
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Использование: %s <команда>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Собираем все аргументы в одну строку
    char input[MAX_COMMAND_LENGTH] = "";
    for (int i = 1; i < argc; i++) {
        strcat(input, argv[i]);
        if (i < argc - 1) strcat(input, " ");
    }

    // Разделяем строку на команды
    char *commands[MAX_ARGS];
    int num_commands = 0;
    split_commands(input, commands, &num_commands);

    // Выполняем каждую команду
    for (int i = 0; i < num_commands; i++) {
        execute_command(commands[i]);
    }

    return 0;
}
