/*-----------------
Programmer: Darrin McAdams
Class: CIS3207-004
Professor: Dr. J.Payton
Date:10/17/2018

Purpose:

This is a unix-like shell for issuing commands through the terminal.
  It features basic in-house functions like cd, ls, and echo. Basic piping, 
  I/O redirection, background execution, command line args, and .sh execution
  are supported.

-------------------*/
#include<dirent.h>
#include<fcntl.h>
#include<pwd.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

#include<sys/types.h>
#include<sys/wait.h>

#include<readline/readline.h>
#include<readline/history.h>

#define FALSE 0
#define TRUE 1
//size of input buffer
#define BUFF 1024
//max args in a command
#define MAX_ARGS 20

/*-----------------
Output Color Codes
-------------------*/

#define RED     "\x1b[31m"
#define YELLOW  "\x1b[33m"
#define GREEN   "\x1b[32m"
#define BLUE    "\x1b[34m"
#define RESET   "\x1b[0m"

/*-----------------
Function Prototypes
-------------------*/

void parse_input(char *input, char *args[MAX_ARGS]);
void process_input(char *args[MAX_ARGS]);
void check_IO(char *args[MAX_ARGS]);
void redirect(char **args);
void check_background(char *args[MAX_ARGS]);
void check_pipes(char *args[MAX_ARGS]);
void piping(char **args);
void batch_commands(char **args);
int check_script(char *arg);
void run_script(char *arg);
void external_prog(char **args);
char *get_prompt();
char *get_dir();
void change_dir(char *newdir);
void list_dir(char **args);
void clear();
void echo(char **args);
void environ();
void escape();
void help();
void pause_cmd();
void shell_loop();

/*-----------------
Global Variables
-------------------*/

//for background execution
int background;
int status;

//for pipes
int piped;
char **args2;

//for input/output redirection
int input_redir;
int output_redir;
int append_redir;

//file names for i/o redirection
char *input_file;
char *output_file;

/*-----------------
Input Processing
-------------------*/

//breaks the input up into individual commands
void parse_input(char *input, char *args[MAX_ARGS]){

  args[0] = strtok(input, " \n\t");
  for (int i = 1; i < MAX_ARGS-1; i++){
    args[i] = strtok(NULL, " \n\t");
    if (args[i] == NULL)
      return;
  }
}


//processes the input and execute the desired commands
void process_input(char *args[MAX_ARGS]){
  //change directory command
  if (!strcmp(args[0], "cd") || !strcmp(args[0], "chdir")) {
    change_dir(args[1]);
  }

  //clear command
  else if (!strcmp(args[0], "clear") || !strcmp(args[0], "clr")) {
    clear();
  }
  //echo command
  else if (!strcmp(args[0], "echo")) {
    echo(args);
  }
  //exit command
  else if (!strcmp(args[0], "exit") || !strcmp(args[0], "quit") ) {
    escape();
  }
  //help
  else if (!strcmp(args[0], "help")) {
    help();
  }
  //list command
  else if (!strcmp(args[0], "ls") || !strcmp(args[0], "dir")) {
    list_dir(args);
    return;
  }
  //pause 
  else if (!strcmp(args[0], "pause")) {
    pause_cmd();
  }
  //environ
  else if (!strcmp(args[0], "environ")) {
    environ();
  }
  //else run external program
  else {
    external_prog(args);
  }
}

/*-----------------
I/O Redirection
-------------------*/

//check for I/O redirection commands in input
void check_IO(char **args){
  //reset global variables
  input_redir = FALSE;
  output_redir = FALSE;
  append_redir = FALSE;

  int i = 0;
  //used to move around args
  char **temp = args;
  //loop through args untill empty or I/O redirection found
  while (args[i] != NULL && i < MAX_ARGS){

    //if output redirection ">"
    if (!strcmp(args[i], ">")){
      output_redir = TRUE;
      //remove redirection arg
      args[i] = NULL;
      //save output file arg
      output_file = args[++i];
    }

    //if output append ">>"
    if(!strcmp(args[i], ">>")){
      append_redir = TRUE;
      //remove redirection arg
      args[i] = NULL;
      //save output file arg
      output_file = args[++i];
    }

    //if input redirection "<"
    if (!strcmp(args[i], "<")){
      input_redir = TRUE;
      //remove redirection arg
      args[i] = NULL;
      //save input file arg
      input_file = args[++i];
    }
  //next arg
  i++;
  }
}

void redirect(char **args){
  //if input redirection
  if (input_redir == TRUE){
    //open input file
    int in = open(input_file,O_RDONLY);
    //if file not found
    if (in < 0){
      //error message
      puts("Error: Input file not found");
      //crash
      exit(1);
    }

    //replace stdin with input file
    dup2(in,STDIN_FILENO);
    //close file
    close(in);
  }

  //if output redirection
  if (output_redir == TRUE){
    //open output file
    int out = open(output_file,O_WRONLY|O_CREAT,0666); // Should also be symbolic values for access rights
    //if file not found
    if (out < 0){
      //error message
      puts("Error: Output file not found");
      //quit
      exit(0);
    }   
    //replace stdout with output file
    dup2(out,STDOUT_FILENO);
    //close output file
    close(out);
    //execute args
  }

  //if appending output redirection
  else if (append_redir == TRUE){
    //open output file in append mode
    int out = open(output_file, O_WRONLY|O_APPEND|O_CREAT,0666); // Should also be symbolic values for access rights
    if (out < 0){
      //error message
      puts("Error: Output file not found");
      //quit
      exit(0);
    }
    //replace stdout with output file
    dup2(out,STDOUT_FILENO);
    //close output file
    close(out);
    //execute args

  }
  //run commands
  external_prog(args);
}

/*-----------------
Background Execution
-------------------*/

//also checks for background execution command in input "&"
void check_background(char *args[MAX_ARGS]){
  //reset global variable to default state
  background = FALSE;

  //loop through args until "&" is found, or end of args
  int i = 0;
  while (args[i] != NULL){
    //if background char is found
    if (strcmp(args[i], "&") == 0){
      background = TRUE ;
      args[i] = NULL;
      puts("Background execution");
      break;
    }
    i++;
  }
}

/*-----------------
Piping
-------------------*/

//check for any piping commands
void check_pipes(char *args[MAX_ARGS]){
  //reset piped value
  piped = FALSE;
  args2 = args;

  //loop until pipe command "|" or end of args found
  while (*args2 != NULL){
    //if pipe command found
    if (strcmp(*args2, "|") == 0){
      //set piped to true
      piped = TRUE;
      //set "|" to null, seperates args and args2
      *args2 = NULL;
      //move args2 to next argument
      args2++;
      //exit function
      return;
    }
    //next arg
    args2++;
  }  
}


void piping(char **args){
  //pipe file descriptors
  int pfds[2];

  //create pipe
  if ( pipe(pfds) == 0 ) {
    //fork 
    pid_t pid = fork();
    //if fork failed
    if (pid < 0){
      puts("Error: Fork failed");
      exit(1);
    }
    //else if child
    else if ( pid == 0 ) {

      // close stdout
      close(1);
      //use pipe's write end as stdout
      dup2( pfds[1], 1 );
      //close read end of pipe
      close( pfds[0] );
      //execute command
      process_input(args);
      //kill child
      exit(0);
    }
    //else parent
    else {
      //close stdin
      close(0);
      //use pipe's read end as stdin
      dup2( pfds[0], 0 );
      //close write end of pipe
      close( pfds[1] );
      //execute  command
      process_input(args2);
      //kill parent
      exit(0);
    }
  }
}

/*-----------------------
Scripts & Batch Commands
-----------------------*/

//execute batch commands
void batch_commands(char **args){
  //check for IO redirection
    check_IO(args);
    //check for background execution
    check_background(args);
    //check for pipe commands
    check_pipes(args);

    //if command is a script file
    if (check_script(args[0])){
      run_script(args[0]);
      //end commands
      return;
    
    }
    //if pipe command was detected
    if (piped == TRUE){
      //fork
      pid_t pipe_pid = fork();
      //if fork failed
      if (pipe_pid < 0){
        puts("fork failed");
        exit(0);
      }
      //if child
      else if (pipe_pid == 0){
        //run piped commands
        piping(args);
      } 
      //else parent
      else{
        //wait for child to finish
        waitpid(pipe_pid, &status, 0);
      }
    }//end if

    //if I/O redirection was found
    else if (input_redir == TRUE || output_redir == TRUE || append_redir == TRUE){
      //fork
      pid_t io_pid = fork();
      //if fork failed
      if (io_pid < 0){
        puts("fork failed");
        exit(0);
      }
      //if child
      else if (io_pid == 0){
        //run piped commands
        redirect(args);
        //kill child process
        exit(0);
      } 
      //else parent
      else{
        //wait for child to finish
        waitpid(io_pid, &status, 0);
      }
    }
    //else no pipe or i/o redirection
    else{
      //just run args
      process_input(args);
    }
}

//check if command is a script file ".sh"
int check_script(char *arg){
  //create temp pointer to go through arg
  char *temp = arg;
  //check the third to last letter of arg
  while (*(temp+3) != '\0'){
    temp++;
  }
  //if last three chars = ."sh"
  if (!strcmp(temp, ".sh"))
    //return true
    return TRUE;
  //else return false
  else 
    return FALSE;
}

//run each line of a .sh file
void run_script(char *arg){
  //open file
  FILE *file = fopen(arg, "r");
  //if file opened
  if (file != NULL){
    //used to hold each line of the file
    char buffer[BUFF];
    //read next line untill end of file
    while (fgets(buffer, sizeof(buffer), file) != NULL){
      char *dir = get_dir();
      printf("\n<SCRIPT>\n%s%s\n", dir, buffer); 
      //break up line into individual args
      char *args[MAX_ARGS];
      parse_input(buffer, args);
      batch_commands(args);
      //cleanup
      free(dir);
    }
    //close file
    fclose(file);
    return;
  }
  //if file could not be opened
  else{
    printf("Error: %s could not be opened", arg);
  }
}

/*-----------------
External Execution
-------------------*/

//handles execution of external programs
void external_prog(char **args){
  //get the main command
  char *temp = *args;
  //move to rest of args
  int status;
  //fork
  pid_t pid = fork();
  //if fork failed
  if (pid < 0){
    //error message
    puts("Error: fork failed");
    exit(1);
  }

  //else if child
  else if (pid == 0){
    //try to run command
    if (execvp(args[0], args) < 0){
      //error message if failed
      puts("Error: Command not recognised");
    }

    //child exits
    exit(0);
  }

  //else parent
  else{
    //if background execution not enabled
    if (background != TRUE){
      //wait for child to finish
      waitpid(pid, &status, 0);
    }
    return;
  }
}

/*-----------------
Helper Functions
-------------------*/

//Get shell prompt
char *get_prompt(){
  //holds string containing directory
  char cwd[BUFF];
  //copy current directory into string
  getcwd(cwd, sizeof(cwd));
  //get user's login
  char *login = getpwuid(getuid())->pw_name;
  //used for formating
  char *temp1 = malloc(sizeof(char)* 2 * BUFF);
  char *temp2 = malloc(sizeof(char)*BUFF);
  //add color to strings
  sprintf(temp1, GREEN "%s:" RESET , login);
  sprintf(temp2, BLUE "%s>" RESET, cwd);
  //combine strings
  strcat(temp1, temp2);
  //return string
  return temp1;
}

//just get the current directory
char *get_dir(){
  //holds string containing directory
  char *cwd = malloc(sizeof(char)*BUFF);
  //copy current directory into string
  getcwd(cwd, sizeof(cwd));
  //return cwd
  return cwd;
}
/*-----------------
Built-In commands
(cd, clr, ls, ect.)
-------------------*/

//change the current directory
void change_dir(char *newdir){
  //change current directory to input string
  if(chdir(newdir)){
    //if change_dir() failed
    puts("Error: directory not found");
  }
}

//list contentents of the directory
void list_dir(char **args){
  //get next arg
  args++;
  //holds current file's name
  char *temp;
  //holds size
  int size = 2;
  //holds all elements in dir as a string
  char *buffer = malloc(sizeof(char)*size);
  //directory struct
  DIR *d;
  struct dirent *dir;
  //open directory
  d = opendir(".");
  //if directory found
  if (d){
    //loop untill end of file
    while ((dir = readdir(d)) != NULL){
      //get name of file
      temp = dir->d_name;
      //if name begins with '.'
      if (*temp == '.'){
        //and no "-a" arg 
        if (*args != NULL && !strcmp(*args, "-a") != 0){
          //print filename
          printf("%s\n", dir->d_name);
        }
      }
      //else just print the filename
      else{
        printf("%s\n", dir->d_name);
      }
    }//end while

    //close directory
    closedir(d);
  }
}

//clears the terminal
void clear(){
  printf("\033[H\033[J");
}

//returns the input as a string
void echo(char **args){
  //skip "echo" part of command
  args++;
  
  //make sure there is text to output
  if (args!= NULL){
    //print first arg
    printf("%s", *args);
    //move to next arg
    args++;

    //loop until end of args
    while (*args != NULL){
      //print current arg
      printf(" %s", *args);
      //move to next arg
      args++;
    }
  }
  //new line
  puts("");
}

//exit the program
void escape(){
  exit(0);
}

//list environment variable
void environ(){
  //get PATH variable
  const char *s = getenv("PATH");
  //if path is NULL
  if (s == NULL){
    puts("PATH not found");
  }
  //else print out path
  else{
    printf("PATH = %s\n", s);
  }
}

//displays a list of commands
void help(){
puts(" _________________________________________________________________________________________");
puts("|   Command      |                       Purpose                                          |");
puts("|_________________________________________________________________________________________|");
puts("| cd, chdir      | Changes current directory                                              |");
puts("|-----------------------------------------------------------------------------------------|");
puts("| clear, clr     | Clears the terminal                                                    |");
puts("|-----------------------------------------------------------------------------------------|");
puts("| echo           | Prints out the rest of the args                                        |");
puts("|-----------------------------------------------------------------------------------------|");
puts("| environ        | Prints out the current environment variables                           |");
puts("|-----------------------------------------------------------------------------------------|");
puts("| exit, quit     | Exit the shell                                                         |");
puts("|-----------------------------------------------------------------------------------------|");
puts("| ls, dir        | Outputs the contents of the current directory. Files beginning with \".\"|");
puts("|                |    hidden unless the \"-a\" arg is used                                  |");
puts("|-----------------------------------------------------------------------------------------|");
puts("| pause          | Pauses the shell untill the enter key is pressed.                      |");
puts("|-----------------------------------------------------------------------------------------|");
puts("| f1 | f2        | Pipes the output from f1 into f2                                       |");
puts("|-----------------------------------------------------------------------------------------|");
puts("| f < input      | Redirects f's input to input                                           |");
puts("|-----------------------------------------------------------------------------------------|");
puts("| f > output     | Redirects f's output to output                                         |");
puts("|-----------------------------------------------------------------------------------------|");
puts("| f >> output    | Appends f's output to output                                           |");
puts("|-----------------------------------------------------------------------------------------|");
puts("| script.sh      | Will attempt to find a .sh file named script, and execute it's commands|");
puts("|_________________________________________________________________________________________|");
puts("\nThe shell will attempt to run external commands using the exec function");
}

//pauses the terminal until the enter key is pressed
void pause_cmd(){
  //uses getpass to hide typed input
  char *temp = getpass("Please press the enter key.");
  free (temp);
}

void test(){
  //testing clear
  puts("Blah blag b\nlah lalala You should\n't \tsee\nany of \t\t\t\tthis\n stuff");
  clear();

  //prompt, ls and cd
  char *a[2] = {"a", "-a"};
  char *b[2] = {"b", "-b"};
  puts("-----------------------------\nTesting prompt, ls, and cd\n-----------------------------");
  printf("%s\n", get_prompt());
  change_dir("..");
  list_dir(b);
  printf("%s\n", get_prompt());
  change_dir("./MyShell");
  list_dir(a);
  printf("%s\n", get_prompt());


  //I/O, Pipe, and Background detection
  puts("-----------------------------\ntesting for I/O, pipe, and background commands\n-----------------------------");
  
  char *args[MAX_ARGS];
  char s1[] = "cat < in.txt > out.txt";
  printf("string 1: %s\n", s1);
  parse_input(s1, args);
  puts("1");
  check_IO(args);
  check_background(args);
  check_pipes(args);
  if (input_redir == TRUE){
    printf("Input file: %s \n", input_file);
  }
  if (output_redir == TRUE){
    printf("Output file: %s \n", output_file);
  }
  if (append_redir == TRUE){
    printf("Appended output file: %s \n", output_file);
  }
  if (background == TRUE){
    puts("background execution enabled");
  }
  if (piped == TRUE){
    puts("Pipe command found");
  }

  char s2[] = "cat >> out.txt";
  printf("string 2: %s\n", s2);
  parse_input(s2, args);
  check_IO(args);
  check_background(args);
  check_pipes(args);
  if (input_redir == TRUE){
    printf("Input file: %s \n", input_file);
  }
  if (output_redir == TRUE){
    printf("Output file: %s \n", output_file);
  }
  if (append_redir == TRUE){
    printf("Appended output file: %s \n", output_file);
  }
  if (background == TRUE){
    puts("background execution enabled");
  }
  if (piped == TRUE){
    puts("Pipe command found");
  }

  char s3[] = "echo hello world | wc -l";
  printf("string 3: %s\n", s3);
  parse_input(s3, args);
  check_IO(args);
  check_background(args);
  check_pipes(args);
  if (input_redir == TRUE){
    printf("Input file: %s \n", input_file);
  }
  if (output_redir == TRUE){
    printf("Output file: %s \n", output_file);
  }
  if (append_redir == TRUE){
    printf("Appended output file: %s \n", output_file);
  }
  if (background == TRUE){
    puts("background execution enabled");
  }
  if (piped == TRUE){
    puts("Pipe command found");
  }
  
  exit(0);
}
//main loop for regular shell use
//follows a cycle of fetch -> parse -> execute 
void shell_loop(){
  while (TRUE){
    //holds input
    char *input;
    char *args[MAX_ARGS];
    //print out terminal promt;
    //get prompt string
    char *prompt = get_prompt();
    //get input
    input = readline(prompt);
    add_history(input);
    //parse the input
    parse_input(input, args);
    //check for shell script file ".sh"
    if (check_script(args[0])){
      run_script(args[0]);
      //clean up
      free(input);
      //restart loop
      continue;
    }
    //check for IO redirection
    check_IO(args);
    //check for background execution
    check_background(args);
    //check for pipe commands
    check_pipes(args);
    
    //if pipe command was detected
    if (piped == TRUE){
      //fork
      pid_t pipe_pid = fork();
      //if fork failed
      if (pipe_pid < 0){
        puts("fork failed");
        exit(0);
      }
      //if child
      else if (pipe_pid == 0){
        //run piped commands
        piping(args);
      } 
      //else parent
      else{
        //wait for child to finish
        waitpid(pipe_pid, &status, 0);
      }
    }//end if

    //if I/O redirection was found
    else if (input_redir == TRUE || output_redir == TRUE || append_redir == TRUE){
      //fork
      pid_t io_pid = fork();
      //if fork failed
      if (io_pid < 0){
        puts("fork failed");
        exit(0);
      }
      //if child
      else if (io_pid == 0){
        //run piped commands
        redirect(args);
        //kill child process
        exit(0);
      } 
      //else parent
      else{
        //wait for child to finish
        waitpid(io_pid, &status, 0);
      }
    }
    //else no pipe or i/o redirection
    else{
      //just run args
      process_input(args);
    }
    //cleanup
    free(input);
    free(prompt);
  }
}

int main(int argc, char **argv){
  //if there are batch commands
  if(argc > 1){
    //run commands
    batch_commands(++argv);
    //quit
    exit(0);
  }
  //test();
  //start main loop of shell
  shell_loop();
}