# Educational Project: Simple Command Line Interpreter (Shell)

This project is an educational implementation of a custom command line interpreter (a shell similar to bash/zsh), developed based on the Yandex School of Data Analysis (YSDA / ШАД) Computer Architecture course materials. The project demonstrates the practical application of POSIX system calls for process management, I/O redirection, and pipeline organization.

## Project Structure

* [shell.c](shell.c) — the shell implementation source code in C.
* [test_shell.c](test_shell.c) — unit tests verifying string parsing and preprocessing.
* [Makefile](Makefile) — build configuration for the project.

## Core Implementation Steps

According to the lecture, the development of the shell is divided into three incremental stages:

### 1. Basic Shell (Fork + Exec + Wait)
In the [main](shell.c#L179) function, the program reads user input in an infinite loop. To execute an external command:
1. `fork()` is called to spawn a new child process.
2. In the child process, `execvp()` is called to replace the process image with the new program.
3. In the parent process, `waitpid()` is called to suspend shell execution until the child process completes. This prevents the creation of zombie processes.

### 2. Input/Output Redirection (< and >)
Syntax constructions like `cmd > file` and `cmd < file` are handled in the [parse_single_command](shell.c#L50) function, which extracts filenames from the token stream and stores them in the [Command](shell.c#L43) structure.
Inside the child process before calling `execvp`:
1. The target file is opened using the `open()` system call.
2. The standard streams `stdin` (0) or `stdout` (1) are redirected to the file descriptor of the opened file using `dup2()`.
3. The original file descriptor is closed using `close()`.

### 3. Pipeline (Pipe, the | symbol)
To chain commands into pipelines like `cmd1 | cmd2 | ... | cmdN`, the [execute_pipeline](shell.c#L79) function:
1. Creates unidirectional channels using the `pipe()` system call. A total of `N-1` pipes are created.
2. For each process, `dup2()` is used to redirect standard streams: the output of the previous command is connected to the input of the next one.
3. All unused pipe file descriptors are closed in all processes. If a write descriptor remains open in any process, the reading process will never receive End-of-File (EOF) and will hang indefinitely.

## Build, Run, and Test

To compile the project, run:
```bash
make
```

To start the shell:
```bash
./shad_shell
```

To run the unit tests:
```bash
make test
```

To clean up build artifacts:
```bash
make clean
```

## Testing Examples

After launching `./shad_shell`, you can verify the implementation with the following scenarios:

1. **Simple commands:**
   ```bash
   shad_shell> ls -la
   shad_shell> uname -a
   ```

2. **Redirecting output to a file:**
   ```bash
   shad_shell> ls -la > output.txt
   shad_shell> cat output.txt
   ```

3. **Redirecting input from a file:**
   ```bash
   shad_shell> wc -l < output.txt
   ```

4. **Multi-command pipeline:**
   ```bash
   shad_shell> cat shell.c | grep include | wc -l
   ```

5. **Built-in commands:**
   ```bash
   shad_shell> cd ..
   shad_shell> exit
   ```
