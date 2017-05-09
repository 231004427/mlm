#include "log.h"


static log4c_category_t *log_category = NULL;

int log_open(const char *category)
{
    if (log4c_init() == 1)
    {
        return -1;
    }
    log_category = log4c_category_get(category);
    return 0 ;
}

void log_message(int priority , const char *fmt , ...)
{
    va_list ap;
    
    assert(log_category != NULL);
    
    va_start(ap, fmt);
    log4c_category_vlog(log_category , priority , fmt , ap);
    va_end(ap);
}

void log_trace(const char *file, int line, const char *fun,size_t size,
               const char *fmt , ...)
{
    char new_fmt[2048];
    const char *head_fmt = "[file:%s, line:%d, function:%s]";
    va_list ap;
    int n;
    
    assert(log_category != NULL);
    n = sprintf(new_fmt, head_fmt , file , line , fun);
    pox_strlcat(new_fmt,fmt,size);
    //strlcat(new_fmt,fmt,size);
    
    va_start(ap , fmt);
    log4c_category_vlog(log_category , LOG4C_PRIORITY_TRACE, new_fmt , ap);
    va_end(ap);
}


int log_close()
{
    return (log4c_fini());
}
/*
 * '_cups_strlcat()' - Safely concatenate two strings.
 */

size_t pox_strlcat(char  *dst,const char *src,size_t size)
{
    size_t	srclen;			/* Length of source string */
    size_t	dstlen;			/* Length of destination string */
    
    dstlen = strlen(dst);
    size   -= dstlen + 1;
    
    if (!size)
        return (dstlen);		/* No room, return immediately... */

    srclen = strlen(src);
    
    
    if (srclen > size)
        srclen = size;
    
    memcpy(dst + dstlen, src, srclen);
    dst[dstlen + srclen] = '\0';
    
    return (dstlen + srclen);
}

size_t pox_strlcpy(char *dst,const char *src,size_t size)
{
    size_t	srclen;
    
    size --;
    
    srclen = strlen(src);
    
    
    if (srclen > size)
        srclen = size;
    
    memcpy(dst, src, srclen);
    dst[srclen] = '\0';
    
    return (srclen);
}
