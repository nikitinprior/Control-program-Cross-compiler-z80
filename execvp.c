/*
 *	execv(name, argv)	(supplies environment)
 *	execvp(name, argv)	(like execv, but does path search)
 */

//#include <errno.h>
#include <string.h>
#include <unix.h>
#include "exec.h"

#if UZI
char * getenv();
int    execv(char *name, char **argv);
int    execve(char *name, char *argv[], char *envp[]);
int    execvp(char *name, char **argv);
char * execat(char *s1, char *s2, char *si);
#endif	//UZI

static char   shell[] =	"/bin/sh";

extern	      errno;
extern char **environ;

//**********************************************************************
// 	execv
//**********************************************************************
int execv(char *name, char **argv) {

    return(execve(name, argv, environ));
}

//**********************************************************************
// 	execvp
//**********************************************************************
int execvp(char *name, char **argv) {
    char *pathstr;
    register char *cp;
    int i;
    register eacces;
    char fname[128];
    char *newargs[256];

    eacces = 0;
    if ((pathstr = getenv("PATH")) == NULL)
	pathstr = ":/bin:/usr/bin";
    cp = strchr(name, '/')? "": pathstr;

    do {
	cp = execat(cp, name, fname);
//retry:
	execv(fname, argv);
	switch(errno) {
	case ENOEXEC:
	    newargs[0] = "sh";
	    newargs[1] = fname;
	    for (i = 1; newargs[i+1] = argv[i]; i++) {
		if (i >= 254) {
		    errno = E2BIG;
		    return(-1);
		}
	    }
	    execv(shell, newargs);
	    return(-1);
	case EACCES:
	    eacces++;
	    break;
	case ENOMEM:
	case E2BIG:
	    return(-1);
	}
    } while (cp);
    if (eacces)
	errno = EACCES;
    return(-1);
}

//**********************************************************************
// 	execat
//**********************************************************************
char * execat(char *s1, char *s2, char *si) {
    register char *s;

    s = si;
    while (*s1 && *s1 != ':' && *s1 != '-')
	*s++ = *s1++;
    if (si != s)
	*s++ = '/';
    while (*s2)
	*s++ = *s2++;
    *s = '\0';
    return(*s1? ++s1: (char *)0);
}
