# COMP 310

### Shell

C program that implements a shell interface, similar to the UNIX shell, that accepts user commands and executes each of them in a separate process.

**Features:**

- Execute commands with parameters such as `cat text.txt`
- Execute commands in the background by appending `&` to them
- Access the 10 most recently entered commands using `history #` where `#` is the command index
- 5 built-in commands, `cd`, `pwd`, `exit`, `fg` and `jobs`, which are similar to their UNIX counterparts
- Output redirection to files using `>`

### Print Spooler

C program that implements a simplified print spooler, where a printer server processes jobs (printing strings) sequentially from a queue populated by the printer clients. In order to do this, multiple processes are used: one for the server and one for each client. Inter-process communication is done through shared memory.

### File system

C program that implements a simple filesystem that can be mounted by the user under a directory in the user's machine. The SFS has many limitations compared to the real file systems such as restricted filename lengths, no subdirectories (only root directory), no user concept, no protection among files, etc. The program implements the FUSE interface, which allows this filesystem to be mounted in Linux.

**Features:**

- Open files, creating them if they don't exist
- Close files
- Read and write to files
- Retrieve file sizes
- Remove files
