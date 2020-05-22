
#include <arpa/inet.h>  //funciones usadas para internet
#include <errno.h>
#include <fcntl.h>  //constantes tipo O_*
#include <net/if.h>
#include <net/if_arp.h>
#include <net/route.h>
#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>  //resolutores
#include <signal.h>  //señales
#include <stdarg.h>  //Manejar argumentos del tipo " ... "
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>  //socket
#include <sys/stat.h>    //información sobre atributos de archivos
#include <sys/time.h>    //funciones de tiempo
#include <sys/types.h>   //tipos de dato *_t para el sistema operativo
#include <sys/utsname.h>
#include <sys/wait.h>  //
#include <syslog.h>    //log del sistema
#include <time.h>
#include <unistd.h>  //llamadas al sistema
#include "cmdline.h"

#define MAX_BUFSIZE 1500

#define MODE_CONFIG_FILE 0     // Archivo de configuración + línea de comandos
#define MODE_ONLY_CMDLINE 1    // Solo línea de comandos
#define MODE_NET_PARAMETERS 2  // Obtener parámetros de red

// Tipos de mensaje
enum dhcp_msg_type {

    DHCPDISCOVER = 1,
    DHCPOFFER    = 2,
    DHCPREQUEST  = 3,
    DHCPDECLINE  = 4,
    DHCPACK      = 5,
    DHCPNAK      = 6,
    DHCPRELEASE  = 7,
    DHCPINFORM   = 8

};

enum dhcp_mode {

    GET_NETWORK_PARAMETERS = 1,
    CMDLINE                = 2,
    CONF_CMDLINE           = 3
};
enum dhcp_lease_state {
    S_FREE     = 0,
    S_LEASED   = 1,
    S_RESERVED = 2,
    S_PROHIBIT = 3,
    S_EXPIRED  = 4,
    S_WAIT     = 5,
    S_OWN      = 6
};

typedef struct dhcp_lease {
    enum dhcp_lease_state state;
    struct in_addr        ip;
    struct in_addr        broadcast;
    struct in_addr        netmask;
    struct in_addr        gateway;
    struct in_addr        dns1;
    struct in_addr        dns2;
    time_t                renewal;
    time_t                rebinding;
    time_t                lease_time;
    struct timespec       elapsed;
    struct timespec       start;
    struct timespec       now;
    u_char                mac[6];
    char                  hostname[255];
    // int                   max_msg_size;
    u_int32_t          xid;
    struct dhcp_lease *next;

} dhcp_lease;

typedef struct dhcp_options {

    struct in_addr     requested_address;        // 50
    char *             hostname;                 // 12
    bool               overload;                 // 52
    char *             tftp_sv_name;             // 66
    char *             boot_filename;            // 67
    enum dhcp_msg_type type;                     // 53
    struct in_addr     sv_identifier;            // 54
    char *             msg_err;                  // 56
    u_int16_t          max_message_size;         // 57
    char *             vendor_class_identifier;  // 60
    u_char *           client_identifier;        // 61
    //
    struct in_addr netmask;  // 1
    struct in_addr router;   // 3
    struct in_addr dns1;     // 6
    struct in_addr dns2;
    char *         dns_domain_name;  // 15
    struct in_addr broadcast;        // 28
    struct in_addr static_route;     // 33, ignorada
    time_t         lease_time;       // 51
    time_t         renewal;          // 58
    time_t         rebinding;        // 59

} dhcp_options;

typedef struct dhcp_msg {

    u_int8_t  op;
    u_int8_t  htype;
    u_int8_t  hlen;
    u_int8_t  hops;
    u_int32_t xid;
    u_int16_t secs;
    u_int16_t flags;

    struct in_addr ciaddr;
    struct in_addr yiaddr;
    struct in_addr siaddr;
    struct in_addr giaddr;

    u_char              chaddr[6];
    u_char              sname[64];  //  DHCP Option Overload option
    u_char              file[128];  // DHCP Option Overload option.
    u_char *            options_requested;
    size_t              len_requested;
    struct dhcp_options options;

} dhcp_msg;

typedef struct net_config {

    struct in_addr     ip;
    struct in_addr     dns1;
    struct in_addr     dns2;
    struct in_addr     broadcast;
    struct in_addr     netmask;
    struct in_addr     gateway;
    struct sockaddr_in resolver;
    struct in_addr     initial_ip;
    struct in_addr     last_ip;
    char               config_file[255];
    char               hostname[1024];
    u_int16_t          port;
    time_t             renewal;
    time_t             rebinding;
    time_t             lease;
    u_char             mac[6];

} net_config;

typedef struct dhcp_config {
    int total;
    int free;
    int active;
    int expired;
    int reserved;

} dhcp_config;

typedef struct dhcp_server {

    int descriptor;

    struct sockaddr_in addr;         // estructura escucha
    struct sockaddr_in remote_addr;  // estructura remota
    struct timeval     timeout;      // tiempo máximo de espera
    struct ifreq       ifr;          // estructura para obtener parámetros actuales de la red

    socklen_t size_addr;
    socklen_t remote_size;

    u_char             buf[MAX_BUFSIZE];  // tamaño min de msg a recibir
    char               interface_name[255];
    enum dhcp_mode     mode;
    struct dhcp_msg    msg;
    struct net_config  config;
    struct dhcp_lease *head;
    struct dhcp_config dhcp_config;
    ssize_t            size_msg;

} dhcp_server;

void dhcp_fatal ( const char *str, const char *error ) {
    printf ( str );
    printf ( ": %s\n", error );
    exit ( EXIT_FAILURE );
}

void dhcp_error ( const char *str ) {
    puts ( str );
    exit ( EXIT_FAILURE );
}

const char *get_state ( enum dhcp_lease_state state ) {
    switch ( state ) {
        case S_FREE:
            return "S_FREE";
        case S_LEASED:
            return "S_LEASED";
        case S_PROHIBIT:
            return "S_PROHIBIT";
        case S_EXPIRED:
            return "S_EXPIRED";
        case S_WAIT:
            return "S_WAIT";
        case S_OWN:
            return "S_OWN";
        case S_RESERVED:
            return "S_RESERVED";
    }
}
int probe_address ( dhcp_lease *lease ) {
}

void print_range ( dhcp_lease *head ) {
    dhcp_lease *tmp = head;
    // Imprimimos
    for ( tmp = head; tmp->next != NULL; tmp = tmp->next ) {

        char str[255];
        printf ( "ip: %s ", inet_ntop ( AF_INET, &tmp->ip, str, INET_ADDRSTRLEN ) );
        printf ( "state: %s ", get_state ( tmp->state ) );
        printf ( "netmask: %s ", inet_ntop ( AF_INET, &tmp->netmask, str, INET_ADDRSTRLEN ) );
        printf ( "dns: %s ", inet_ntop ( AF_INET, &tmp->dns1, str, INET_ADDRSTRLEN ) );
        printf ( "dns2: %s\n", inet_ntop ( AF_INET, &tmp->dns2, str, INET_ADDRSTRLEN ) );
        printf ( "broadcast: %s ", inet_ntop ( AF_INET, &tmp->broadcast, str, INET_ADDRSTRLEN ) );
        printf ( "gateway: %s ", inet_ntop ( AF_INET, &tmp->gateway, str, INET_ADDRSTRLEN ) );
        printf ( "hostname: %s ", tmp->hostname );
        printf ( "mac: %02x:%02x:%02x:%02x:%02x:%02x\n", tmp->mac[0], tmp->mac[1], tmp->mac[2], tmp->mac[3],
                 tmp->mac[4], tmp->mac[5] );
        printf ( "t1: %li ", tmp->renewal );
        printf ( "t2: %li ", tmp->rebinding );
        printf ( "t3: %li\n\n", tmp->lease_time );
    }
}

void print_lease_info ( dhcp_lease *tmp ) {

    // Imprimimos
    char str[255];
    printf ( "ip: %s \n", inet_ntop ( AF_INET, &tmp->ip, str, INET_ADDRSTRLEN ) );
    printf ( "state: %s \n", get_state ( tmp->state ) );
    printf ( "netmask: %s \n", inet_ntop ( AF_INET, &tmp->netmask, str, INET_ADDRSTRLEN ) );
    printf ( "dns: %s ", inet_ntop ( AF_INET, &tmp->dns1, str, INET_ADDRSTRLEN ) );
    printf ( "dns2: %s\n", inet_ntop ( AF_INET, &tmp->dns2, str, INET_ADDRSTRLEN ) );
    printf ( "broadcast: %s \n", inet_ntop ( AF_INET, &tmp->broadcast, str, INET_ADDRSTRLEN ) );
    printf ( "gateway: %s \n", inet_ntop ( AF_INET, &tmp->gateway, str, INET_ADDRSTRLEN ) );
    printf ( "hostname: %s \n", tmp->hostname );
    printf ( "mac: %02x:%02x:%02x:%02x:%02x:%02x\n", tmp->mac[0], tmp->mac[1], tmp->mac[2], tmp->mac[3],
            tmp->mac[4], tmp->mac[5] );
    printf ( "t1: %li \n", tmp->renewal );
    printf ( "t2: %li \n", tmp->rebinding );
    printf ( "t3: %li\n", tmp->lease_time );

}

struct dhcp_lease *get_free_lease ( dhcp_lease *head ) {
    for ( dhcp_lease *tmp = head; tmp->next != NULL; tmp = tmp->next )
        if ( tmp->state == S_FREE )
            return tmp;
    return NULL;
}

u_char search_xid ( u_int32_t xid, dhcp_lease *head ) {

    for ( dhcp_lease *tmp = head; tmp->next != NULL; tmp = tmp->next )
        if ( xid == tmp->xid )
            return 1;
    return 0;
}
u_char search_lease ( in_addr_t addr, dhcp_lease *head ) {
    for ( dhcp_lease *tmp = head; tmp->next != NULL; tmp = tmp->next )
        if ( tmp->ip.s_addr == addr
             && tmp->state == S_LEASED/*
             && *(tmp->mac + 0) == *(mac + 0)
             && *(tmp->mac + 1) == *(mac + 1)
             && *(tmp->mac + 2) == *(mac + 2)
             && *(tmp->mac + 3) == *(mac + 3)
             && *(tmp->mac + 4) == *(mac + 4)
             && *(tmp->mac + 5) == *(mac + 5) */) {
            return 1;
        }

    return 0;
}
struct dhcp_lease * confirm_lease ( dhcp_lease *head, in_addr_t addr ) {

    for ( dhcp_lease *tmp = head; tmp != NULL; tmp = tmp->next )
        if ( tmp->ip.s_addr == addr && tmp->state == S_LEASED ) {

            // ReIniciamos temporizador
            if ( clock_gettime ( CLOCK_MONOTONIC, &tmp->start ) == -1 )
                dhcp_error ( "Error from clock_gettime() in confirm_lease()" );
            return tmp;
        }
}
void build_msg ( struct dhcp_server *server, struct dhcp_lease *lease, enum dhcp_msg_type type ) {

    u_char *  p   = server->buf;
    dhcp_msg *msg = &server->msg;
    u_int32_t tmp = ntohl ( lease->ip.s_addr );

    memset ( p, 0, MAX_BUFSIZE );

    *p = DHCPOFFER;
    p++;
    *p = msg->htype;
    p++;
    *p = msg->hlen;
    p++;
    *p = msg->hops;
    p++;
    memcpy ( p, &msg->xid, 4 );
    p += 4;
    *p = 0;
    p++;  // usa 2 bytes; primero
    *p = msg->secs;
    p++;  // segundo
    *p = 0x80;
    p++;
    *p = 0x00;
    p++;
    p += 4;                             // ciaddr
    *( p + 0 ) = ( tmp >> 24 ) & 0xff;  // your ip
    *( p + 1 ) = ( tmp >> 16 ) & 0xff;
    *( p + 2 ) = ( tmp >> 8 ) & 0xff;
    *( p + 3 ) = ( tmp >> 0 ) & 0xff;
    p += 4;
    p += 4;                         // next ip address
    p += 4;                         // relay address
    memcpy ( p, &msg->chaddr, 6 );  // mac
    p = server->buf + 236;          // DHCP magic cookie

    *( p + 0 ) = 99;
    *( p + 1 ) = 130;
    *( p + 2 ) = 83;
    *( p + 3 ) = 99;
    p += 4;
    *p = 53;  // DHCP Message Type
    p++;
    *p = 1;
    p++;
    *p = type;
    p++;

    *( p + 0 ) = 54;  // ip server identifier
    *( p + 1 ) = 4;
    p += 2;

    tmp        = ntohl ( server->config.ip.s_addr );  // dhcp server address
    *( p + 0 ) = ( tmp >> 24 ) & 0xff;
    *( p + 1 ) = ( tmp >> 16 ) & 0xff;
    *( p + 2 ) = ( tmp >> 8 ) & 0xff;
    *( p + 3 ) = ( tmp >> 0 ) & 0xff;
    p += 4;

    *( p + 0 ) = 51;  // lease
    *( p + 1 ) = 4;
    *( p + 2 ) = ( server->config.lease >> 24 ) & 0xff;
    *( p + 3 ) = ( server->config.lease >> 16 ) & 0xff;
    *( p + 4 ) = ( server->config.lease >> 8 ) & 0xff;
    *( p + 5 ) = ( server->config.lease >> 0 ) & 0xff;

    p += 6;

    *( p + 0 ) = 1;  // subnet
    *( p + 1 ) = 4;
    tmp        = ntohl ( server->config.netmask.s_addr );
    *( p + 2 ) = ( tmp >> 24 ) & 0xff;
    *( p + 3 ) = ( tmp >> 16 ) & 0xff;
    *( p + 4 ) = ( tmp >> 8 ) & 0xff;
    *( p + 5 ) = ( tmp >> 0 ) & 0xff;

    p += 6;

    *( p + 0 ) = 3;  // router
    *( p + 1 ) = 4;

    tmp        = ntohl ( server->config.gateway.s_addr );
    *( p + 2 ) = ( tmp >> 24 ) & 0xff;
    *( p + 3 ) = ( tmp >> 16 ) & 0xff;
    *( p + 4 ) = ( tmp >> 8 ) & 0xff;
    *( p + 5 ) = ( tmp >> 0 ) & 0xff;

    p += 6;

    *( p + 0 ) = 6;  // dns
    *( p + 1 ) = 8;

    tmp        = ntohl ( server->config.dns1.s_addr );
    *( p + 2 ) = ( tmp >> 24 ) & 0xff;
    *( p + 3 ) = ( tmp >> 16 ) & 0xff;
    *( p + 4 ) = ( tmp >> 8 ) & 0xff;
    *( p + 5 ) = ( tmp >> 0 ) & 0xff;

    tmp        = ntohl ( server->config.dns2.s_addr );
    *( p + 6 ) = ( tmp >> 24 ) & 0xff;
    *( p + 7 ) = ( tmp >> 16 ) & 0xff;
    *( p + 8 ) = ( tmp >> 8 ) & 0xff;
    *( p + 9 ) = ( tmp >> 0 ) & 0xff;

    p += 10;
    *p = 0xff;  // fin
    p++;
    server->size_msg = p - server->buf;
}

void build_config_msg ( struct dhcp_server *server, struct dhcp_lease *lease, enum dhcp_msg_type type ) {

    u_char *  p   = server->buf;
    dhcp_msg *msg = &server->msg;
    u_int32_t tmp = ntohl ( lease->ip.s_addr );

    memset ( p, 0, MAX_BUFSIZE );

    *p = DHCPOFFER;
    p++;
    *p = msg->htype;
    p++;
    *p = msg->hlen;
    p++;
    *p = msg->hops;
    p++;
    memcpy ( p, &msg->xid, 4 );
    p += 4;
    *p = 0;
    p++;  // usa 2 bytes; primero
    *p = msg->secs;
    p++;  // segundo
    *p = 0x80;
    p++;
    *p = 0x00;
    p++;
    p += 4;                         // ciaddr
    p += 4;                         // your ip
    p += 4;                         // next ip address
    p += 4;                         // relay address
    memcpy ( p, &msg->chaddr, 6 );  // mac
    p = server->buf + 236;          // DHCP magic cookie

    *( p + 0 ) = 99;
    *( p + 1 ) = 130;
    *( p + 2 ) = 83;
    *( p + 3 ) = 99;
    p += 4;
    *p = 53;  // DHCP Message Type
    p++;
    *p = 1;
    p++;
    *p = type;
    p++;

    *( p + 0 ) = 54;  // ip server identifier
    *( p + 1 ) = 4;
    p += 2;

    tmp        = ntohl ( server->config.ip.s_addr );  // dhcp server address
    *( p + 0 ) = ( tmp >> 24 ) & 0xff;
    *( p + 1 ) = ( tmp >> 16 ) & 0xff;
    *( p + 2 ) = ( tmp >> 8 ) & 0xff;
    *( p + 3 ) = ( tmp >> 0 ) & 0xff;
    p += 4;

    *( p + 0 ) = 1;  // subnet
    *( p + 1 ) = 4;
    tmp        = ntohl ( server->config.netmask.s_addr );
    *( p + 2 ) = ( tmp >> 24 ) & 0xff;
    *( p + 3 ) = ( tmp >> 16 ) & 0xff;
    *( p + 4 ) = ( tmp >> 8 ) & 0xff;
    *( p + 5 ) = ( tmp >> 0 ) & 0xff;

    p += 6;

    *( p + 0 ) = 3;  // router
    *( p + 1 ) = 4;

    tmp        = ntohl ( server->config.gateway.s_addr );
    *( p + 2 ) = ( tmp >> 24 ) & 0xff;
    *( p + 3 ) = ( tmp >> 16 ) & 0xff;
    *( p + 4 ) = ( tmp >> 8 ) & 0xff;
    *( p + 5 ) = ( tmp >> 0 ) & 0xff;

    p += 6;

    *( p + 0 ) = 6;  // dns
    *( p + 1 ) = 8;

    tmp        = ntohl ( server->config.dns1.s_addr );
    *( p + 2 ) = ( tmp >> 24 ) & 0xff;
    *( p + 3 ) = ( tmp >> 16 ) & 0xff;
    *( p + 4 ) = ( tmp >> 8 ) & 0xff;
    *( p + 5 ) = ( tmp >> 0 ) & 0xff;

    tmp        = ntohl ( server->config.dns2.s_addr );
    *( p + 6 ) = ( tmp >> 24 ) & 0xff;
    *( p + 7 ) = ( tmp >> 16 ) & 0xff;
    *( p + 8 ) = ( tmp >> 8 ) & 0xff;
    *( p + 9 ) = ( tmp >> 0 ) & 0xff;

    p += 10;
    *p = 0xff;  // fin
    p++;
    server->size_msg = p - server->buf;
}
void check_status ( dhcp_lease *head ) {

    for ( dhcp_lease *tmp = head; tmp != NULL; tmp = tmp->next )
        if ( tmp->state != S_FREE ) {

            if ( clock_gettime ( CLOCK_MONOTONIC, &tmp->now ) == -1 )
                dhcp_error ( "Error from clock_gettime() in check_status()" );

            tmp->elapsed.tv_sec  = tmp->now.tv_sec - tmp->start.tv_sec;
            tmp->elapsed.tv_nsec = tmp->now.tv_nsec - tmp->start.tv_nsec;

            if ( tmp->elapsed.tv_sec >= tmp->lease_time )
                tmp->state = S_FREE;
        }
}

u_int8_t register_lease ( dhcp_lease *head, u_int32_t xid, u_char *mac ) {

    for ( dhcp_lease *tmp = head; tmp != NULL; tmp = tmp->next )
        if ( tmp->xid == xid ) {

            tmp->state = S_LEASED;
            memcpy(tmp->mac,mac, 6);
            // Iniciamos temporizador
            if ( clock_gettime ( CLOCK_MONOTONIC, &tmp->start ) == -1 )
                dhcp_error ( "Error from clock_gettime() in register_lease()" );

            return 1;
        }
    return 0;
}

void dec_dhcp_client_options ( u_char *v, dhcp_msg *msg ) {

    syslog ( LOG_NOTICE, "Option number: %d, len: %d", *v, *( v + 1 ) );
    size_t  size;
    u_char *tmp;

    switch ( *v ) {
        case 12:
            size                  = *( v + 1 );
            msg->options.hostname = malloc ( size + 1 );

            if ( !msg->options.hostname )
                dhcp_fatal (
                    "Error from malloc() in case 12, "
                    "dec_dhcp_client_options %s",
                    strerror ( errno ) );

            tmp = v + 2;

            memcpy ( &msg->options.hostname, tmp, size );
            *( &msg->options.hostname + size + 1 ) = '\0';
            syslog ( LOG_NOTICE, "Option: 12 size: %d hostname: %s", size, &msg->options.hostname );
            break;
        case 50:
            msg->options.requested_address.s_addr
                = htonl ( ( *( v + 2 ) << 24 ) | ( *( v + 3 ) << 16 ) | ( *( v + 4 ) << 8 ) | *( v + 5 ) );
            char str[INET_ADDRSTRLEN];

            syslog ( LOG_NOTICE, "Requested adress: %s",
                     inet_ntop ( AF_INET, &msg->options.requested_address, str, INET_ADDRSTRLEN ) );
            break;
        case 51:
            msg->options.lease_time = ( *( v + 2 ) << 24 ) | ( *( v + 3 ) << 16 ) | ( *( v + 4 ) << 8 ) | *( v + 5 );
            break;
        case 52:
            msg->options.overload = *( v + 2 );
            // falta leer campos file y sname
            break;
        case 53:
            msg->options.type = *( v + 2 );

            break;
        case 54:
            msg->options.sv_identifier.s_addr
                = htonl ( ( *( v + 2 ) << 24 ) | ( *( v + 3 ) << 16 ) | ( *( v + 4 ) << 8 ) | *( v + 5 ) );
            break;
        case 55:
            /*size                   = *( v + 1 );
            msg->len_requested     = size;
            msg->options_requested = malloc ( size );

            if ( !msg->options_requested )
                err_log_exit ( LOG_ERR,
                               "Error from malloc() in case 55, "
                               "dec_dhcp_client_options %s",
                               strerror ( errno ) );
            tmp = v + 2;

            for ( int i                          = 0; i < size; ++i )
                *( &msg->options_requested + i ) = *( tmp + i );

            tmp = NULL;*/
            // getchar();
            break;
        case 56:  // sv-cl
            tmp  = v + 1;
            size = *( tmp );
            tmp++;
            msg->options.msg_err = malloc ( size + 1 );

            if ( !msg->options.msg_err )
                dhcp_fatal (
                    "Error from malloc() in case 56, "
                    "dec_dhcp_client_options %s",
                    strerror ( errno ) );

            memcpy ( &msg->options.msg_err, tmp, size );
            *( &msg->options.msg_err + size + 1 ) = '\0';  // agregamos el fin de línea
            tmp                                   = NULL;
            break;
        case 57:
            msg->options.max_message_size = ( *( v + 2 ) << 8 ) | *( v + 3 );
            break;
        case 60:
            size                                 = *( v + 1 );
            tmp                                  = v + 2;
            msg->options.vendor_class_identifier = malloc ( size + 1 );
            memcpy ( msg->options.vendor_class_identifier, tmp, size );

            *( &msg->options.vendor_class_identifier + size + 1 ) = '\0';
            tmp                                                   = NULL;
            break;
        case 61:
            // Si viene, no alterar; en caso contrario, eliminar la opción
            // en la respuesta.
            size = *( v + 1 );
            tmp  = v + 2;

            msg->options.client_identifier = malloc ( size );
            if ( !msg->options.client_identifier )
                dhcp_fatal ( "Error from malloc() in case 61, dec_dhcp_msg %s", strerror ( errno ) );

            memcpy ( msg->options.client_identifier, tmp, size );

            break;
        case 66:

            size                      = *( v + 1 );
            tmp                       = v + 2;
            msg->options.tftp_sv_name = malloc ( size + 1 );
            memcpy ( &msg->options.tftp_sv_name, tmp, size );
            *( &msg->options.tftp_sv_name + size + 1 ) = '\0';

            break;
    }
}

void dec_dhcp_msg ( dhcp_msg *msg, u_char *buf ) {
    u_char *p = buf;
    u_char *tmp;

    // Message op code
    msg->op = *( p + 0 );
    // Hardware address type
    msg->htype = *( p + 1 );
    // Hardware address length
    msg->hlen = *( p + 2 );
    // Client sets to zero, optionally used by relay agents
    // when booting via a relay agent.
    msg->hops = *( p + 3 );
    p += 4;
    // Transaction ID
    msg->xid = htonl ( ( *( p + 0 ) << 24 ) | ( *( p + 1 ) << 16 ) | ( *( p + 2 ) << 8 ) | *( p + 3 ) );
    p += 4;
    // Filled in by client, seconds elapsed since client
    // began address acquisition or renewal process.
    msg->secs = htons ( *( p + 0 ) | *( p + 1 ) );
    p += 2;
    // Flags
    msg->flags = *( p + 0 ) | *( p + 1 );
    p += 2;
    // Client IP address; only filled in if client is in
    // BOUND, RENEW or REBINDING state and can respond
    // to ARP requests.
    msg->ciaddr.s_addr = htonl ( ( *( p + 0 ) << 24 ) | ( *( p + 1 ) << 16 ) | ( *( p + 2 ) << 8 ) | *( p + 3 ) );
    p += 4;
    //'your' (client) IP address.
    msg->yiaddr.s_addr = htonl ( ( *( p + 0 ) << 24 ) | ( *( p + 1 ) << 16 ) | ( *( p + 2 ) << 8 ) | *( p + 3 ) );
    p += 4;
    // IP address of next server to use in bootstrap;
    // returned in DHCPOFFER, DHCPACK by server.
    msg->siaddr.s_addr = htonl ( ( *( p + 0 ) << 24 ) | ( *( p + 1 ) << 16 ) | ( *( p + 2 ) << 8 ) | *( p + 3 ) );
    p += 4;
    // Relay agent IP address, used in booting via a
    // relay agent.
    msg->giaddr.s_addr = htonl ( ( *( p + 0 ) << 24 ) | ( *( p + 1 ) << 16 ) | ( *( p + 2 ) << 8 ) | *( p + 3 ) );
    p += 4;
    // Client hardware address.
    *( msg->chaddr + 0 ) = *( p + 0 );
    *( msg->chaddr + 1 ) = *( p + 1 );
    *( msg->chaddr + 2 ) = *( p + 2 );
    *( msg->chaddr + 3 ) = *( p + 3 );
    *( msg->chaddr + 4 ) = *( p + 4 );
    *( msg->chaddr + 5 ) = *( p + 5 );
    p += 6;
    /*  Procesamos solo las opciones restantes que son indispensables para el
     * funcionamiento de DHCP */

    for ( u_char *v = buf + 240; *v != 255; v += *( v + 1 ) + 2 )
        dec_dhcp_client_options ( v, msg );
}

void send_msg ( dhcp_server *server, in_addr_t ip ) {
    ssize_t sent;

    struct sockaddr_in addr;
    memset ( &addr, 0, sizeof ( struct in_addr ) );

    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = ip;
    addr.sin_port        = htons ( 68 );

    sent = sendto ( server->descriptor, server->buf, server->size_msg, 0, ( struct sockaddr * ) &addr,
                    sizeof ( struct sockaddr_in ) );

    if ( sent != server->size_msg )
        dhcp_fatal ( "Error in sendto from send_dhcpoffer: %s", strerror ( errno ) );
}

void change_lease ( dhcp_lease *head, in_addr_t addr ) {
    for ( dhcp_lease *tmp = head; tmp != NULL; tmp = tmp->next )
        if ( addr == tmp->ip.s_addr ) {
            tmp->state = S_LEASED;

            if ( clock_gettime ( CLOCK_MONOTONIC, &tmp->start ) == -1 )
                dhcp_error ( "Error from clock_gettime() in register_lease()" );
        }
}

void wait_request ( dhcp_server *server ) {
    ssize_t     received;
    dhcp_lease *tmp;

    // Limpiamos el buffer y esperamos msg válido
    memset ( server->buf, 0, MAX_BUFSIZE );
    memset ( &server->msg, 0, sizeof ( struct dhcp_msg ) );

    received = recvfrom ( server->descriptor, server->buf, MAX_BUFSIZE, 0, ( struct sockaddr * ) &server->remote_addr,
                          &server->remote_size );

    if ( received != -1 && received != EWOULDBLOCK && server->buf[0] == 1 && server->buf[236] == 99
         && server->buf[237] == 130 && server->buf[238] == 83 && server->buf[239] == 99 ) {

        puts ( "Mensaje DHCP recibido" );

        // Revisamos si ha llegado un msg DHCPDISCOVER o DHCPREQUEST
        dec_dhcp_msg ( &server->msg, server->buf );
        puts ( "Mensaje DHCP decodificado" );

        switch ( server->msg.options.type ) {
            //        DHCPDISCOVER
            //        El cliente está buscando servidores DHCP
            //        disponibles.
            //        DHCPOFFER
            //                El servidor responde al cliente DHCPDISCOVER.
            //        DHCPREQUEST
            //        El cliente transmite al servidor y solicita los
            //        parámetros ofrecidos desde un servidor en concreto,
            //        como se define en el paquete.
            //        DHCPDECLINE
            //                La comunicación cliente a servidor, indica que la
            //        dirección de red ya está en uso.
            //        DHCPACK
            //        La comunicación servidor a cliente con los
            //        parámetros de configuración, incluida la dirección
            //        de red comprometida.
            //        DHCPNAK
            //                La comunicación servidor a cliente, en la que se
            //        rechaza la petición del parámetro de configuración.
            //        DHCPRELEASE
            //        La comunicación cliente a servidor, en la que se
            //        renuncia a la dirección de red y se cancela la
            //        concesión restante.
            //        DHCPINFORM
            //        La comunicación cliente a servidor, donde se
            //        solicitan parámetros de configuración local que el
            //        cliente ya ha configurado externamente como una
            //        dirección.
            case DHCPDISCOVER:
                puts ( "Mensaje DHCPDiscover recibido" );

                if ( server->dhcp_config.free && server->msg.giaddr.s_addr == 0 ) {

                    // Si no encontramos una ip libre, avisamos y regresamos
                    tmp = get_free_lease ( server->head );
                    if ( !tmp ) {
                        puts ( "No hay IP libres por el momento" );
                        return;
                    }
                    // Guardamos el xid y enviamos
                    tmp->state = S_WAIT;
                    tmp->xid   = server->msg.xid;

                    build_msg ( server, tmp, DHCPOFFER );
                    send_msg ( server, INADDR_BROADCAST );
                    puts ( "DHCPOffer enviado" );
                }
                break;
            case DHCPREQUEST:
                puts ( "Mensaje DHCPRequest recibido" );

                // Para una respuesta a un DHCPOffer válido:
                // 1 - Client IP Address debe ser cero
                // 2 - El xid debe estar registrado
                // 3 - El identificador del servidor debe tener la IP correspondiente al del servidor DHCP

                printf("ciaddr: %d\n", server->msg.ciaddr.s_addr);
                printf ("search xid(): %d\n",search_xid ( server->msg.xid, server->head ));
                printf("server identifier: %d\n", server->msg.options.sv_identifier.s_addr == server->config.ip.s_addr);

                if ( server->msg.ciaddr.s_addr == 0 && search_xid ( server->msg.xid, server->head )
                     && server->msg.options.sv_identifier.s_addr == server->config.ip.s_addr ) {
                    puts ( "DHCPRequest válido" );

                    // Registramos el alquiler
                    if (register_lease ( server->head, server->msg.xid, server->msg.chaddr ))
                        puts("Registrado alquiler correctamente");
                    else
                        puts("Fallo en registrar alquiler");

                    // Buscamos dirección para enviar DHCPACK
                    for ( tmp = server->head; tmp != NULL; tmp = tmp->next )
                        if ( tmp->xid == server->msg.xid )
                            break;

                    // Construimos DHCPACK
                    build_msg ( server, tmp, DHCPACK );

                    // Enviamos DHCPACK
                    send_msg ( server, INADDR_BROADCAST );
                    puts("DHCPACK enviado");
                    return;
                }

                // Si es una petición para verificar o extender una concesión
                // Se debe añadir el mismo identificador de cliente
                // y todos los parametros de su DHCPDISCOVER
                printf("search_lease(): %d\n",search_lease ( server->msg.ciaddr.s_addr, server->head ));

                if ( server->msg.ciaddr.s_addr != 0 && search_lease ( server->msg.ciaddr.s_addr, server->head )
                     ) {
                    puts("Reconfirmamos concesión");// Confirmamos concesión

                    // Confirmamos concesión
                    tmp = confirm_lease ( server->head, server->msg.ciaddr.s_addr );

                    if (!tmp ) {
                        puts ( "Registro no encontrado" );
                        build_msg(server, tmp, DHCPNAK);
                        send_msg(server, INADDR_BROADCAST);
                        return;
                    }
                    print_lease_info(tmp);
                    // Construimos DHCPACK
                    build_msg ( server, tmp, DHCPACK );

                    // Enviamos DHCPACK
                    send_msg ( server, INADDR_BROADCAST );
                }


                break;

            case DHCPDECLINE:
                puts ( "DHCPDECLINE recibido" );
                // Buscamos dirección para enviar DHCPACK
                for ( tmp = server->head; tmp != NULL; tmp = tmp->next )
                    if ( tmp->ip.s_addr == server->msg.ciaddr.s_addr )
                        change_lease ( server->head, tmp->ip.s_addr );
            break;

            case DHCPRELEASE:
                puts ( "DHCPRELEASE recibido" );
                if ( server->msg.options.sv_identifier.s_addr == server->config.ip.s_addr )
                    for ( tmp = server->head; tmp != NULL; tmp = tmp->next )
                        if ( tmp->ip.s_addr == server->msg.ciaddr.s_addr
                             && *(tmp->mac + 0) == *(server->msg.chaddr + 0)
                             && *(tmp->mac + 1) == *(server->msg.chaddr + 1)
                             && *(tmp->mac + 2) == *(server->msg.chaddr + 2)
                             && *(tmp->mac + 3) == *(server->msg.chaddr + 3)
                             && *(tmp->mac + 4) == *(server->msg.chaddr + 4)
                             && *(tmp->mac + 5) == *(server->msg.chaddr + 5)
                             ) {
                            puts("Liberamos dirección");
                            tmp->state = S_FREE;
                            tmp->xid   = 0;
                            print_lease_info(tmp);
                            memset ( tmp->mac, 0, 6 );
                            memset ( &tmp->start, 0, sizeof ( struct timespec ) );

                        }

                break;

            case DHCPINFORM:
                puts ( "DHCPINFORM recibido" );
                // The server responds to a DHCPINFORM message by sending a DHCPACK
                // message directly to the address given in the 'ciaddr' field of the
                // DHCPINFORM message.  The server MUST NOT send a lease expiration time
                // to the client and SHOULD NOT fill in 'yiaddr'.  The server includes
                // other parameters in the DHCPACK message as defined in section 4.3.1.

                // Buscamos dirección para enviar DHCPACK
                for ( tmp = server->head; tmp != NULL; tmp = tmp->next )
                    if ( tmp->ip.s_addr == server->msg.ciaddr.s_addr )
                        build_config_msg ( server, tmp, DHCPACK );

                // Enviamos DHCPACK a la IP
                send_msg ( server, server->msg.ciaddr.s_addr );
                break;
            default:
                // No nos interesa otro tipo de msg DHCP, salimos
                return;
        }
    }
}

void get_lease_count ( dhcp_server *server ) {

    for ( dhcp_lease *tmp = server->head; tmp != NULL; tmp = tmp->next ) {

        switch ( tmp->state ) {
            case S_FREE:
                server->dhcp_config.free++;
                break;
            case S_LEASED:
                server->dhcp_config.active++;
                break;
            case S_PROHIBIT:
            case S_RESERVED:
                server->dhcp_config.reserved++;
                break;
            case S_EXPIRED:
                break;
            case S_WAIT:
                break;
            case S_OWN:
                break;
        }
    }
}

void up_service ( dhcp_server *server ) {
    dhcp_lease *tmp;

    tmp          = malloc ( sizeof ( struct dhcp_lease ) );
    server->head = tmp;

    // Generamos ip a administrar
    for ( in_addr_t i = server->config.initial_ip.s_addr; i != server->config.last_ip.s_addr; i += htonl ( 1 ) ) {
        server->dhcp_config.total++;
        tmp->state            = S_FREE;
        tmp->ip.s_addr        = i;
        tmp->broadcast.s_addr = server->config.broadcast.s_addr;
        tmp->netmask.s_addr   = server->config.netmask.s_addr;
        tmp->gateway.s_addr   = server->config.gateway.s_addr;
        tmp->dns1.s_addr      = server->config.dns1.s_addr;
        tmp->dns2.s_addr      = server->config.dns2.s_addr;
        tmp->rebinding        = server->config.rebinding;
        tmp->renewal          = server->config.renewal;
        tmp->lease_time       = server->config.lease;
        tmp->next             = malloc ( sizeof ( struct dhcp_lease ) );
        memset ( tmp->next, 0, sizeof ( struct dhcp_lease ) );

        tmp = tmp->next;
    }
}

void get_used_addresses ( dhcp_server *server ) {
}

void dhcp_init ( dhcp_server *server ) {

    const int flag = 1;

    // Iniciamos resolutores locales
    res_init ();

    server->descriptor = socket ( AF_INET, SOCK_DGRAM, 0 );

    if ( server->descriptor == -1 )
        dhcp_fatal ( "Error from socket() in dhcp_init()", strerror ( errno ) );

    // Asignamos datos
    server->addr.sin_family      = AF_INET;
    server->addr.sin_addr.s_addr = INADDR_ANY;
    server->addr.sin_port        = htons ( server->config.port );
    server->size_addr            = sizeof ( struct sockaddr_in );

    server->timeout.tv_sec = 1;

    // Opciones de socket y comprobaciones

    // Tiempo de espera máximo para recibir cada msg
    if ( setsockopt ( server->descriptor, SOL_SOCKET, SO_RCVTIMEO, ( char * ) &server->timeout,
                      sizeof ( server->timeout ) )
         < 0 )
        dhcp_fatal ( "Error from setsockopt(timeout) in dhcp_init()", strerror ( errno ) );

    // Para reutilizar la dirección
    if ( setsockopt ( server->descriptor, SOL_SOCKET, SO_REUSEADDR, ( char * ) &flag, sizeof ( flag ) ) < 0 )
        dhcp_fatal ( "Can't set SO_REUSEADDR option on dhcp socket", strerror ( errno ) );

    // Para enviar mensajes BROADCAST
    if ( setsockopt ( server->descriptor, SOL_SOCKET, SO_BROADCAST, ( char * ) &flag, sizeof ( flag ) ) < 0 )
        dhcp_fatal ( "Can't set SO_BROADCAST option on dhcp socket", strerror ( errno ) );

    // Bind a una única interfaz de red
    if ( setsockopt ( server->descriptor, SOL_SOCKET, SO_BINDTODEVICE, ( void * ) &server->ifr,
                      sizeof ( struct ifreq ) )
         < 0 )
        dhcp_fatal ( "Error from setsockopt() SO_BINDTODEVICE in dhcp_init()", strerror ( errno ) );

    if ( server->mode != GET_NETWORK_PARAMETERS ) {
        //Modo para levantar de cero la interfaz, lo único que falta es que aparezca la bandera IFF_RUNNING activada en la inferfaz (revisada con ifconfig)
        //No funciona aunque se agregue la bandera correctamente:
        // -------->>>>>> server->ifr.ifr_flags = tmp | ( IFF_UP | IFF_BROADCAST | IFF_RUNNING | IFF_MULTICAST );*
        /*
        strncpy ( server->ifr.ifr_name, server->interface_name, IFNAMSIZ );

        struct sockaddr_in *addr = ( struct sockaddr_in * ) &server->ifr.ifr_addr;

        // IP a usar
        addr->sin_family      = AF_INET;
        addr->sin_addr.s_addr = server->config.ip.s_addr;
        if ( ioctl ( server->descriptor, SIOCSIFADDR, &server->ifr ) == -1 )
            dhcp_fatal ( "ioctl SIOCSIFADDR from dhcp_init()", strerror ( errno ) );

        // Máscara de red (netmask)
        addr->sin_addr.s_addr = server->config.netmask.s_addr;
        if ( ioctl ( server->descriptor, SIOCSIFNETMASK, &server->ifr ) == -1 )
            dhcp_fatal ( "ioctl SIOCSIFNETMASK from dhcp_init()", strerror ( errno ) );

        // Difusión (broadcast)
        addr->sin_addr.s_addr = server->config.broadcast.s_addr;
        if ( ioctl ( server->descriptor, SIOCSIFBRDADDR, &server->ifr ) == -1 )
            dhcp_fatal ( "ioctl SIOCSIFBRDADDR from dhcp_init()", strerror ( errno ) );
        // Puerta de enlace (gateway)
        addr->sin_addr.s_addr = server->config.gateway.s_addr;
        if ( ioctl ( server->descriptor, SIOCSIFDSTADDR, &server->ifr ) == -1 )
            dhcp_fatal ( "ioctl SIOCSIFDSTADDR from dhcp_init()", strerror ( errno ) );

        // Obtenemos opciones activadas
        if ( ioctl ( server->descriptor, SIOCGIFFLAGS, &server->ifr ) == -1 )
            dhcp_fatal ( "ioctl SIOCGIFFLAGS from dhcp_init()", strerror ( errno ) );


        if ( ioctl ( server->descriptor, SIOCSIFFLAGS, &server->ifr ) == -1 )
            dhcp_fatal ( "ioctl SIOCSIFFLAGS from dhcp_init()", strerror ( errno ) );

        // Añadimos las que nos interesan
        server->ifr.ifr_flags = tmp | ( IFF_UP | IFF_BROADCAST | IFF_RUNNING | IFF_MULTICAST );*/

        /*if ( ioctl ( server->descriptor, SIOCGIFADDR, &server->ifr ) == -1 )
            dhcp_fatal ( "ioctl SIOCSIFADDR from dhcp_init()", strerror ( errno ) );
        server->config.ip.s_addr = addr->sin_addr.s_addr;

        if ( ioctl ( server->descriptor, SIOCGIFNETMASK, &server->ifr ) == -1 )
            dhcp_fatal ( "ioctl SIOCSIFNETMASK from dhcp_init()", strerror ( errno ) );
        server->config.netmask.s_addr = addr->sin_addr.s_addr;

        if ( ioctl ( server->descriptor, SIOCGIFBRDADDR, &server->ifr ) == -1 )
            dhcp_fatal ( "ioctl SIOCSIFBRDADDR from dhcp_init()", strerror ( errno ) );
        server->config.broadcast.s_addr = addr->sin_addr.s_addr;

        /*if ( ioctl ( server->descriptor, SIOCGIFDSTADDR, &server->ifr ) == -1 )
            dhcp_fatal ( "ioctl SIOCSIFDSTADDR from dhcp_init()", strerror ( errno ) );
        server->config.gateway.s_addr = addr->sin_addr.s_addr;*/

    }

    // Bind
    if ( bind ( server->descriptor, ( struct sockaddr * ) &server->addr, sizeof ( struct sockaddr_in ) ) == -1 )
        dhcp_fatal ( "Error from bind in dhcp_init()", strerror ( errno ) );

    // Obtenemos dirección MAC
    memset ( &server->ifr, 0, sizeof ( struct ifreq ) );
    snprintf ( server->ifr.ifr_name, sizeof ( server->ifr.ifr_name ), server->interface_name );

    if ( ioctl ( server->descriptor, SIOCGIFHWADDR, &server->ifr ) == -1 )
        dhcp_fatal ( "ioctl SIOCGIFHWADDR", strerror ( errno ) );

    // Comprobamos que sea una dirección de MAC de Ethernet
    if ( server->ifr.ifr_hwaddr.sa_family != ARPHRD_ETHER ) {
        printf ( "No es una interfaz Ethernet\n" );
        exit ( EXIT_FAILURE );
    }

    // La copiamos
    const unsigned char *mac = ( unsigned char * ) server->ifr.ifr_hwaddr.sa_data;
    memcpy ( server->config.mac, mac, 6 );
}

void print_options ( struct net_config *config ) {
    char str[255];

    printf ( "conf file: %s\n", config->config_file );
    printf ( "ip server: %s\n", inet_ntop ( AF_INET, &config->ip, str, INET_ADDRSTRLEN ) );
    printf ( "netmask: %s\n", inet_ntop ( AF_INET, &config->netmask, str, INET_ADDRSTRLEN ) );
    printf ( "dns1: %s\n", inet_ntop ( AF_INET, &config->dns1, str, INET_ADDRSTRLEN ) );
    printf ( "dns2: %s\n", inet_ntop ( AF_INET, &config->dns2, str, INET_ADDRSTRLEN ) );
    printf ( "broadcast: %s\n", inet_ntop ( AF_INET, &config->broadcast, str, INET_ADDRSTRLEN ) );
    printf ( "gateway: %s\n", inet_ntop ( AF_INET, &config->gateway, str, INET_ADDRSTRLEN ) );
    printf ( "hostname: %s\n", config->hostname );
    printf ( "mac: %02x:%02x:%02x:%02x:%02x:%02x\n", config->mac[0], config->mac[1], config->mac[2], config->mac[3],
             config->mac[4], config->mac[5] );
    printf ( "initial ip: %s\n", inet_ntop ( AF_INET, &config->initial_ip, str, INET_ADDRSTRLEN ) );
    printf ( "last ip: %s\n", inet_ntop ( AF_INET, &config->last_ip, str, INET_ADDRSTRLEN ) );
    printf ( "t1: %li\n", config->renewal );
    printf ( "t2: %li\n", config->rebinding );
    printf ( "t3: %li\n", config->lease );
}

void get_network_config ( dhcp_server *server ) {

    // Dirección IPv4 en uso
    memset ( &server->ifr, 0, sizeof ( struct ifreq ) );
    snprintf ( server->ifr.ifr_name, sizeof ( server->ifr.ifr_name ), server->interface_name );
    if ( ioctl ( server->descriptor, SIOCGIFADDR, &server->ifr ) == -1 )
        dhcp_fatal ( "ioctl SIOCGIFADDR", strerror ( errno ) );

    server->config.ip.s_addr = ( ( struct sockaddr_in * ) &server->ifr.ifr_addr )->sin_addr.s_addr;

    // Dirección de difusión (broadcast)
    if ( ioctl ( server->descriptor, SIOCGIFBRDADDR, &server->ifr ) == -1 )
        dhcp_fatal ( "ioctl SIOCGIFBRDADDR", strerror ( errno ) );

    server->config.broadcast.s_addr = ( ( struct sockaddr_in * ) &server->ifr.ifr_broadaddr )->sin_addr.s_addr;
    // Máscara de red
    if ( ioctl ( server->descriptor, SIOCGIFNETMASK, &server->ifr ) == -1 )
        dhcp_fatal ( "ioctl SIOCGIFNETMASK", strerror ( errno ) );

    server->config.netmask.s_addr = ( ( struct sockaddr_in * ) &server->ifr.ifr_netmask )->sin_addr.s_addr;

    // Puerta de enlace (gateway)
    if ( ioctl ( server->descriptor, SIOCGIFDSTADDR, &server->ifr ) == -1 )
        dhcp_fatal ( "ioctl SIOCGIFDSTADDR", strerror ( errno ) );

    server->config.gateway.s_addr = ( ( struct sockaddr_in * ) &server->ifr.ifr_netmask )->sin_addr.s_addr;

    // DNS principal
    memcpy ( &server->config.resolver, &_res.nsaddr_list[0], sizeof ( struct sockaddr_in ) );
    server->config.dns1.s_addr = server->config.resolver.sin_addr.s_addr;

    // DNS secundario
    // Si no hay un registro adicional para el dns2, usamos el de Google, 8.8.8.8
    if ( _res.nscount == 1 ) {
        memset ( &server->config.resolver, 0, sizeof ( struct sockaddr_in ) );
        memcpy ( &server->config.resolver, &_res.nsaddr_list[1], sizeof ( struct sockaddr_in ) );
        server->config.dns2.s_addr = server->config.resolver.sin_addr.s_addr;

    } else {
        server->config.dns2.s_addr = inet_addr ( "8.8.8.8" );
    }
}

void parse_config ( int argc, char *argv[], struct gengetopt_args_info *args_info, struct cmdline_parser_params *params,
                    struct dhcp_server *server ) {

    // Leemos las opciones de línea de comando por si hay algo que sobreescribir
    if ( cmdline_parser ( argc, argv, args_info ) != 0 )
        dhcp_fatal ( "Error from cmdline_parser() in parse_config", strerror ( errno ) );

    // Leemos el archivo de configuración

    if ( args_info->conf_file_given && args_info->net_config_given )
        dhcp_error ( "Elija solo un modo de operación (-n o -c o ninguno)" );

    if ( args_info->conf_file_given )
        server->mode = CONF_CMDLINE;

    if ( !args_info->conf_file_given && !args_info->net_config_given )
        server->mode = CMDLINE;

    if ( args_info->net_config_given )
        server->mode = GET_NETWORK_PARAMETERS;

    // Copiamos el nombre de la interfaz
    strcpy ( server->interface_name, args_info->interface_arg );

    // Verificamos qué puerto usar
    if ( args_info->port_given && args_info->port_arg >= 0 && args_info->port_arg <= 65536 )
        server->config.port = args_info->port_arg;
    else
        server->config.port = 67;

    switch ( server->mode ) {
        case CONF_CMDLINE:

            params->initialize = 0;
            params->override   = 0;
            if ( cmdline_parser_config_file ( args_info->conf_file_arg, args_info, params ) != 0 )
                dhcp_fatal ( "Error from cmdline_parser_config_file() in parse_config", strerror ( errno ) );
            if ( !args_info->ip_given )
                dhcp_error ( "Falta especificar IP propia del servidor" );
            break;

        case CMDLINE:
            if ( args_info->broadcast_given && args_info->dns_given && args_info->gateway_given
                 && args_info->netmask_given && args_info->t3_given && args_info->ip_given && args_info->range_given
                 && args_info->range_min == 2 ) {
            } else {

                if ( !args_info->ip_given )
                    puts ( "Falta especificar IP propia del servidor" );
                if ( !args_info->broadcast_given )
                    puts ( "Falta dirección de difusión (broadcast)" );
                if ( !args_info->dns_given )
                    puts ( "Faltan dns" );
                if ( !args_info->gateway_given )
                    puts ( "Falta dirección de la puerta de enlace (gateway)" );
                if ( !args_info->netmask_given )
                    puts ( "Falta dirección de máscara de red (netmask)" );
                if ( !args_info->range_given )
                    puts ( "Falta rango a administrar (range)" );
                if ( !args_info->t3_given )
                    puts ( "Falta tiempo máximo de concesión (t3)" );

                dhcp_error (
                    "Especifique broadcast, dns, gateway, range,netmask y lease_time si se usará solo los "
                    "parámetros "
                    "vía "
                    "línea de "
                    "comandos." );
            }
            break;

        case GET_NETWORK_PARAMETERS:
            break;
    }

    // Nombre de host
    struct utsname uts;

    if ( uname ( &uts ) == -1 )
        dhcp_fatal ( "uname", strerror ( errno ) );

    strcpy ( server->config.hostname, uts.nodename );

    // Tiempo a esperar por cada msg
    if ( args_info->timeout_given )
        server->timeout.tv_sec = args_info->timeout_arg;
    else
        server->timeout.tv_sec = 1;

    if ( server->mode == CONF_CMDLINE || server->mode == CMDLINE ) {

        // Dirección a usar
        if ( args_info->ip_given )
            server->config.ip.s_addr = inet_addr ( args_info->ip_arg );

        // Dirección de difusión
        if ( args_info->broadcast_given )
            server->config.broadcast.s_addr = inet_addr ( args_info->broadcast_arg );

        // Archivo de configuración
        if ( args_info->conf_file_given )
            strcpy ( server->config.config_file, args_info->conf_file_arg );

        // Puerta de enlace
        if ( args_info->gateway_given )
            server->config.gateway.s_addr = inet_addr ( args_info->gateway_arg );

        // Máscara de red
        if ( args_info->netmask_given )
            server->config.netmask.s_addr = inet_addr ( args_info->netmask_arg );

        // Rango a administrar
        if ( args_info->range_given ) {
            server->config.initial_ip.s_addr = inet_addr ( args_info->range_arg[0] );
            server->config.last_ip.s_addr    = inet_addr ( args_info->range_arg[1] );
        }

        // Tiempo de renovación
        if ( args_info->t1_given )
            server->config.renewal = args_info->t1_arg;

        // Tiempo de revinculación
        if ( args_info->t2_given )
            server->config.rebinding = args_info->t2_arg;

        // Tiempo de concesión
        if ( args_info->t3_given )
            server->config.lease = args_info->t3_arg;

        // Si solo nos dieron t3, calculamos t2 y 1
        // t2: 87.5%
        // t1: 50 %
        if ( args_info->t3_given && !args_info->t1_given && !args_info->t2_given ) {

            server->config.rebinding = ( ( time_t ) ( args_info->t3_arg * 87.5 ) ) / 100;
            server->config.renewal   = ( ( time_t ) args_info->t3_arg ) / 2;
        }

        // DNS
        if ( args_info->dns_given && args_info->dns_max ) {

            // DNS Principal
            server->config.dns1.s_addr = inet_addr ( args_info->dns_orig[0] );

            // DNS Secundario
            server->config.dns2.s_addr = inet_addr ( args_info->dns_orig[1] );

        } else if ( args_info->dns_given ) {
            // DNS Principal
            server->config.dns1.s_addr = inet_addr ( args_info->dns_orig[0] );
            // DNS Secundario
            server->config.dns2.s_addr = inet_addr ( "8.8.8.8" );
        }
    }
}

void terminate ( struct dhcp_lease *head ) {
    struct dhcp_lease *tmp = head;
    struct dhcp_lease *tmp2;
    while ( tmp->next != NULL ) {
        tmp2 = tmp->next;
        free ( tmp );
        tmp = tmp2;
    }
}

int main ( int argc, char *argv[] ) {

    struct gengetopt_args_info    args_info;
    struct cmdline_parser_params *params;
    struct dhcp_server            server;
    int                           signal = 0;

    memset ( &server, 0, sizeof ( struct dhcp_server ) );
    params = cmdline_parser_params_create ();

    // Obtenemos configuración a usar:
    // 1 - Desde archivo de configuración + línea de comandos
    // 2 - Solo línea de comandos
    // 3 - Obtener datos actuales de la red (Para varios servidores DHCP activos)
    parse_config ( argc, argv, &args_info, params, &server );

    // Inicializamos
    dhcp_init ( &server );

    if ( server.mode == GET_NETWORK_PARAMETERS ) {

        get_network_config ( &server );

        // Registrar direcciones en uso
        get_used_addresses ( &server );
    }

    print_options ( &server.config );

    // Liberamos memoria
    cmdline_parser_free ( &args_info );
    free ( params );

    // Levantar servicio
    up_service ( &server );

    // Obtenemos el número de IP reservadas, abandonadas y libres
    get_lease_count ( &server );
    // Proveer y administrar servicio
    for ( ;; ) {
        wait_request ( &server );
        check_status ( server.head );
    }
    // Liberamos
    /*
 */
    return 0;
}
