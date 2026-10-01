#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define FILE_NAME_SIZE 256


int wait_for_child(pid_t child){
    int status;
    pid_t result;

    do {
    result = waitpid(child, &status, 0);
    }while (result == -1 && errno == EINTR);

    if (result == -1) {
        return -1;
    }
    
    if (!WIFEXITED(status)){
        return -1;
    }
    if (WEXITSTATUS(status) != 0){
        return -1;
    }
    return 0;
}

void print_error(const char *message, size_t size){
    write(STDERR_FILENO, message, size);
}

int read_file_name(char *file_name){
    int index = 0;
    char character;

    while (index < FILE_NAME_SIZE - 1){
        ssize_t bytes_read = read(STDIN_FILENO, &character, 1);

        if (bytes_read == -1){
            if (errno == EINTR){
                continue;
            }
            return -1;
        }
        if (bytes_read == 0 || character == '\n'){
            break;
        }
        file_name[index] = character;
        index++;
    }

    file_name[index] = '\0';

    if (index == 0){
        return -1;
    }
    return 0;
}

int main(void)
{

    struct sigaction action;
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    if (sigaction(SIGPIPE, &action, NULL) == -1){
        const char message[] = "Error: cannot configure SIGPIPE\n";
        print_error(message, sizeof(message) -1);
        return 1;
    }
    
    char first_file_name[FILE_NAME_SIZE];
    char second_file_name[FILE_NAME_SIZE];

    const char first_prompt[] = "First file name: ";
    write(STDOUT_FILENO, first_prompt, sizeof(first_prompt) - 1);

    if (read_file_name(first_file_name) == -1){
        const char message[] = "Error: invalid first file name\n";
        print_error(message, sizeof(message) - 1);
        return 1;
    }

    const char second_prompt[] = "Second file name: ";
    write(STDOUT_FILENO, second_prompt, sizeof(second_prompt) - 1);

    if (read_file_name(second_file_name) == -1){
        const char message[] = "Error: invalid second file name\n";
        print_error(message, sizeof(message) - 1);
        return 1;
    }
    
    int pipe2[2];
    int pipe1[2];

    if (pipe(pipe1) == -1) {
        const char message[] = "Error: cannot create pipe1\n";
        print_error(message, sizeof(message) - 1);
        return 1;
    }

    if (pipe(pipe2) == -1) {
        const char message[] = "Error: cannot create pipe2\n";
        print_error(message, sizeof(message) - 1);
        
        close(pipe1[0]);
        close(pipe1[1]);
        return 1;
    }

    pid_t child1 = fork();

    if (child1 == -1){
        const char message[] = "Error: cannot create child1\n";
        print_error(message, sizeof(message) - 1);
        
        close(pipe1[0]);
        close(pipe1[1]);
        close(pipe2[0]);
        close(pipe2[1]);

        return 1;
    }

    if (child1 == 0){
        close(pipe1[1]);
        close(pipe2[0]);
        close(pipe2[1]);

        int output_file = open(first_file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if (output_file == -1){
            const char message[] = "Error: child1 cannot open output file\n";
            print_error(message, sizeof(message) - 1);
            close(pipe1[0]);
            _exit(1);
        }

        if (dup2(pipe1[0], STDIN_FILENO) == -1) {
            const char message[] ="Error: child1 cannot redirect stdin\n";
                print_error(message, sizeof(message) - 1);
            close(pipe1[0]);
            close(output_file);
            _exit(1);
        }
        
        if(dup2(output_file, STDOUT_FILENO) == -1){
            const char message[] ="Error: child1 cannot redirect stdout\n";
                print_error(message, sizeof(message) - 1);

            close(pipe1[0]);
            close(output_file);
            _exit(1);
        }
        close(pipe1[0]);
        close(output_file);
        execl("./child", "child", (char *)0);
        const char message[] = "Error: cannot execute child\n";
        print_error(message, sizeof(message) - 1);
        _exit(1);
    }

    pid_t child2 = fork();

    if (child2 == -1){
        const char message[] = "Error: cannot create child2\n";
        print_error(message, sizeof(message) - 1);
        
        close(pipe1[0]);
        close(pipe1[1]);
        close(pipe2[0]);
        close(pipe2[1]);

        waitpid(child1, NULL, 0);
        return 1;
    }

    if (child2 == 0){
        close(pipe1[0]);
        close(pipe1[1]);
        close(pipe2[1]);

        int output_file = open(second_file_name, O_WRONLY | O_CREAT | O_TRUNC, 0644);

        if (output_file == -1){
            const char message[] = "Error: child2 cannot open output file\n";
            print_error(message, sizeof(message) - 1);
            close(pipe2[0]);
            _exit(1);
        }
        
        if (dup2(pipe2[0], STDIN_FILENO) == -1){
            const char message[] = "Error: child2 cannot redirect stdin\n";
            print_error(message, sizeof(message) - 1);
            close(pipe2[0]);
            close(output_file);
            _exit(1);
        }

        if (dup2(output_file, STDOUT_FILENO) == -1){
            const char message[] ="Error: child2 cannot redirect stdout\n";
            print_error(message, sizeof(message) - 1);
            close(pipe2[0]);
            close(output_file);
            _exit(1);
        }

        close(pipe2[0]);
        close(output_file);
        execl("./child", "child", (char *)0);
        const char message[] = "Error: cannot execute child\n";
        print_error(message, sizeof(message) - 1);
        _exit(1);   
    }

    close(pipe1[0]);
    close(pipe2[0]);

    char character;
    int line_number = 1;

    while (1){
        ssize_t bytes_read = read(STDIN_FILENO, &character, 1);

        if (bytes_read == -1){
            if (errno == EINTR){
                continue;
            }
            const char message[] = "Error: cannot read input\n";
            print_error(message, sizeof(message) - 1);
            close(pipe1[1]);
            close(pipe2[1]);

            waitpid(child1, NULL, 0);
            waitpid(child2, NULL, 0);
            return 1;
        }
        if (bytes_read == 0){
            break;
        }
        int destination;

        if (line_number % 2 == 1){
            destination = pipe1[1];
        } else{
            destination = pipe2[1];
        }
        ssize_t bytes_written;
        do {
            bytes_written = write(destination, &character, 1);
        } while (bytes_written == -1 && errno == EINTR);

        if (bytes_written != 1) {
        
        const char message[] = "Error: cannot write to pipe\n";
        print_error(message, sizeof(message) - 1);
        close(pipe1[1]);
        close(pipe2[1]);

        waitpid(child1, NULL, 0);
        waitpid(child2, NULL, 0);
        return 1;
        }
        if (character == '\n'){
            line_number++;
        }
    }

    close(pipe1[1]);
    close(pipe2[1]);

    int child1_result = wait_for_child(child1);
    int child2_result = wait_for_child(child2);

    if (child1_result == -1 || child2_result == -1){
        const char message[] = "Error: child process failed\n";
        print_error(message, sizeof(message) - 1);
        return 1;
    }
    
    return 0;
}
