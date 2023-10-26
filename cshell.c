#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define CMDLINE_MAX 512

struct commandLineArg {
    char *command;
    char **arguments;
    int redirectionType;
    char *directory;
    int argsNum;
};

struct command
{
    struct commandLineArg commandLine;
    struct command* next;
};

// Global variables
int numberOfProcesses = 0;
char commandReturn[CMDLINE_MAX];
int backgroundProcessEnabled = 0;

// Function Declarations
int changeOutput(int newdir, char* directory);
struct commandLineArg create_commandLineArg(char* cmd);
int execute(struct commandLineArg commands);
struct command* createCommandList(char* cmd);
void freeCommandList(struct command* headPtr);
void pipeline(struct command* headptr);
int CheckBackgroundProcess(char* cmd);
int executeCD(char* directory, char* cmd);

int main(void)
{
    char cmd[CMDLINE_MAX];
    char og_cmd[CMDLINE_MAX];
    pid_t pid;
    int status;

    while(1)
    {
        char* nl;
        
        if(!backgroundProcessEnabled)
            waitpid(pid, &status, WNOHANG);

        // Print prompt 
        printf("cshell$ ");
        fflush(stdout);

        // Get command line
        fgets(og_cmd, CMDLINE_MAX, stdin);

        // Print command line if stdin is not provided by terminal
        if (!isatty(STDIN_FILENO))
        {
            printf("%s", og_cmd);
            fflush(stdout);
        }

        // Remove trailing newline from command line
        nl = strchr(og_cmd, '\n');
        if (nl)
            *nl = '\0';

        // Create a copy of the cmd so we can safely modify it
        strcpy(cmd, og_cmd);

        // If the input is nothing (enter), display the prompt again
        if (!strcmp(cmd, "")) continue;

        // Check to see if the job desires to be processed in the background
        backgroundProcessEnabled = CheckBackgroundProcess(cmd);
        
        // Creates a list of commands given in the command line and returns the first command
        struct command* headPtr = createCommandList(cmd);
        if (headPtr == NULL) continue; // safety check

        // New way to do commands
        struct commandLineArg commands;

        // Reads the first command off the list (FIF0)
        commands = headPtr->commandLine; 

        // Builtin command for exit 
        if (!strcmp(cmd, "exit"))
        {
            if(!backgroundProcessEnabled && waitpid(pid, &status, WNOHANG) != 0)
            {
                // Breaks the loop after freeing the dynamically allocated memory
                fprintf(stderr, "Bye...\n");
                fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 0);
                freeCommandList(headPtr);
                break;
            }
            else
            {
                // If background jobs are still running then don't exit
                fprintf(stderr, "Error: Background jobs still active...\n");
                fprintf(stderr, "+ completed '%s' [%d]\n", cmd, 1);
                continue;
            }
            
        } 

        // Builtin command for cd 
        if (!strcmp(commands.command, "cd")) 
        {
            executeCD(commands.arguments[1], cmd);
            continue;
        }

        // Regular command 
        pid = fork();
        if (pid == 0)
        {
            // Child  
            strcpy(commandReturn, "+ completed '");
            strcat(commandReturn, og_cmd);
            strcat(commandReturn, "\' ");

            pipeline(headPtr);
        }
        else if (pid > 0)
        {
            // Parent
            
            // Waits for a process if not a background process
            if(backgroundProcessEnabled)
                waitpid(-1, &status, WNOHANG);
            else
                waitpid(pid, &status, 0);
            

            // Frees up all stored data
            freeCommandList(headPtr);
        }
        else // Error handling
        {
            perror("fork");
            exit(1);
        }
    }

    return EXIT_SUCCESS;
}

// This function is in charge of changing the output directory for redirection
int changeOutput(int newdir, char* directory) 
{
    int file;
    if (newdir == 1) // In the case of > redirection
        file = open(directory, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    else if (newdir == 2) // In the case of >> redirection
        file = open(directory, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (file == -1) // Error handling
    {
        perror("Error: cannot open output file\n");
        exit(1);
    }

    // Sets the stdout to the open file
    dup2(file, STDOUT_FILENO);

    close(file);
    return -1;
}

// This function creates a struct commandLineArg for each of the commands and populates it with the necessary data
struct commandLineArg create_commandLineArg(char* cmd) {
    struct commandLineArg command;
    int totalArguments = 0;
    char **args = calloc(totalArguments+1, sizeof(char*));
    if (args == NULL) // Error handling
    {
        perror("Calloc failed to allocate for args.\n");
        exit(1);
    }

    char* delimiter = " ";

    if (strstr(cmd, ">") != NULL) 
    {
        // Decides between a > or >> redirection
        command.redirectionType = 1;
        if (strstr(cmd, ">>") != NULL) 
        {
            command.redirectionType = 2; 
        }
           
        // Variables to tokenize by >
        char* redirectionDelimiter = ">";
        char* current_argument = strtok(cmd, redirectionDelimiter);
        cmd = current_argument;

        // Stores the destination of the redirection
        char* output = strtok(NULL, redirectionDelimiter);
        while (output[0] == ' ') 
        {
            output += 1;
        }
    
        command.directory = calloc(strlen(output), sizeof(char));
        if (command.directory == NULL) // Error handling
        {
            perror("Calloc failed to allocate for args.\n");
            exit(1);
        }
        strcpy(command.directory, output);
    } 
    else 
    {
        command.directory = NULL;
    }

    // Separates the command with spaces and stores it to the struct
    char* current_argument = strtok(cmd, delimiter);
    while(current_argument != NULL)
    {
        totalArguments += 1;
        // Increases the size of the args for each additional arg (this means we don't need the size beforehand)
        args = realloc(args, (totalArguments+1) * sizeof(char*));
        if (args == NULL) // Error handling
        {
            perror("Calloc failed to allocate for args.\n");
            exit(1);
        }

        // Allocates and stores the memory needed for each word of the command.
        args[totalArguments-1] = (char*)calloc(strlen(current_argument), sizeof(char));
        if (args[totalArguments-1] == NULL) // Error handling
        {
            perror("Calloc failed to allocate for args.\n");
            exit(1);
        }
        strncpy(args[totalArguments-1], current_argument, strlen(current_argument));
        current_argument = strtok(NULL, delimiter);
    }
    args[totalArguments] = NULL;

    if (totalArguments > 11) // Error handling for too many arguments
    {
        fprintf(stderr, "Error: too many process arguments\n");
        totalArguments = -1;
    }

    // Populates the struct with the data
    command.argsNum = totalArguments;
    command.arguments = args;
    command.command = args[0];

    return command;
}

// This function is what executes all commands to the child
int execute(struct commandLineArg commands) 
{ 
    // Function for pwd (uses getcwd())
    if (!strcmp(commands.command, "pwd")) 
    { 
        char cwd[CMDLINE_MAX];
        getcwd(cwd, sizeof(cwd));
        printf("%s\n", cwd);
        exit(0);
    }

    // If cd command is detected, break because cd will be called later 
    else if (!strcmp(commands.command, "cd")) 
    {
        exit(0);
    } 
    else 
    {
        if (commands.directory != NULL) // When the commands contains a redirection
        {
            // Changes output then resets the directory string
            changeOutput(commands.redirectionType, commands.directory);
            free(commands.directory);
            commands.directory = NULL;
        }

        int retval = execvp(commands.command, commands.arguments);
        fprintf(stderr, "Error: command not found\n");
        return retval;
    }
}

// In charge of checking for parsing errors as listed in the pdf.
int checkParsingErrors(char* cmd)
{
    int i = 0;

    while (cmd[i] == ' ') // Skips any possible leading spaces
    {
        i++; 
    }
    if (cmd[i] == '>' || cmd[i] == '|') // Checks if > or | is called before a command
    {
        fprintf(stderr, "Error: missing command\n");
        return 1;
    }
    i = strlen(cmd) - 1;
    while (cmd[i] == ' ') // Sets i to the location of the last non-space character
        i--;
    if (cmd[i] == '|') // Checks if | is missing a command after it
    {
        fprintf(stderr, "Error: missing command\n");
        return 1;
    }
    else if (cmd[i] == '>') // Checks if redirection is missing an output
    {
        fprintf(stderr, "Error: no output file\n");
        return 1;
    }

    int redirecting = 0;
    for (int iterator = 0; iterator <= i; iterator++) // Checks if > occurs before |
    {
        if (cmd[iterator] == '>') 
        {
            redirecting = 1;
        }
        if (redirecting && cmd[iterator] == '|') 
        {
            fprintf(stderr, "Error: mislocated output redirection\n");
            return 1;
        }
    }
    return 0;
}

// In charge of separating commands when piplineing and creating a struct command.
struct command* createCommandList(char* cmdinput)
{
    // Copies the command so the original is not tokenized
    char cmd[CMDLINE_MAX];
    strcpy(cmd, cmdinput);

    if (checkParsingErrors(cmd)) // Skips the function if there are parcing errors
        goto END;

    numberOfProcesses = 0;
    char* delimiter = "|";

    char* spot = cmd;
    char* current_command = strtok_r(spot, delimiter, &spot);

    struct command* headPtr = NULL;
    struct command* currentPtr = NULL;

    // Creates a commandLineArg for each of the commands
    while(current_command != NULL)
    {
        struct command* newCommand = (struct command*)malloc(sizeof(struct command));
        if(newCommand == NULL) // Error handling
        {
            perror("Malloc failed to allocate for command.\n");
            exit(1);
        }
        newCommand->commandLine = create_commandLineArg(current_command);   
        if (newCommand->commandLine.argsNum == -1) // Error handling
            goto END;
        
        if(headPtr == NULL)
        {
            headPtr = newCommand;
            currentPtr = headPtr;
        }
        else
        {
            currentPtr->next = newCommand;
        }

        newCommand->next = NULL;
        currentPtr = newCommand;
        ++numberOfProcesses;

        current_command = strtok_r(spot, delimiter, &spot);
    }

    return headPtr;
    END:
    return NULL;
}

// In charge of freeing all the dynamically allocated memory
void freeCommandList(struct command* headPtr)
{
    struct command* currentPtr = headPtr;
    while(currentPtr != NULL)
    {
        struct command* deleteNode = currentPtr;

        // Frees all the arguments
        for (int i = 0; i < currentPtr->commandLine.argsNum; i++) 
        {
            free(currentPtr->commandLine.arguments[i]);
        }

        // Frees the directory (only needed for exit)
        if (currentPtr->commandLine.directory != NULL) 
            free(currentPtr->commandLine.directory);

        // Frees arguments array
        free(currentPtr->commandLine.arguments);

        currentPtr = currentPtr->next;

        // Frees the struct
        free(deleteNode);
    }
    return;
}

// Runs commands and separtates commands in case of pipelining
void pipeline(struct command* headptr)
{
    struct command* currentptr = headptr;
    while(currentptr->next != NULL)
    {
        // Create the pipe
        int fd[2];
        pipe(fd);

        pid_t pid = fork();
        if(pid == 0)
        {
            // Child

            // Link the write port to stdout
            dup2(fd[1], 1); 
            execute(currentptr->commandLine);
            exit(1);
        }
        else if (pid > 0)
        {
            // Parent
            int status;
            waitpid(pid, &status, 0);
            
            strcat(commandReturn, "[");
            char returnvalue = WEXITSTATUS(status) + '0';
            strcat(commandReturn, &returnvalue);
            strcat(commandReturn, "]");
        } 
        else // Error handling
        {
            perror("piping error");
            exit(1);
        }
        
        // Link the read port to stdin
        dup2(fd[0], 0);
        close(fd[1]);
        
        // Move to the next command
        currentptr = currentptr->next;
    }
    pid_t pid = fork();
    if(pid == 0)
    {
        // Child
        execute(currentptr->commandLine);
        exit(1);
    } 
    else if (pid > 0)
    {
        // Parent
        int status;
        waitpid(pid, &status, 0);

        strcat(commandReturn, "[");
        char returnvalue = WEXITSTATUS(status) + '0';
        strcat(commandReturn, &returnvalue);
        strcat(commandReturn, "]");
        
        fprintf(stderr, "%s\n", commandReturn);
        
        exit(0);
    } 
    else
    {
        perror("piping error");
        exit(1);
    }
}

// Checks if a command has an & char at the end and returns 1 if so
int CheckBackgroundProcess(char* cmd) 
{
    int length = strlen(cmd);
    while (cmd[length-1] == ' ') // Removes trailing spaces
        length--;
    if (cmd[length-1] == '&') 
    {
        cmd[length-1] = ' ';
        return 1;
    }
    else
    {
        return 0;
    }
}

// Responsible for the cd command
int executeCD(char* directory, char* cmd)
{
    int retval = chdir(directory);

    if(retval == -1) // Error handling
    {
        fprintf(stderr, "Error: cannot cd into directory\n");
        retval = 1;
    }

    fprintf(stderr, "+ completed '%s' [%d]\n", cmd, retval);
    return retval;
}