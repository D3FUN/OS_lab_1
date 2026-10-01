#include <errno.h>
#include <unistd.h>

int is_vowel(char character){
    return character == 'a' ||
    character == 'e' ||
     character == 'i' ||
     character == 'o' ||
     character == 'u' ||
     character == 'A' ||
     character == 'E' ||
     character == 'I' ||
     character == 'O' ||
     character == 'U';
}

int main(void){
    char character;

    while(1){
        ssize_t bytes_read = read(STDIN_FILENO, &character, 1);

        if (bytes_read == -1){
            if (errno == EINTR){
                continue;
            }

            const char message[] = "Error: child cannot read\n";
            write(STDERR_FILENO, message, sizeof(message) - 1);
            return 1;
        }

        if (bytes_read == 0){
            break;
        }
        if (is_vowel(character)){
            continue;
        }
        
        ssize_t bytes_written;
        do {
            bytes_written = write(STDOUT_FILENO, &character, 1);
        } while(
            bytes_written == - 1 && errno == EINTR); 

        if (bytes_written != 1) {
            const char message[] = "Error: child cannot write\n";
            write(STDERR_FILENO, message, sizeof(message) - 1);
            return 1;
        }
    }
    return 0;
}