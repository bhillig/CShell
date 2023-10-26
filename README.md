# CShell

# About
This program was made by a team of two. It emulates the functionality of a shell, </br>
allowing users to run programs and commands, pipeline processes, create background </br>
jobs, and even redirect input and output.
# Implementation
The implementation of this program goes through 5 distinct steps for each input: </br>
1. **The user input is read, parsed and broken into smaller arguments or commands.**
2. **(If needed) Multiple commands are linked together in the case of pipelining.**
3. **(If needed) The output of the final command is redirected to a file in the** </br>
**case of redirection.**
4. **Each command is executed as a child of the parent process (possibly as a** </br>
**background process)**
5. **All dynamically stored memory is freed.** </br></br>


# Printing the prompt
Our implementation of a basic shell starts with a simple loop which prints </br>
a prompt and takes in input. This input is stored in a variable, cmd. We </br>
then check for a few edge cases such as no input being given before we </br>
parse the input. </br> </br>
Parsing the input is a two step process with functions createCommandList() </br>
and createCommandArg(). The former uses the latter to build its structure. </br> </br>

# Parsing the user's input
The parsing process for each user input follows 3 distinct steps.
1. **Separating the commands in the case of a pipline.** </br>
Since the input could contain multiple commands piped together we store </br>
the commands in a struct that contains a struct containting the info of </br>
each command (commandLineArg()) and a linked list to the next command. </br>
First we take the user's input and use strtok_r() to tokenize the input </br>
into multiple commands with "|" as the delimiter. </br>
2. **Separating destination output from the command in the case of redirection.**</br>
For each of the outputs/commands of strtok_r (from step 1) we use a separate </br>
function to check if the commands contains a redirection. If it does we </br>
use strtok() to tokenize the executable command (aka the part of the input </br>
before ">") and then we store the destination and the type of redirection </br>
(> or >>) in a commandLineArg() struct. </br>
3. **Separating the command into arguments and populating a struct.** </br>
In this step we take the output of step 2 (which is just a command) and then </br>
we malloc an array of char pointers to store the arguments. Then we use strtok() </br>
to tokenize the command with the " " delimiter and use malloc to allocate memory </br>
for each of the words in the command. These memory addresses are then added to </br>
the array of char pointers. This array of char pointers is then added to the </br>
commandLineArg() struct. </br></br>

# Checking for background jobs
Since our shell's behavior will substancially change if its executing a </br>
background process, we check for this right before we parse the input. The </br>
implementation simply checks the last token of the input for an ampersand </br>
symbol. If one is detected it replaces the symbol with a "" and returns 1, </br>
otherwise it returns 0. The removal of the symbol is done since it's only </br>
there to notify the shell it wants to be run in the background. </br> </br>

# Built in commands
Now that the command has been parsed, we run it through the built in </br>
commands (exit, cd, etc) since the work involved with those is specific. </br>
If none of those commands match, we then fork() to create a child process </br>
for our pipeline. </br> </br>

# The Pipeline process
## Forking the main process
Since we want our command(s) to execute before looping back to the shell </br>
prompt, we encompass our pipeline inside a child process. The parent </br>
process simply waits for the pipeline process to finish its execution </br>
before freeing the memory of the commands and looping back to the prompt. </br> </br>
If the job being executing wishes to be run in the background, then the </br>
parent doesn't wait for the pipeline to finish before looping back to </br>
the shell prompt. </br>
## The Pipeline Function
Inside the child process lies the pipeline. The pipeline takes in the </br>
head of a linked list as a pointer and iterates through until the </br>
currentPtr->next is equal to NULL. </br> </br>
**It goes through four distinct steps at each iteration** </br>
1. **The process creates a pipe and forks.** </br>
This is done so that we can establish a connection between the child and </br>
parent process. </br>
2. **The child process links the write port to stdout and executes the command** </br>
Doing this links the output of the current command to the input of the next</br>
one. The process of executing the command is detailed next. </br>
3. **The parent process waits for the child and records its return status** </br>
The parent process remains the same from the first iteration to the last. </br>
Creating child processes for each command allows the shell to record each </br>
return value. </br>
4. **Lastly, the read port is linked to stdin and the pipe is closed** </br>
The parent process takes in the input of the stdin to finish connecting </br>
the pipeline process.
</br> </br>
This loop iterates until we reach currentPtr->next is equal to NULL. Once </br>
reached, we know that we are on the last command so we apply the same </br>
process of forking the process to execute, while the parent records, </br>
except this time we print the return values for the entire chain of </br>
commands before finally killing the process.
# Executing the command
When executing a command, a function recieves a struct containing </br>
the command, arguments, output directory, and output type </br>
(> or >>) of the command to be executed. First, the </br>
function uses an if statement to check if the command is "pwd" or </br>
"cd" as they are built in commands. Then we check if the command will be </br>
redirected (by seeing if the destination is not NULL) and if so </br>
to redirect the output of the command a separate function (details of which </br>
are located below). Once that is complete the function uses execvp to execute </br>
the command and all of its arguments. </br></br>

# Changing the output
When a command wants to redirect the output to a file a function is called. </br>
This function recieves a redirection type (1 or 2 pertaining to > or >> </br>
respectively) and a destination. If the type is 1 then the file is opened or </br>
created in truncate(O_TRUNC) mode and if the type is 2 then the file is opened </br>
or created in append(O_APPEND) mode. If the file cannot be opened an error is </br>
returned. Finally the function uses dup2 to redirect the stdout to the file, the </br>
file is closed, and the function ends. </br></br>

# Freeing the memory
After a command is executed the memory used to store all of its arguments and </br>
pipline commands must be freed. The parent function that loops through main does </br>
this after each child/command is finished by running the freeCommands function. </br>
This function takes in the head pointer of the struct command and then uses a </br>
while loop to loop until the next pointer in command is null. Within the loop is </br>
uses free() to free up the memory for each of the arguments in the struct </br>
commandLineArg variable, the directory redirection variable of the struct </br>
commandLineArg (if not equal to NULL), the struct commandLineArg variable </br>
itself, as well as the struct command variable. </br>
