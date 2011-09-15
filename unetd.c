/* vim: ft=c ff=unix fenc=utf-8
 * file: udetd.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <signal.h>

#include <errno.h>

static volatile bool needStop = false;

int
submain (int sock, int argc, char **argv)
{
	if (argc)
	{
		/* change fds */
		close (STDOUT_FILENO);
		close (STDIN_FILENO);
		if (dup2 (sock, STDOUT_FILENO) != -1 && dup2 (sock, STDIN_FILENO) != -1)
		{
			close (sock);
			/* run program */
			execvp (argv[0], argv);
		}
		return errno;
	}
	return -1;
}

static void
sig (int signo)
{
	switch (signo)
	{
		case SIGCHLD:
			/* capture all dead children */
			wait (NULL);
			break;
		case SIGTERM:
		case SIGSTOP:
			/* set stop state */
			needStop = true;
			break;
		default:
			break;
	}
	signal (signo, sig);
}

void
usage (char *self)
{
	fprintf (stderr, "usage: %s <socket_path> "\
			"<program> [<arg0> <argn> ...]\n", self);
}

int
main (int argc, char *argv[])
{
	int sock = -1;
	int nsock = -1;
	int errn_ = 0;
	pid_t pid;
	struct sockaddr_un addr;
	socklen_t addrlen;
	if (argc < 3)
	{
		usage (argv[0]);
		return -1;
	}
	/* register signal handler, for prevent rise of zombies */
	signal (SIGCHLD, sig);
	signal (SIGSTOP, sig);
	signal (SIGTERM, sig);
	/* remove old socket */
	unlink (argv[1]);
	do
	{
		/* create socket */
		sock = socket (AF_UNIX, SOCK_STREAM, 0);
		if (sock == -1)
		{
			errn_ = errno;
			break;
		}
		/* prepare data for bind */
		memset (&addr, 0, sizeof(addr));
		addr.sun_family = AF_UNIX;
		memcpy (addr.sun_path, argv[1], sizeof (addr.sun_path) - 1);
		/* bind socket */
		errn_ = bind (sock, (struct sockaddr*)&addr, sizeof (addr));
		if (errn_ == -1)
		{
			errn_ = errno;
			break;
		}
		errn_ = listen (sock, 1);
	} while (0);
	fprintf (stderr, "sock = %d\n", sock);
	/* wait for connections */
	while (!errn_ && !needStop)
	{
		nsock = accept (sock, (struct sockaddr*)&addr, &addrlen);
		if (nsock == -1)
		{
			if (errno != EAGAIN && errno != EINTR)
			{
				errn_ = errno;
				break;
			}
		}
		else
		{
			pid = fork ();
			if (!pid)
			{
				/* i'm fork */
				close (sock);
				errn_ = submain (nsock, argc - 2, &(argv[2]));
				close (nsock);
				exit (errn_);
			}
			else
			if (pid > 0)
			{
				close (nsock);
				/* i'm parent */
			}
			else
			{
				/* fork fail */
				fprintf (stderr, "FaILed FOrk\n");
				close (nsock);
			}
		}
	}
	/* close socket */
	close (sock);
	/* remove socket from fs */
	unlink (argv[1]);
	if (needStop == true)
		return 0;
	else
		return errn_;
}

