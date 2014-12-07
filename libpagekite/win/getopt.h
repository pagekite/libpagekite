#ifndef GETOPT_H
#define GETOPT_H

#ifdef _MSC_VER 
extern char *optarg;
#endif

int getopt(int nargc, char * const nargv[], const char *ostr) ;
 
#endif
