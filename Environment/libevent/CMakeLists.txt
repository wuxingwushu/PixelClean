project(libevent C)

include(CheckTypeSize)
include(CheckIncludeFile)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckStructHasMember)
include(CheckCSourceCompiles)

if(WIN32)
    set(EVENT__SIZEOF_INT 4)
    set(EVENT__SIZEOF_SHORT 2)
    set(EVENT__SIZEOF_LONG 4)
    set(EVENT__SIZEOF_LONG_LONG 8)
    set(EVENT__SIZEOF_VOID_P 8)
    set(EVENT__SIZEOF_SIZE_T 8)
    set(EVENT__SIZEOF_TIME_T 8)
    set(EVENT__SIZEOF_OFF_T 4)
    set(EVENT__size_t size_t)
    set(EVENT__SIZEOF_SSIZE_T 8)
    set(EVENT__ssize_t SSIZE_T)
    set(EVENT__SIZEOF_SOCKLEN_T 4)
    set(EVENT__socklen_t unsigned int)

    set(EVENT__HAVE_ARPA_INET_H 0)
    set(EVENT__HAVE_FCNTL_H 1)
    set(EVENT__HAVE_INTTYPES_H 1)
    set(EVENT__HAVE_MEMORY_H 1)
    set(EVENT__HAVE_NETDB_H 0)
    set(EVENT__HAVE_NETINET_IN_H 0)
    set(EVENT__HAVE_NETINET_IN6_H 0)
    set(EVENT__HAVE_NETINET_TCP_H 0)
    set(EVENT__HAVE_SIGNAL_H 1)
    set(EVENT__HAVE_STDARG_H 1)
    set(EVENT__HAVE_STDDEF_H 1)
    set(EVENT__HAVE_STDINT_H 1)
    set(EVENT__HAVE_STDLIB_H 1)
    set(EVENT__HAVE_STRING_H 1)
    set(EVENT__HAVE_ERRNO_H 1)
    set(EVENT__HAVE_UNISTD_H 0)
    set(EVENT__HAVE_TIME_H 1)
    set(EVENT__HAVE_SYS_STAT_H 1)
    set(EVENT__HAVE_SYS_TIME_H 0)
    set(EVENT__HAVE_SYS_TYPES_H 1)
    set(EVENT__HAVE_SYS_PARAM_H 0)
    set(EVENT__HAVE_SYS_QUEUE_H 0)
    set(EVENT__HAVE_SYS_UIO_H 0)
    set(EVENT__HAVE_SYS_SOCKET_H 0)
    set(EVENT__HAVE_SYS_IOCTL_H 0)
    set(EVENT__HAVE_SYS_SELECT_H 0)
    set(EVENT__HAVE_SYS_MMAN_H 0)
    set(EVENT__HAVE_SYS_RESOURCE_H 0)
    set(EVENT__HAVE_SYS_WAIT_H 0)
    set(EVENT__HAVE_SYS_UN_H 0)
    set(EVENT__HAVE_POLL_H 0)
    set(EVENT__HAVE_DLFCN_H 0)
    set(EVENT__HAVE_IFADDRS_H 0)
    set(EVENT__HAVE_WINSOCK2_H 1)
    set(EVENT__HAVE_WS2TCPIP_H 1)
    set(EVENT__HAVE_AFUNIX_H 0)
    set(EVENT__HAVE_IO_H 1)

    set(EVENT__HAVE_GETADDRINFO 1)
    set(EVENT__HAVE_GETNAMEINFO 1)
    set(EVENT__HAVE_GETPROTOBYNUMBER 0)
    set(EVENT__HAVE_GETSERVBYNAME 0)
    set(EVENT__HAVE_GETHOSTBYNAME 0)
    set(EVENT__HAVE_INET_NTOP 0)
    set(EVENT__HAVE_INET_PTON 0)
    set(EVENT__HAVE_GETTIMEOFDAY 0)
    set(EVENT__HAVE_SIGNAL 1)
    set(EVENT__HAVE_STRTOLL 1)
    set(EVENT__HAVE_STRLCPY 0)
    set(EVENT__HAVE_STRSEP 0)
    set(EVENT__HAVE_STRTOK_R 1)
    set(EVENT__HAVE_NANOSLEEP 0)
    set(EVENT__HAVE_PUTENV 1)
    set(EVENT__HAVE_UMASK 1)
    set(EVENT__HAVE_USLEEP 1)
    set(EVENT__HAVE_PIPE 0)
    set(EVENT__HAVE_PIPE2 0)
    set(EVENT__HAVE_FCNTL 0)
    set(EVENT__HAVE_MMAP 0)
    set(EVENT__HAVE_MMAP64 0)
    set(EVENT__HAVE_SELECT 0)
    set(EVENT__HAVE_POLL 0)
    set(EVENT__HAVE_SIGACTION 0)
    set(EVENT__HAVE_STRSIGNAL 0)
    set(EVENT__HAVE_SETENV 0)
    set(EVENT__HAVE_UNSETENV 0)
    set(EVENT__HAVE_SETRLIMIT 0)
    set(EVENT__HAVE_SENDFILE 0)
    set(EVENT__HAVE_ACCEPT4 0)
    set(EVENT__HAVE_GETIFADDRS 0)
    set(EVENT__HAVE_GETEGID 0)
    set(EVENT__HAVE_GETEUID 0)
    set(EVENT__HAVE_ISSETUGID 0)
    set(EVENT__HAVE_ARC4RANDOM 0)
    set(EVENT__HAVE_ARC4RANDOM_BUF 0)
    set(EVENT__HAVE_ARC4RANDOM_ADDRANDOM 0)
    set(EVENT__HAVE_GETRANDOM 0)
    set(EVENT__HAVE_CLOCK_GETTIME 0)
    set(EVENT__HAVE_TIMERFD_CREATE 0)
    set(EVENT__HAVE_KQUEUE 0)
    set(EVENT__HAVE_PORT_CREATE 0)
    set(EVENT__HAVE_EVENTFD 0)
    set(EVENT__HAVE_MACH_ABSOLUTE_TIME 0)
    set(EVENT__HAVE_SYSCTL 0)
    set(EVENT__HAVE__GMTIME64_S 1)
    set(EVENT__HAVE__GMTIME64 1)

    set(EVENT__HAVE_STRUCT_SOCKADDR_STORAGE 1)
    set(EVENT__HAVE_STRUCT_SOCKADDR_STORAGE_SS_FAMILY 1)
    set(EVENT__HAVE_STRUCT_SOCKADDR_IN6 1)
    set(EVENT__HAVE_STRUCT_IN6_ADDR 1)
    set(EVENT__HAVE_STRUCT_IN6_ADDR_S6_ADDR16 1)
    set(EVENT__HAVE_STRUCT_IN6_ADDR_S6_ADDR32 1)
    set(EVENT__HAVE_STRUCT_ADDRINFO 1)
    set(EVENT__HAVE_STRUCT_SOCKADDR_UN 0)
    set(EVENT__HAVE_STRUCT_LINGER 1)
    set(EVENT__HAVE_SA_FAMILY_T 1)
    set(EVENT__HAVE_FD_MASK 1)
    set(EVENT__HAVE_UINT8_T 1)
    set(EVENT__HAVE_UINT16_T 1)
    set(EVENT__HAVE_UINT32_T 1)
    set(EVENT__HAVE_UINT64_T 1)
    set(EVENT__HAVE_UINTPTR_T 1)
    set(EVENT__HAVE_STRUCT_SOCKADDR_IN6_SIN6_LEN 0)
    set(EVENT__HAVE_STRUCT_SOCKADDR_IN_SIN_LEN 0)
    set(EVENT__HAVE_STRUCT_SOCKADDR_STORAGE___SS_FAMILY 0)

    set(EVENT__HAVE_SYS_EPOLL_H 0)
    set(EVENT__HAVE_EPOLL 0)
    set(EVENT__HAVE_EPOLL_CREATE1 0)
    set(EVENT__HAVE_EPOLL_CTL 0)
    set(EVENT__HAVE_SYS_EVENT_H 0)
    set(EVENT__HAVE_WORKING_KQUEUE 0)
    set(EVENT__HAVE_SYS_SIGNALFD_H 0)
    set(EVENT__HAVE_SIGNALFD 0)
    set(EVENT__HAVE_PORT_H 0)
    set(EVENT__HAVE_EVENT_PORTS 0)
    set(EVENT__HAVE_SYS_DEVPOLL_H 0)
    set(EVENT__HAVE_DEVPOLL 0)
    set(EVENT__HAVE_WEPOLL 1)
    set(EVENT__HAVE_PTHREADS 0)
    set(EVENT__HAVE_PTHREAD_MUTEXATTR_SETPROTOCOL 0)
    set(EVENT__HAVE_TAILQFOREACH 0)
    set(EVENT__HAVE_TIMERADD 0)
    set(EVENT__HAVE_Timeradd 0)

    set(EVENT__HAVE_DECL_CTL_KERN 0)
    set(EVENT__HAVE_DECL_KERN_ARND 0)
    set(EVENT__HAVE_SETFD 0)
    set(EVENT__HAVE_GETHOSTBYNAME_R 0)
    set(EVENT__HAVE_GETHOSTBYNAME_R_3_ARG 0)
    set(EVENT__HAVE_GETHOSTBYNAME_R_5_ARG 0)
    set(EVENT__HAVE_GETHOSTBYNAME_R_6_ARG 0)
    set(EVENT__HAVE_OPENSSL 0)
    set(EVENT__HAVE_MBEDTLS 0)
    set(EVENT__HAVE_LIBZ 0)

    set(EVENT__inline inline)

    set(EVENT__HAVE___func__ 1)
    set(EVENT__HAVE___FUNCTION__ 1)

    set(EVENT__DNS_USE_FTIME_FOR_ID 1)
    set(EVENT__DNS_USE_CPU_CLOCK_FOR_ID 0)

else()
    check_type_size("int"          EVENT__SIZEOF_INT)
    check_type_size("short"        EVENT__SIZEOF_SHORT)
    check_type_size("long"         EVENT__SIZEOF_LONG)
    check_type_size("long long"    EVENT__SIZEOF_LONG_LONG)
    check_type_size("void *"       EVENT__SIZEOF_VOID_P)
    check_type_size("size_t"       EVENT__SIZEOF_SIZE_T)
    check_type_size("time_t"       EVENT__SIZEOF_TIME_T)
    check_type_size("off_t"        EVENT__SIZEOF_OFF_T)
    if(NOT EVENT__SIZEOF_SIZE_T)
        set(EVENT__SIZEOF_SIZE_T 4)
    endif()

    set(EVENT__size_t size_t)

    check_type_size("ssize_t" EVENT__SIZEOF_SSIZE_T_LOWER)
    check_type_size("SSIZE_T" EVENT__SIZEOF_SSIZE_T_UPPER)
    if(EVENT__SIZEOF_SSIZE_T_LOWER)
        set(EVENT__SIZEOF_SSIZE_T ${EVENT__SIZEOF_SSIZE_T_LOWER})
        set(EVENT__ssize_t "ssize_t")
    elseif(EVENT__SIZEOF_SSIZE_T_UPPER)
        set(EVENT__SIZEOF_SSIZE_T ${EVENT__SIZEOF_SSIZE_T_UPPER})
        set(EVENT__ssize_t "SSIZE_T")
    else()
        set(EVENT__SIZEOF_SSIZE_T 4)
        set(EVENT__ssize_t "int")
    endif()

    check_type_size("socklen_t" EVENT__SIZEOF_SOCKLEN_T)
    if(NOT EVENT__SIZEOF_SOCKLEN_T)
        set(EVENT__SIZEOF_SOCKLEN_T 4)
        set(EVENT__socklen_t "unsigned int")
    else()
        set(EVENT__socklen_t "socklen_t")
    endif()

    check_include_file("arpa/inet.h"     EVENT__HAVE_ARPA_INET_H)
    check_include_file("fcntl.h"         EVENT__HAVE_FCNTL_H)
    check_include_file("inttypes.h"      EVENT__HAVE_INTTYPES_H)
    check_include_file("memory.h"        EVENT__HAVE_MEMORY_H)
    check_include_file("netdb.h"         EVENT__HAVE_NETDB_H)
    check_include_file("netinet/in.h"    EVENT__HAVE_NETINET_IN_H)
    check_include_file("netinet/in6.h"   EVENT__HAVE_NETINET_IN6_H)
    check_include_file("netinet/tcp.h"   EVENT__HAVE_NETINET_TCP_H)
    check_include_file("signal.h"        EVENT__HAVE_SIGNAL_H)
    check_include_file("stdarg.h"        EVENT__HAVE_STDARG_H)
    check_include_file("stddef.h"        EVENT__HAVE_STDDEF_H)
    check_include_file("stdint.h"        EVENT__HAVE_STDINT_H)
    check_include_file("stdlib.h"        EVENT__HAVE_STDLIB_H)
    check_include_file("string.h"        EVENT__HAVE_STRING_H)
    check_include_file("errno.h"         EVENT__HAVE_ERRNO_H)
    check_include_file("unistd.h"        EVENT__HAVE_UNISTD_H)
    check_include_file("time.h"          EVENT__HAVE_TIME_H)
    check_include_file("sys/stat.h"      EVENT__HAVE_SYS_STAT_H)
    check_include_file("sys/time.h"      EVENT__HAVE_SYS_TIME_H)
    check_include_file("sys/types.h"     EVENT__HAVE_SYS_TYPES_H)
    check_include_file("sys/param.h"     EVENT__HAVE_SYS_PARAM_H)
    check_include_file("sys/queue.h"     EVENT__HAVE_SYS_QUEUE_H)
    check_include_file("sys/uio.h"       EVENT__HAVE_SYS_UIO_H)
    check_include_file("sys/socket.h"    EVENT__HAVE_SYS_SOCKET_H)
    check_include_file("sys/ioctl.h"     EVENT__HAVE_SYS_IOCTL_H)
    check_include_file("sys/select.h"    EVENT__HAVE_SYS_SELECT_H)
    check_include_file("sys/mman.h"      EVENT__HAVE_SYS_MMAN_H)
    check_include_file("sys/resource.h"  EVENT__HAVE_SYS_RESOURCE_H)
    check_include_file("sys/wait.h"      EVENT__HAVE_SYS_WAIT_H)
    check_include_file("sys/un.h"        EVENT__HAVE_SYS_UN_H)
    check_include_file("poll.h"          EVENT__HAVE_POLL_H)
    check_include_file("dlfcn.h"         EVENT__HAVE_DLFCN_H)
    check_include_file("ifaddrs.h"       EVENT__HAVE_IFADDRS_H)

    if(APPLE)
        check_include_file("mach/mach_time.h" EVENT__HAVE_MACH_MACH_TIME_H)
        check_include_file("mach/mach.h"      EVENT__HAVE_MACH_MACH_H)
    endif()

    check_function_exists(getaddrinfo           EVENT__HAVE_GETADDRINFO)
    check_function_exists(getnameinfo           EVENT__HAVE_GETNAMEINFO)
    check_function_exists(getprotobynumber      EVENT__HAVE_GETPROTOBYNUMBER)
    check_function_exists(getservbyname         EVENT__HAVE_GETSERVBYNAME)
    check_function_exists(gethostbyname         EVENT__HAVE_GETHOSTBYNAME)
    check_function_exists(inet_ntop             EVENT__HAVE_INET_NTOP)
    check_function_exists(inet_pton             EVENT__HAVE_INET_PTON)
    check_function_exists(gettimeofday          EVENT__HAVE_GETTIMEOFDAY)
    check_function_exists(signal                EVENT__HAVE_SIGNAL)
    check_function_exists(strtoll               EVENT__HAVE_STRTOLL)
    check_function_exists(strlcpy               EVENT__HAVE_STRLCPY)
    check_function_exists(strsep                EVENT__HAVE_STRSEP)
    check_function_exists(strtok_r              EVENT__HAVE_STRTOK_R)
    check_function_exists(nanosleep             EVENT__HAVE_NANOSLEEP)
    check_function_exists(putenv                EVENT__HAVE_PUTENV)
    check_function_exists(umask                 EVENT__HAVE_UMASK)
    check_function_exists(usleep                EVENT__HAVE_USLEEP)
    check_function_exists(pipe                  EVENT__HAVE_PIPE)
    check_function_exists(pipe2                 EVENT__HAVE_PIPE2)
    check_function_exists(fcntl                 EVENT__HAVE_FCNTL)
    check_function_exists(mmap                  EVENT__HAVE_MMAP)
    check_function_exists(mmap64                EVENT__HAVE_MMAP64)
    check_function_exists(select                EVENT__HAVE_SELECT)
    check_function_exists(poll                  EVENT__HAVE_POLL)
    check_function_exists(sigaction             EVENT__HAVE_SIGACTION)
    check_function_exists(strsignal             EVENT__HAVE_STRSIGNAL)
    check_function_exists(setenv                EVENT__HAVE_SETENV)
    check_function_exists(unsetenv              EVENT__HAVE_UNSETENV)
    check_function_exists(setrlimit             EVENT__HAVE_SETRLIMIT)
    check_function_exists(sendfile              EVENT__HAVE_SENDFILE)
    check_function_exists(accept4               EVENT__HAVE_ACCEPT4)
    check_function_exists(getifaddrs            EVENT__HAVE_GETIFADDRS)
    check_function_exists(getegid               EVENT__HAVE_GETEGID)
    check_function_exists(geteuid               EVENT__HAVE_GETEUID)
    check_function_exists(issetugid             EVENT__HAVE_ISSETUGID)
    check_function_exists(arc4random            EVENT__HAVE_ARC4RANDOM)
    check_function_exists(arc4random_buf        EVENT__HAVE_ARC4RANDOM_BUF)
    check_function_exists(arc4random_addrandom  EVENT__HAVE_ARC4RANDOM_ADDRANDOM)
    check_function_exists(getrandom             EVENT__HAVE_GETRANDOM)
    check_function_exists(clock_gettime         EVENT__HAVE_CLOCK_GETTIME)
    check_function_exists(timerfd_create        EVENT__HAVE_TIMERFD_CREATE)
    check_function_exists(kqueue                EVENT__HAVE_KQUEUE)
    check_function_exists(port_create           EVENT__HAVE_PORT_CREATE)
    check_function_exists(eventfd               EVENT__HAVE_EVENTFD)
    check_function_exists(mach_absolute_time    EVENT__HAVE_MACH_ABSOLUTE_TIME)
    check_function_exists(sysctl                EVENT__HAVE_SYSCTL)
    check_function_exists(_gmtime64_s           EVENT__HAVE__GMTIME64_S)
    check_function_exists(_gmtime64             EVENT__HAVE__GMTIME64)

    check_struct_has_member("struct sockaddr_in6" sin6_len "netinet/in.h" EVENT__HAVE_STRUCT_SOCKADDR_IN6_SIN6_LEN)
    check_struct_has_member("struct sockaddr_in"  sin_len  "netinet/in.h" EVENT__HAVE_STRUCT_SOCKADDR_IN_SIN_LEN)
    check_type_size("struct sockaddr_un"      EVENT__HAVE_STRUCT_SOCKADDR_UN)
    check_type_size("struct sockaddr_storage" EVENT__HAVE_STRUCT_SOCKADDR_STORAGE)
    check_struct_has_member("struct sockaddr_storage" ss_family  "sys/socket.h;netinet/in.h" EVENT__HAVE_STRUCT_SOCKADDR_STORAGE_SS_FAMILY)
    check_struct_has_member("struct sockaddr_storage" __ss_family "sys/socket.h;netinet/in.h" EVENT__HAVE_STRUCT_SOCKADDR_STORAGE___SS_FAMILY)
    check_type_size("struct addrinfo"         EVENT__HAVE_STRUCT_ADDRINFO)
    check_type_size("struct in6_addr"         EVENT__HAVE_STRUCT_IN6_ADDR)
    check_struct_has_member("struct in6_addr" s6_addr16 "netinet/in.h" EVENT__HAVE_STRUCT_IN6_ADDR_S6_ADDR16)
    check_struct_has_member("struct in6_addr" s6_addr32 "netinet/in.h" EVENT__HAVE_STRUCT_IN6_ADDR_S6_ADDR32)
    check_type_size("struct sockaddr_in6"     EVENT__HAVE_STRUCT_SOCKADDR_IN6)
    check_type_size("struct linger"           EVENT__HAVE_STRUCT_LINGER)
    check_type_size("sa_family_t"             EVENT__HAVE_SA_FAMILY_T)
    check_type_size("fd_mask"                 EVENT__HAVE_FD_MASK)
    check_type_size("uint8_t"                 EVENT__HAVE_UINT8_T)
    check_type_size("uint16_t"                EVENT__HAVE_UINT16_T)
    check_type_size("uint32_t"                EVENT__HAVE_UINT32_T)
    check_type_size("uint64_t"                EVENT__HAVE_UINT64_T)
    check_type_size("uintptr_t"               EVENT__HAVE_UINTPTR_T)

    check_c_source_compiles("inline void f(void){} int main(){return 0;}" EVENT_HAVE_INLINE_SUPPORT)
    check_c_source_compiles("__inline void f(void){} int main(){return 0;}" EVENT_HAVE___INLINE_SUPPORT)
    check_c_source_compiles("__inline__ void f(void){} int main(){return 0;}" EVENT_HAVE___INLINE___SUPPORT)
    if(EVENT_HAVE_INLINE_SUPPORT)
        set(EVENT__inline inline)
    elseif(EVENT_HAVE___INLINE_SUPPORT)
        set(EVENT__inline __inline)
    elseif(EVENT_HAVE___INLINE___SUPPORT)
        set(EVENT__inline __inline__)
    else()
        set(EVENT__inline)
    endif()

    check_symbol_exists("__func__"     "" EVENT__HAVE___func__)
    check_symbol_exists("__FUNCTION__" "" EVENT__HAVE___FUNCTION__)

    check_symbol_exists(TAILQ_FOREACH "sys/queue.h" EVENT__HAVE_TAILQFOREACH)

    check_symbol_exists(timerclear "sys/time.h" EVENT__HAVE_TIMERCLEAR)
    check_symbol_exists(timercmp   "sys/time.h" EVENT__HAVE_TIMERCMP)
    check_symbol_exists(timerisset "sys/time.h" EVENT__HAVE_TIMERISSET)
    check_symbol_exists(timeradd   "sys/time.h" EVENT__HAVE_TIMERADD)

    check_include_file("sys/epoll.h" EVENT__HAVE_SYS_EPOLL_H)
    if(EVENT__HAVE_SYS_EPOLL_H)
        set(EVENT__HAVE_EPOLL 1)
    endif()
    check_function_exists(epoll_create1 EVENT__HAVE_EPOLL_CREATE1)
    check_function_exists(epoll_ctl     EVENT__HAVE_EPOLL_CTL)

    check_include_file("sys/event.h" EVENT__HAVE_SYS_EVENT_H)
    if(EVENT__HAVE_KQUEUE)
        set(EVENT__HAVE_WORKING_KQUEUE 1)
    endif()

    check_include_file("sys/signalfd.h" EVENT__HAVE_SYS_SIGNALFD_H)
    if(EVENT__HAVE_SYS_SIGNALFD_H)
        set(EVENT__HAVE_SIGNALFD 1)
    endif()

    check_include_file("port.h" EVENT__HAVE_PORT_H)
    if(EVENT__HAVE_PORT_CREATE)
        set(EVENT__HAVE_EVENT_PORTS 1)
    endif()

    check_include_file("sys/devpoll.h" EVENT__HAVE_SYS_DEVPOLL_H)
    if(EVENT__HAVE_SYS_DEVPOLL_H)
        set(EVENT__HAVE_DEVPOLL 1)
    endif()

    check_symbol_exists(CTL_KERN  "sys/sysctl.h" EVENT__HAVE_DECL_CTL_KERN)
    check_symbol_exists(KERN_ARND "sys/sysctl.h" EVENT__HAVE_DECL_KERN_ARND)
    check_symbol_exists(F_SETFD "fcntl.h" EVENT__HAVE_SETFD)
    check_symbol_exists(gethostbyname_r "netdb.h" EVENT__HAVE_GETHOSTBYNAME_R)

    if(EVENT__HAVE_CLOCK_GETTIME)
        set(EVENT__DNS_USE_CPU_CLOCK_FOR_ID 1)
    endif()
endif()

set(EVENT_NUMERIC_VERSION 0x02020100)
set(EVENT_PACKAGE_VERSION "2.2.1")
set(EVENT_VERSION_MAJOR 2)
set(EVENT_VERSION_MINOR 2)
set(EVENT_VERSION_PATCH 1)
set(EVENT_VERSION "2.2.1-alpha-dev")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/event-config.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/include/event2/event-config.h
    NEWLINE_STYLE UNIX)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/evconfig-private.h.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/include/evconfig-private.h)

add_definitions(-DHAVE_CONFIG_H)

set(HDR_PRIVATE
    bufferevent-internal.h
    changelist-internal.h
    defer-internal.h
    epolltable-internal.h
    evbuffer-internal.h
    event-internal.h
    evmap-internal.h
    evrpc-internal.h
    evsignal-internal.h
    evthread-internal.h
    ht-internal.h
    http-internal.h
    iocp-internal.h
    ipv6-internal.h
    kqueue-internal.h
    log-internal.h
    minheap-internal.h
    mm-internal.h
    ratelim-internal.h
    strlcpy-internal.h
    time-internal.h
    util-internal.h
    openssl-compat.h
    ssl-compat.h
    mbedtls-compat.h
    sha1.h
    wepoll.h
    compat/sys/queue.h)

set(HDR_PUBLIC
    include/event.h
    include/evdns.h
    include/evhttp.h
    include/evrpc.h
    include/evutil.h
    include/event2/buffer.h
    include/event2/bufferevent.h
    include/event2/bufferevent_compat.h
    include/event2/bufferevent_struct.h
    include/event2/buffer_compat.h
    include/event2/dns.h
    include/event2/dns_compat.h
    include/event2/dns_struct.h
    include/event2/event.h
    include/event2/event_compat.h
    include/event2/event_struct.h
    include/event2/watch.h
    include/event2/http.h
    include/event2/http_compat.h
    include/event2/http_struct.h
    include/event2/keyvalq_struct.h
    include/event2/listener.h
    include/event2/rpc.h
    include/event2/rpc_compat.h
    include/event2/rpc_struct.h
    include/event2/tag.h
    include/event2/tag_compat.h
    include/event2/thread.h
    include/event2/util.h
    include/event2/ws.h
    include/event2/visibility.h
    include/event2/bufferevent_ssl.h
    ${CMAKE_CURRENT_BINARY_DIR}/include/event2/event-config.h)

set(SRC_CORE
    buffer.c
    bufferevent.c
    bufferevent_filter.c
    bufferevent_pair.c
    bufferevent_ratelim.c
    bufferevent_sock.c
    event.c
    evmap.c
    evthread.c
    evutil.c
    evutil_rand.c
    evutil_time.c
    watch.c
    listener.c
    log.c
    signal.c
    strlcpy.c)

set(SRC_EXTRA
    event_tagging.c
    http.c
    evdns.c
    ws.c
    sha1.c
    evrpc.c)

if(WIN32)
    list(APPEND SRC_CORE
        buffer_iocp.c
        bufferevent_async.c
        event_iocp.c
        win32select.c
        evthread_win32.c)

    if(EVENT__HAVE_WEPOLL)
        list(APPEND SRC_CORE epoll.c wepoll.c)
    endif()

    list(APPEND SRC_CORE
        WIN32-Code/getopt.c
        WIN32-Code/getopt_long.c)
    list(APPEND HDR_PRIVATE WIN32-Code/getopt.h WIN32-Code/tree.h)
elseif(APPLE)
    list(APPEND SRC_CORE select.c poll.c kqueue.c)
    if(EVENT__HAVE_SIGNALFD)
        list(APPEND SRC_CORE signalfd.c)
    endif()
    find_package(Threads REQUIRED)
    list(APPEND SRC_CORE evthread_pthread.c)
else()
    list(APPEND SRC_CORE select.c poll.c)
    if(EVENT__HAVE_EPOLL)
        list(APPEND SRC_CORE epoll.c)
    endif()
    if(EVENT__HAVE_KQUEUE)
        list(APPEND SRC_CORE kqueue.c)
    endif()
    if(EVENT__HAVE_EVENT_PORTS)
        list(APPEND SRC_CORE evport.c)
    endif()
    if(EVENT__HAVE_DEVPOLL)
        list(APPEND SRC_CORE devpoll.c)
    endif()
    if(EVENT__HAVE_SIGNALFD)
        list(APPEND SRC_CORE signalfd.c)
    endif()
    find_package(Threads REQUIRED)
    list(APPEND SRC_CORE evthread_pthread.c)
endif()

add_library(libevent ${SRC_CORE} ${SRC_EXTRA} ${HDR_PRIVATE} ${HDR_PUBLIC})
target_include_directories(libevent PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include)
target_include_directories(libevent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
    ${CMAKE_CURRENT_BINARY_DIR}/include)

if(WIN32)
    target_link_libraries(libevent PUBLIC ws2_32 shell32 advapi32 bcrypt iphlpapi)
    target_compile_definitions(libevent PRIVATE
        -D_CRT_SECURE_NO_WARNINGS
        -D_CRT_NONSTDC_NO_DEPRECATE
        -D_WIN32_WINNT=0x0600
        -DWINVER=0x0600
        -DLITTLE_ENDIAN)
    target_include_directories(libevent PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/WIN32-Code)
    target_compile_definitions(libevent PRIVATE -D_WINDOWS)
elseif(APPLE)
    target_link_libraries(libevent PUBLIC ${CMAKE_THREAD_LIBS_INIT})
else()
    target_link_libraries(libevent PUBLIC ${CMAKE_THREAD_LIBS_INIT})
endif()
