// SPDX-License-Identifier: BSD-3-Clause

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cmd.h"
#include "utils.h"

#define READ 0
#define WRITE 1


static bool shell_cd(word_t *dir)
{
	return chdir(get_word(dir));
}

static int shell_exit(void)
{
	exit(EXIT_SUCCESS);
}

static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	if (s->verb->next_part != NULL) {
		const char *val = NULL;

		if (strcmp(s->verb->next_part->string, "=") == 0 && strcmp(s->verb->next_part->next_part->string, "USER") == 0) {
			char *user = NULL;

			user = getenv("USER");
			if (user != NULL)
				val = strcat(user, s->verb->next_part->next_part->next_part->string);
		} else {
			val = s->verb->next_part->next_part->string;
		}
		return setenv(s->verb->string, val, 1);
	}

	if (strcmp(get_word(s->verb), "cd") == 0) {
		if (s->out != NULL && s->err != NULL) {
			int output_fd;

			if (s->io_flags == IO_OUT_APPEND)
				output_fd = open(get_word(s->out), O_WRONLY | O_CREAT | O_APPEND, 0666);
			else
				output_fd = open(get_word(s->out), O_WRONLY | O_CREAT | O_TRUNC, 0666);

			close(output_fd);
		} else if (s->out != NULL) {
			int output_fd;

			if (s->io_flags == IO_OUT_APPEND)
				output_fd = open(get_word(s->out), O_WRONLY | O_CREAT | O_APPEND, 0666);
			else
				output_fd = open(get_word(s->out), O_WRONLY | O_CREAT | O_TRUNC, 0666);

			close(output_fd);
		} else if (s->err != NULL) {
			int error_fd;

			if (s->io_flags == IO_ERR_APPEND)
				error_fd = open(get_word(s->err), O_WRONLY | O_CREAT | O_APPEND, 0666);
			else
				error_fd = open(get_word(s->err), O_WRONLY | O_CREAT | O_TRUNC, 0666);

			close(error_fd);
		}
		return shell_cd(s->params);
	}

	if (strcmp(get_word(s->verb), "exit") == 0 || strcmp(get_word(s->verb), "quit") == 0)
		return shell_exit();

	pid_t pid = fork();

	if (pid == 0) {
		if (s->in != NULL)
			dup2(open(get_word(s->in), O_RDONLY), STDIN_FILENO);
		if (s->out != NULL && s->err != NULL) {
			int output_fd, error_fd;

			if (strcmp(get_word(s->out), get_word(s->err)) == 0) {
				if (s->io_flags == IO_OUT_APPEND)
					output_fd = open(get_word(s->out), O_WRONLY | O_CREAT | O_APPEND, 0666);
				else
					output_fd = open(get_word(s->out), O_WRONLY | O_CREAT | O_TRUNC, 0666);

				dup2(output_fd, STDOUT_FILENO);
				dup2(output_fd, STDERR_FILENO);
				close(output_fd);
			} else {
				if (s->io_flags == IO_OUT_APPEND)
					output_fd = open(get_word(s->out), O_WRONLY | O_CREAT | O_APPEND, 0666);
				else
					output_fd = open(get_word(s->out), O_WRONLY | O_CREAT | O_TRUNC, 0666);
				if (s->io_flags == IO_ERR_APPEND)
					error_fd = open(get_word(s->err), O_WRONLY | O_CREAT | O_APPEND, 0666);
				else
					error_fd = open(get_word(s->err), O_WRONLY | O_CREAT | O_TRUNC, 0666);

				dup2(output_fd, STDOUT_FILENO);
				close(output_fd);
				dup2(error_fd, STDERR_FILENO);
				close(error_fd);
			}
		} else if (s->out != NULL) {
			int output_fd;

			if (s->io_flags == IO_OUT_APPEND)
				output_fd = open(get_word(s->out), O_WRONLY | O_CREAT | O_APPEND, 0666);
			else
				output_fd = open(get_word(s->out), O_WRONLY | O_CREAT | O_TRUNC, 0666);

			dup2(output_fd, STDOUT_FILENO);
			close(output_fd);
		} else if (s->err != NULL) {
			int error_fd;

			if (s->io_flags == IO_ERR_APPEND)
				error_fd = open(get_word(s->err), O_WRONLY | O_CREAT | O_APPEND, 0666);
			else
				error_fd = open(get_word(s->err), O_WRONLY | O_CREAT | O_TRUNC, 0666);

			dup2(error_fd, STDERR_FILENO);
			close(error_fd);
		}
		int size;

		if (execvp(get_word(s->verb), get_argv(s, &size)) == -1) {
			fprintf(stderr, "Execution failed for '%s'\n", get_word(s->verb));
			exit(EXIT_FAILURE);
		}
		return 0;
	}
	int status;

	waitpid(pid, &status, 0);

	if (WIFEXITED(status))
		return WEXITSTATUS(status);

	return 0;
}



static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
							command_t *father)
{
	pid_t pid1;
	int status1;
	pid_t pid2;
	int status2;

	pid1 = fork();

	if (pid1 == 0) {
		exit(parse_command(cmd1, level + 1, father));
	} else {
		pid2 = fork();

		if (pid2 == 0) {
			exit(parse_command(cmd2, level + 1, father));
		} else {
			waitpid(pid1, &status1, 0);
			waitpid(pid2, &status2, 0);

			if (WIFEXITED(status1))
				return WEXITSTATUS(status1);

			if (WIFEXITED(status2))
				return WEXITSTATUS(status2);

			return 0;
		}
	}
}

static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
						command_t *father)
{
	int fd[2];
	pid_t pid1;
	int status1;
	pid_t pid2;
	int status2;

	if (pipe(fd) == -1)
		return false;

	pid1 = fork();

	if (pid1 == 0) {
		dup2(fd[WRITE], STDOUT_FILENO);
		exit(parse_command(cmd1, level, father));
	} else {
		pid2 = fork();

		if (pid2 == 0) {
			close(fd[WRITE]);
			dup2(fd[READ], STDIN_FILENO);
			close(fd[READ]);
			exit(parse_command(cmd2, level, father));
		} else {
			close(fd[READ]);
			close(fd[WRITE]);
			waitpid(pid1, &status1, 0);
			waitpid(pid2, &status2, 0);

			if (WIFEXITED(status1))
				return WEXITSTATUS(status1);

			if (WIFEXITED(status2))
				return WEXITSTATUS(status2);

			return 0;
		}
	}
}

int parse_command(command_t *c, int level, command_t *father)
{
	if (c->op == OP_NONE)
		return parse_simple(c->scmd, level, father);

	switch (c->op) {
	case OP_SEQUENTIAL:
		parse_command(c->cmd1, level, c);
		return parse_command(c->cmd2, level, c);

	case OP_PARALLEL:
		return run_in_parallel(c->cmd1, c->cmd2, level, c);

	case OP_CONDITIONAL_NZERO:
		if (parse_command(c->cmd1, level, c))
			return parse_command(c->cmd2, level, c);
		else
			return 0;

	case OP_CONDITIONAL_ZERO:
		if (!parse_command(c->cmd1, level, c))
			return parse_command(c->cmd2, level, c);
		else
			return 0;

	case OP_PIPE:
		return run_on_pipe(c->cmd1, c->cmd2, level, c);

	default:
		return SHELL_EXIT;
	}
}
