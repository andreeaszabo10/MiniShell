Copyright Szabo Cristina-Andreea 2023-2024

# Mini Shell

## Overview

Mini Shell is a lightweight shell environment designed for Unix-like systems, focusing on fundamental command execution capabilities. It supports built-in shell commands, the execution of external programs, and essential shell features such as redirection, piping, and parallel execution of commands. This shell operates using system-level functions, providing a basic yet functional interface for interacting with the operating system.

## Features

### Built-in Commands
- **`cd <directory>`**: Changes the current working directory to the specified path.
- **`pwd`**: Displays the current working directory.
- **`exit`**: Terminates the shell session and exits the program.

### External Command Execution
- When a command is not recognized as a built-in, the shell will use `execvp()` to execute the command in a new child process. It supports argument passing and environment variable expansion, making it possible to run various external programs seamlessly.

### Redirection
- **`< filename`**: Directs input from a file into the command.
- **`> filename`**: Redirects the output of a command to a file (overwrites).
- **`>> filename`**: Appends the output to a file.
- **`2> filename`**: Redirects standard error to a file.
- **`&> filename`**: Redirects both output and error to a file.

### Piping (`|`)
- You can chain commands using the pipe operator, which connects the output of one command to the input of another. This is accomplished using anonymous pipes and file descriptor manipulation (`pipe()` and `dup2()`).

### Parallel Execution (`&`)
- Commands can be run concurrently in separate processes by appending `&` at the end of the command. The shell uses `fork()` to create child processes and `waitpid()` to ensure proper synchronization.

### Conditional Execution
- **`&&`**: Executes the next command only if the previous command succeeds (returns a zero exit status).
- **`||`**: Executes the next command only if the previous command fails (returns a non-zero exit status).

## Implementation Insights

- The shell operates through low-level system calls like `fork()`, `execvp()`, `pipe()`, and `dup2()` to create child processes, redirect inputs/outputs, and handle piping and command chaining.
- Environment variable expansion is performed by checking and substituting values in commands before execution.
- The shell supports basic job control, allowing background processes and sequential execution using operators like `;`, `&&`, `||`, and `&`.
