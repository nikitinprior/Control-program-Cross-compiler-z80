/*
 *	Copyright (C) 1984-1888 HI-TECH SOFTWARE
 *
 *	This software remains the property of HI-TECH SOFTWARE and is
 *	supplied under licence only. The use of this software is
 *	permitted under the terms of that licence only. Copying of
 *	this software except for the purpose of making backup or
 *	working copies for the use of the licensee on a single
 *	processor is prohibited.
 *
 *	This is a modified version for cross-compiler and compiler
 *	for UZI-180 (Unix Edition 7 for the Zilog Z80 CPU).
 *
 *	zc3 command
 *
 *	zc3 [-C] [-O] [-I] [-F] [-E] [-CPM] [-U] [-D] [-S] [-X] [-P] [-W] [-M] files {-Llib}
 *
 *	All options of Hi-Tech C compiler v3.09 for CP/M are used.
 *
 *	Added option "-E" to change the name of the output executable
 *	(compatible with Tony Nicholson's version).
 *
 *	Added option "-CPM" to compile source files for the CP/M operating
 *	system. By default without this option source files are compiled for
 *	the operating system UZI-180. In this case, crtuzi.obj and libcuzi.lib
 *	are used instead of crtcpm.obj and libc.lib. In addition, the output
 *	executable file is named without an extension. 
 *
 *	Added option "-H" to display available compiler options.
 *
 *	Intermediate work files are assigned unique names using the mktemp()
 *	function.
 *
 *	Creating overlay files is not supported.
 *
 *	The rest of the algorithm corresponds to the original Hi-Tech C compiler
 *	v3.09 for CP/M.
 *
 *	Until the source code of OBJTOHEX and CREF is reconstructed, the -CR
 *	and 'A' options are temporarily unavailable because they are excluded
 *	by conditional compilation. To include these options in the source code,
 *	delete comment from line
 *
 *	    #define OBJTOHEX_AND_CREF.
 *
 *	The absence of these options slightly reduces the functionality of
 *	the compiler.
 *
 *	The program compiles under Linux and MacOS operating systems.
 *
 *	When you run the program displays the name of the operating system in
 *	which it runs. 
 *
 * 	To create a control program the cross-compiler zc3 for Linux or MacOS
 *	use the command:
 *
 *	    gcc  -O2 -ozc3 zc3.c
 *
 *	To create a control program compiler zc3 for UZI-180:
 *
 *	    zc3 -O zc3.c execvp.c mktemp.c
 *
 *	Changes made by Andrey Nikitin on 21-08-2022.
 */

#define OBJTOHEX_AND_CREF	/* Delete comment to get full version of program. */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#if UZI

int   fork();
int   wait(int *wstatus);
char *mktemp(char *template);

#else

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#endif	//UZI

#ifndef uchar
#define uchar unsigned char
#endif

//**********************************************************************
//	Prototype functions
//**********************************************************************
int       main(int, char **);		// 1
void	  setup(void);			// 2
void	  doit(void);			// 3
void	  rm(char *);			// 4
void	  addobj(char *);		// 5
void	  addlib(char *);		// 6
void	  error(char *);		// 7
char    * xalloc(int);			// 8
int 	  unx_exec(char *, char **);	// 9
int	  doexec(char *, char **);	// 10
void	  assemble(char *);		// 11
void	  compile(char *);		// 12

#ifdef __APPLE__
#define	OS_HOST	"(for MacOS)\n"
#endif

#ifdef _WIN32
#define	OS_HOST	"(for Windows)\n"
#endif

#ifdef UZI
#define	OS_HOST	"(for UZI-180)\n"
#endif

#ifdef __linux__
#define	OS_HOST	"(for Linux)\n"
#endif

#define	MAXLIST	60		// max arg list
#define	BIGLIST	120		// room for much more

#define	PROMPT	"c"
#define	DEFPATH	""
#if 0					  		// cp/m ---+
#define	TEMP	""					//	   |
#define	DEFTMP	""					//	   |
#endif					  		// cp/m ---+
#define	LIBSUFF	".lib"		// library suffix

#define	LFLAGS	"-Z"
#define	STDLIB	"c"
#define	GETARGS	"-U__getargs"

char tmplate[] = "cc-XXXXXX";

static char	keep,		// Retain .obj files, don't link
		keepas,		// Retain .as files, don't assemble
		verbose,	// Verbose - echo all commands
		optimize,	// Invoke optimizer
		speed,		// Optimize for speed
#ifdef OBJTOHEX_AND_CREF
		reloc,		// Auto-relocate program at run time
		xref,		// Generate cross reference listing
#endif
		cpm = 0,	// 1 - produce CP/M .com file, 0 - UZI-180 executable (default)
	      * outmame,	// Specified output name
	        mark,		// 1 - indicates whether the output name is specified
		nolocal;	// Strip local symbols

static char   *	iuds[MAXLIST],	// -[IUD] args to preprocessor
	      *	objs[MAXLIST],	// .obj files and others for linker
	      *	flgs[BIGLIST],	// flags etc for linker
	      *	libs[MAXLIST],	// .lib files for linker
	      *	c_as[MAXLIST];	// .c files to compile or .as to assemble

static uchar 	iud_idx,	// Index into uids[]
		obj_idx,	//   "     "  objs[]
		flg_idx,	//   "     "  flgs[]
		lib_idx,	//   "     "  libs[]
		c_as_idx;	//   "     "  c_as[]

static char *	paths[] = {
    "linq3",			// 0
    "objtohex",			// 1
    "cgen3",			// 2
    "optim3",			// 3
    "cpp_new3",			// 4
    "zasx3",			// 5
    "lib", 			// 6 no lib suffix
    "p1x3",			// 7
    "crtuzi.obj",		// 8 Default for UZI180
    "cref3"			// 9
};

#define	linker		paths[0]
#define	objto		paths[1]
#define	cgen		paths[2]
#define	optim		paths[3]
#define	cpp		paths[4]
#define	assem		paths[5]
#define	libpath		paths[6]
#define	pass1		paths[7]
#define	strtoff		paths[8]
#define	cref		paths[9]

#define	RELSTRT	strtoff[plen]

static char   * temps[] = {
    ".1",			// 0
    ".2",			// 1
    ".3",			// 2
    ".4",			// 3
    ".obj",			// 4
    ".___",			// 5
    ".tmp"			// 6
};

#define	tmpf1		temps[0]
#define	tmpf2		temps[1]
#define	tmpf3		temps[2]
#define	redname		temps[3]
#define	l_dot_obj	temps[4]
#define	execmd		temps[5]
#define	crtmp		temps[6]

// Default for UZI180
static char	      *	cppdef[] = { "-DUZI", "-DHI_TECH_C", "-Dz80" };
// For CPM	      *	cppdef[] = { "-DCPM", "-DHI_TECH_C", "-Dz80" };

#define Def_Mode	0755		// Default mode of executable output files

static char	      *	cpppath = "-I";

static char		tmpbuf[128];	// Gen. purpose buffer
static char		single[40];	// Single object file to be deleted
static short		nfiles;		// Number of source or object files seen
static char	      *	outfile;	// Output file name for objtohex

static short		nerrs;		// Errors from passes
static short		plen;		// Length of path

#ifdef OBJTOHEX_AND_CREF
static char	      * xrname;
#endif

//**********************************************************************
// 1	Main
//**********************************************************************
int main(int argc, char ** argv) {
    register char * cp, * xp;
    short	    i;

    fprintf(stderr, "\nHI-TECH C COMPILER Z80 v3.09 ");
    fprintf(stderr, OS_HOST);
    fprintf(stderr, "Copyright (C) 1984-87 HI-TECH SOFTWARE\n");
//    fprintf(stderr, "Updated from https://github.com/nikitinprior/Cross-compiler\n");

#if EDUC
    fprintf(stderr, "Licensed for Educational purposes only\n");
#endif

    if(argc == 1)
#if 0					  		// cp/m ---+
	argv = _getargs((char *)0, PROMPT);		//	   |
#else					  		// cp/m ---+
	error("Options and files to compile are not specified. Use zc3 -h for help.\n");
//	exit(127); 
#endif

    setup();

    while(*++argv) {
	if(argv[0][0] == '-') {

	    if(islower(i = argv[0][1]))
		argv[0][1] = i = toupper(i);

	    switch(i) {
#ifdef OBJTOHEX_AND_CREF
	      case 'A':
			reloc   = 1;		// Auto-relocate program at run time
			RELSTRT = 'R';
			flgs[flg_idx++] = "-L";
			break;
#endif
	      case 'R':
			flgs[flg_idx++] = GETARGS;
			break;

	      case 'V':
			verbose = 1;		// Verbose - echo all commands
			break;

	      case 'S':
			keepas = 1;		// Retain .as files, don't assemble

	      case 'C':
			if(strcmp(argv[0], "-Cpm") == 0
			|| strcmp(argv[0], "-CPM") == 0) {
				cpm = 1;
//				cppdef[0] = "-DCPM";
				iuds[0]   = "-DCPM";
				objs[0]   = "crtcpm.obj";
				break;
			}
#ifdef OBJTOHEX_AND_CREF
			if(argv[0][2] == 'r'
			|| argv[0][2] == 'R') {
			    xref = 1;		// Generate cross reference listing
			    if(argv[0][3]) {
				xrname = &argv[0][1];
				xrname[0] = '-';
				xrname[1] = 'o';
			    } else
				xrname = (char *)0;
			} else
#endif
			    keep = 1;		// Retain .obj files, don't link
			break;

	      case 'E':
			mark    = 1;
			outmame = &argv[0][2];	// Specified output name
			break;

	      case 'O':
			optimize = 1;		// Invoke optimizer
			if(argv[0][2] == 'F'
			|| argv[0][2] == 'f')
			    speed = 1;		// Optimize for speed
			break;

	      case 'I':
	      case 'U':
	      case 'D':
			iuds[iud_idx++] = argv[0];
			break;

	      case 'L':
			addlib(&argv[0][2]);
			break;

	      case 'F':
			argv[0][1] = 'D';
			flgs[flg_idx++] = argv[0];
			break;

	      case 'X':
			nolocal = 1;		// Strip local symbols

	      case 'P':
	      case 'M':
	      case 'W':
			flgs[flg_idx++] = argv[0];
			break;
	      case 'H':
//			usage();
			fprintf(stderr, "\n      Hi-Tech Z80 C compiler V3.09 options:\n");
#ifdef OBJTOHEX_AND_CREF
			fprintf(stderr, "\t-A         Generate a self-relocating .COM program\n");
#endif
			fprintf(stderr, "\t-C         Generate object code only; don't link\n");
			fprintf(stderr, "\t-CPM       Compile for CP/M. Without this option for UZI-180\n");
#ifdef OBJTOHEX_AND_CREF
			fprintf(stderr, "\t-CRfile    Produce a cross-reference listing e.g. -CRfile.crf\n");
#endif
			fprintf(stderr, "\t-Dname     Defines name as 1., e.g. -DDEBUG=1\n");
			fprintf(stderr, "\t-Dname=var Defines name as var\n");
			fprintf(stderr, "\t-Efile     Specify executable output filename, e.g. -Efile\n");
			fprintf(stderr, "\t-Ffile     Generate a symbol file for debug.com (default L.SYM)\n");
			fprintf(stderr, "\t-H         Output help and exit\n");
			fprintf(stderr, "\t-Idir      Adds directory to the search path.\n");
			fprintf(stderr, "\t-L         Scan a library, e.g. -LF scans the floating point library\n");
			fprintf(stderr, "\t-M         Generate a map file, e.g. -Mfile.map\n");
			fprintf(stderr, "\t-O         Invoke the peephole optimizer (reduced code-size)\n");
			fprintf(stderr, "\t-OF        Invoke the optimizer for speed (Fast)\n");
			fprintf(stderr, "\t-S         Generate assembler code in a .as file; don't assemble or link\n");
			fprintf(stderr, "\t-Uname     Remove an initial definition of name e.g. -UDEBUG\n");
			fprintf(stderr, "\t-V         Be verbose during compilation\n");
			fprintf(stderr, "\t-Wn        Sets the line length in the map file to n\n");
			fprintf(stderr, "\t-X         Suppress local symbols in symbol tables\n");
			exit(0); // Ignore all other options for HELP

	      default:
			fprintf(stderr, "Unknown flag %s\n", argv[0]);
			exit(1);
	    }
	    continue;
	}

	nfiles++;

	cp = argv[0];

#if 0					  		// cp/m ---+
	while(*cp) {					//	   |
	    if(islower(*cp))				//	   |
		*cp = toupper(*cp);			//	   |
	    cp++;					//	   |
	}						//	   |
#endif					  		// cp/m ---+

	cp = strrchr(argv[0], '.');

	if(cp && (strcmp(cp, ".c") == 0 || strcmp(cp, ".as") == 0 )) {
	    c_as[c_as_idx++] = argv[0];
	    if( (xp = strrchr(argv[0], ':')) )
		xp++;
	    else
		xp = argv[0];

	    *cp = 0;

	    strcat(strcpy(tmpbuf, xp), ".obj");

	    addobj(tmpbuf);
	    strcpy(single, tmpbuf);
	    *cp = '.';
	} else
	    addobj(argv[0]);
    }
    doit();
    exit(nerrs != 0);
}

//**********************************************************************
// 2	Setup
//**********************************************************************
void setup(void) {
    register char * cp;
    short	    i, len;

//printf("\ntest setup start\n");

    cp = DEFPATH;

    plen = strlen(cp);

    umask(077);  // Only user has access to temp files

    cpppath = strcat(strcpy(xalloc(plen+(int)strlen(cpppath)+1), cpppath), cp);

    for(i = 0 ; i < sizeof paths/sizeof paths[0] ; i++) {
	paths[i] = strcat(strcpy(xalloc(plen+(int)strlen(paths[i])+1), cp), paths[i]);
    }

    cp = mktemp(tmplate);	/* UZI180 version of temp files */
    if(cp) {
	len = strlen(cp);
	for(i = 0 ; i < sizeof temps/sizeof temps[0] ; i++) {
	    temps[i] = strcat(strcpy(xalloc(len+(int)strlen(temps[i])+1), cp), temps[i]);
	}
    }

    objs[0] = paths[8]; //strtoff;
    obj_idx = 1;
    flgs[0] = LFLAGS;	// "-Z"
    flg_idx = 1;

    for(i = 0 ; i < sizeof (cppdef) / (sizeof cppdef[0]) ; i++) {
	iuds[i] = cppdef[i];

//printf("test setup  iuds[%d] = %s\t cppdef[%d] = %s\n", i, iuds[i], i, cppdef[i]);
    }
    iud_idx = i;

//printf("test setup end\n");
}

//**********************************************************************
// 3	doit
//**********************************************************************
void doit(void) {
    register char * cp;
    register uchar  i;

#ifdef OBJTOHEX_AND_CREF
    if(xref)
	close(creat(crtmp, 0600));
#endif

    close(2);
    dup(1);

    iuds[iud_idx++] = cpppath;

    for(i = 0 ; i < c_as_idx ; i++) {
	cp = strrchr(c_as[i], '.');

	if(strcmp(cp, ".c") == 0) 
	    compile(c_as[i]);
	else
	    assemble(c_as[i]);
    }

    rm(tmpf1);
    rm(tmpf2);
    rm(tmpf3);

    if(!(keep || nerrs)) {
	flgs[flg_idx++] = "-Ptext=0,data,bss";
#ifdef OBJTOHEX_AND_CREF
	if(reloc) {
	    flgs[flg_idx++] = strcat(strcpy(xalloc((int)strlen(l_dot_obj)+3), "-o"), l_dot_obj);
	} else {
#endif
	    flgs[flg_idx++] = "-C100H";


	    flgs[flg_idx++] = strcat(strcpy(xalloc((int)strlen(outfile)+3), "-O"), outfile);


#ifdef OBJTOHEX_AND_CREF
	}
#endif

	for(i = 0 ; i < obj_idx ; i++) {
	    flgs[flg_idx++] = objs[i];
	}

	if(cpm == 1)
	    addlib(STDLIB);
	else
	    addlib("cuzi");


	for(i = 0 ; i < lib_idx ; i++) {
	    flgs[flg_idx++] = libs[i];
	}

	flgs[flg_idx] = 0;


	doexec(linker, flgs);			// Building a program

#ifdef OBJTOHEX_AND_CREF
	if(reloc) {
	    flgs[0] = "-R";
	    flgs[1] = "-B100H";
	    flgs[2] = l_dot_obj;
	    flgs[3] = outfile;
	    flgs[4] = (char *)0;

	    doexec(objto, flgs);		// obj to hex

	    rm(l_dot_obj);
	} 
#endif
#if 0					  		// cp/m ---+
	else						//	   |
	    chmod(outfile, Def_Mode);			//	   |
#endif					  		// cp/m ---+

	if(c_as_idx == 1 && nfiles == 1)
	    rm(single);
    }
#ifdef OBJTOHEX_AND_CREF
    if(xref) {
	if(xrname) {
	    flgs[0] = xrname;
	    strcat(strcpy(tmpbuf, "-h"), outfile);

	    if( (cp = strrchr(tmpbuf, '.')) )
		strcpy(cp, ".crf");
	    else
		strcat(tmpbuf, ".crf");

	    flgs[1] = tmpbuf;
	    flgs[2] = crtmp;
	    flgs[3] = 0;

	    doexec(cref, flgs);			// Building cross-references

	    rm(crtmp);

	} else {
	    printf("Cross reference info left in %s: run CREF to produce listing\n", crtmp);
	}
    }
#endif
    fclose(stdout);
    fclose(stdin);
    exit(0);
}

//**********************************************************************
// 4	rm
//**********************************************************************
void rm(char * file) {

    if(verbose) {
	printf("del %s\n", file);
    }
    unlink(file);
}


//**********************************************************************
// 5	addobj
//**********************************************************************
void addobj(char * s) {
    char      *	cp;
    char      * name;
    uchar	len;
    static char oname;

/*
 *  Shaping the name of the output executable
 */
    if(oname == 0) {		// For the first time oname = 0,
	oname = 1;		// in other cases     oname = 1
	if(mark == 1)
	    name = outmame;	// If a outmame is assigned, use outmame,
	else			// otherwise use the s - name of first module
	    name = s;
	if((cp = strrchr(name, '.')))
	    len = cp - name;
	else
	    len = strlen(name);
	if(cpm == 1) {
	    cp = xalloc(len + strlen("-O.com") + 1);
	} else {
	    cp = xalloc(len + strlen("-O") + 1);
	}
	strncat(strcpy(cp, "-O"), name, len);

	if(cpm == 1) {
	    strcpy(cp + len + 2, ".com");
	}
	outfile = cp + 2;
    }

    cp = xalloc((int)strlen(s) + 1);
    strcpy(cp, s);
    objs[obj_idx++] = cp;
}

//**********************************************************************
// 6	addlib
//**********************************************************************
void addlib(char * s) {
    char * cp;

    strcpy(tmpbuf, libpath);
    strcat(strcat(tmpbuf, s), LIBSUFF);
    cp = xalloc((int)strlen(tmpbuf) + 1);
    strcpy(cp, tmpbuf);
    libs[lib_idx++] = cp;
}

//**********************************************************************
// 7	error
//**********************************************************************
void error(char * s) {

    fprintf(stderr, "%s", s);
    exit(1);
}

//**********************************************************************
// 8	xalloc
//**********************************************************************
char * xalloc(int s) {
    register char * cp;

    if(!(cp = (char *)malloc(s))) {
	error("Out of memory");
    }
    return cp;
}

//**********************************************************************
// 9	unx_exec
//**********************************************************************
int unx_exec(char * name, char ** vec) {
    int pid;
    int status;

    if( (pid = fork()) ) {
	if(pid == -1)
	    return -1;
	while(wait(&status) != pid)
	    continue;
	return status;
    }
    execvp(name, vec);	// Uses PATH to search for executable files
    perror(name);
    exit(1);
}

//**********************************************************************
// 10	Doexec
//**********************************************************************
int doexec(char * name, char ** vec) {
    char *new_argv[25];
//    int	status, pid;
    int w, i;
//    register void (*istat)(int), (*qstat)(int);

    new_argv[0] = name;
    w           = 1;
 
    if (verbose)
	printf("%s", name);

    while (w < 24) { 
	new_argv[w] = vec[w-1];

	if (verbose)
	    printf(" %s", new_argv[w]); 

	w++;

	if (vec[w - 1] == 0)
	    break;
    }

    if (verbose)
	printf("\n");

    new_argv[w] = 0;

    if( (i = unx_exec(name, new_argv)) ) {
	if(i == -1) {
	    perror(name);
	    error("Exec failed");
	}
	nerrs++;
	return 0;
    }
    return 1;
}

//**********************************************************************
// 11	Assemble
//**********************************************************************
void assemble(char * s) {
    char * vec[5];
    char   buf[80];
    char * cp;
    uchar  i;

    if(c_as_idx > 1)
	printf("%s\n", s);

    i = 0;
    if(optimize && !speed) {
	vec[i++] = "-J";
    }
    if(nolocal) {
	vec[i++] = "-X";
    }

    if( (cp = strrchr(s, ':')) )
	cp++;
    else
	cp = s;

    strcat(strcpy(buf, "-O"), cp);
 
    if(strrchr(buf, '.'))
	*strrchr(buf, '.') = 0;

    strcat(buf, ".obj");

    vec[i++] = buf;
    vec[i++] = s;
    vec[i]   = (char *)0;

    doexec(assem, vec);				// Assembling
}

//**********************************************************************
// 12	Compile
//**********************************************************************
void compile(char * s) {
    register char * cp;
    uchar           i, j;
    char          * vec[MAXLIST];
#ifdef OBJTOHEX_AND_CREF
    char            cbuf[50];
#endif

//printf("\ntest compile(%s) start\n", s);

    if(c_as_idx > 1)
	printf("%s\n", s);

    for(j = 0; j < iud_idx ; j++) {
	vec[j] = iuds[j];

//printf("test compile vec[%d] = %s\t iuds[%d] = %s\n", j, vec[j], j, iuds[j]);
    }

//printf("test compile End for\n");

    vec[j++] = s;

//printf("test compile vec[%d] = %s\n", j-1, vec[j-1]);

    vec[j++] = tmpf1;

//printf("test compile vec[%d] = %s\n", j-1, vec[j-1]);

    vec[j]   = (char *)0;
//printf("test compile vec[%d] = %s\n", j, vec[j]);

//printf("test compile START CPP\n\n");

    if(!doexec(cpp, vec))			// Preprocessor Pass
	return;

    if( (cp = strrchr(s, ':')) )
	s = cp+1;

    *strrchr(s, '.') = 0;

    i = 0;
    if(keepas && !optimize) {
	vec[i++] = "-S";
    }
#ifdef OBJTOHEX_AND_CREF
    if(xref) {
	vec[i++] = strcat(strcpy(cbuf, "-c"), crtmp);
    }
#endif
    vec[i++] = tmpf1;
    vec[i++] = tmpf2;
    vec[i++] = tmpf3;
    vec[i++] = (char *)0;

    if(!doexec(pass1, vec))			// First pass
	return;

    vec[0] = tmpf2;
    vec[1] = keepas && !optimize ? strcat(strcpy(tmpbuf, s), ".as") : tmpf1;
    vec[2] = (char *)0;

    if(!doexec(cgen, vec))			// Code generation
	return;

    if(keepas && !optimize)
	return;		//???

    cp = tmpf1;

    if(optimize) {
	i = 0;
	if(speed) {
	    vec[i++] = "-F";
        }

	vec[i++] = tmpf1;

	if(keepas)
	    vec[i++] = strcat(strcpy(tmpbuf, s), ".as");
	else
	    vec[i++] = tmpf2;

	vec[i] = (char *)0;

	if(!doexec(optim, vec))			// Optimization
	    return;

	if(keepas)
	    return;

	cp = tmpf2;
    }

    i = 0;
    if(nolocal) {
	vec[i++] = "-x";
    }
    if(optimize && !speed) {
	vec[i++] = "-j";
    }
    vec[i++] = "-n";
    vec[i++] = strcat(strcat(strcpy(tmpbuf, "-o"), s), ".obj");
    vec[i++] = cp;
    vec[i]   = (char *)0;

    doexec(assem, vec);				// Assembling
}

