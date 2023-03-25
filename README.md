# Project 3

# Samed Ganibegovic
# Operating Systems CS4760

# Assignment 3: Message Queues and Palindromes
# Due: March 15th 2021

--------------------------------------------------------------------------------------------------------

Brief Description: 

You will be required to create a separate child process from your main process.  
That is, the main process will just spawn the child processes and wait for messages from them.  
The main process also sets a timer at the start of computation to 25 seconds.  
If computation has not finished by this time, the main process kills all the spawned processes and then exits. 
Make sure that you print appropriate message(s). 
In addition,  the main process should print a message when an interrupt signal (^C) is received.  
All the children should be killed as a result.  
All other signals you can ignore (do not handle).  
As a precaution, add this feature only after your program is well debugged. 
The code for the child processes should be compiled separately and the executables be called Coordinator and palin.

--------------------------------------------------------------------------------------------------------

Invoking the solution:

coordinator should take in several command line options as follows:

coordinator -h 
coordinator [-h] [-c i] [-m j] datafile

-h          Describe how the project should be run and then, terminate.
-c i        Indicate how many child processes should be launched in total.
-m j        Indicate the maximum number of children j allowed to exist in the system at the same time.  (Default 20)
datafile    Input file containing the palindromes, one per line.


--------------------------------------------------------------------------------------------------------

How to run the project: 

First, you will need to type "make" into the command prompt.
This will create the necessary .o files as well as the executable files.
Then, you type ./coordinator followed by any of the above options. *if no datafile is given, an error message will be prompted*
Now, the program will run as expected.
To clean the project, type "make clean".

--------------------------------------------------------------------------------------------------------

Issues and problems with the program:

Issues I had were reading for files and implementing message queues.

--------------------------------------------------------------------------------------------------------

Link to version control:

https://github.com/sg-21/cs4760_project3 
