//
//  sd_port.h
//  stardict
//
//  Platform abstraction layer for cross-platform support
//


#ifndef sd_port_h
#define sd_port_h

#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
    #include <io.h>
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <winsock2.h>
    #pragma comment(lib, "ws2_32.lib")

    #define SD_STAT     struct _stat
    #define sd_stat    _stat
    #define sd_open    _open
    #define sd_close   _close
    #define sd_strcasecmp     _stricmp
    #define sd_strncasecmp    _strnicmp
    #define SD_ISREG(m)       (!((m) & _S_IFDIR))
    #define SD_ISDIR(m)       ((m) & _S_IFDIR)
    #define SD_O_RDONLY       _O_RDONLY
    #define SD_STAT_STRUCT    struct _stat

    // MSVC does not provide strndup
    static inline char *sd_strndup(const char *s, size_t n) {
        size_t len = strnlen(s, n);
        char *p = (char *)malloc(len + 1);
        if (p) {
            memcpy(p, s, len);
            p[len] = '\0';
        }
        return p;
    }
    #define strndup sd_strndup
#else
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <strings.h>
    #include <netinet/in.h>  // for ntohl

    #define SD_STAT     struct stat
    #define sd_stat    stat
    #define sd_open    open
    #define sd_close   close
    #define sd_strcasecmp     strcasecmp
    #define sd_strncasecmp    strncasecmp
    #define SD_ISREG(m)       S_ISREG(m)
    #define SD_ISDIR(m)       S_ISDIR(m)
    #define SD_O_RDONLY       O_RDONLY
    #define SD_STAT_STRUCT    struct stat
#endif

#endif /* sd_port_h */
