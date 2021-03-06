#ifndef _ERROR_H_
#define _ERROR_H_
#include	<stdarg.h>		/* ANSI C header file */


void	 err_dump(const char *, ...);
void	 err_msg(const char *, ...);
void	 err_quit(const char *, ...);
void	 err_ret(const char *, ...);
void	 err_sys(const char *, ...);
#endif
