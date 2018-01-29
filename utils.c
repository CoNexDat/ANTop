#include <netinet/ip6.h>
#include <stdio.h>


void copy_in6_addr(struct in6_addr *tgt, struct in6_addr *src)
{
  memcpy(tgt,src, sizeof(struct in6_addr));
}


  char *ip6_to_str(struct in6_addr addr)
{
    char *str;
    static char ip6_buf[4][40];
    static int count = 0;
    int which;

    which = (count%4);
    bzero(ip6_buf[which], 40);
    sprintf(ip6_buf[which], "%x:%x:%x:%x:%x:%x:%x:%x",
	       (int)ntohs(addr.s6_addr16[0]), (int)ntohs(addr.s6_addr16[1]),
	       (int)ntohs(addr.s6_addr16[2]), (int)ntohs(addr.s6_addr16[3]),
	       (int)ntohs(addr.s6_addr16[4]), (int)ntohs(addr.s6_addr16[5]),
	       (int)ntohs(addr.s6_addr16[6]), (int)ntohs(addr.s6_addr16[7]));
    //printf("count = %d, which = %d\n", count, which);
    str = ip6_buf[which];
    count++;
    return str;
}


  /* compute ICMPv6 checksum */
uint32_t cksum_sum(uint16_t *addr, int len, int ntoh) {
    uint16_t * w   = addr;
    uint16_t   ret = 0;
    uint32_t   sum = 0;
    while (len > 1) {
        ret = *w++;
        if (ntoh) ret = ntohs(ret);
        sum += ret;
        len -= 2;
    }
    if (len == 1) {
        *(unsigned char *) (&ret) = *(unsigned char *) w;  sum += ret;
    }
    return sum;
}


/*  RFC 1883 IPV6

    ipv6_pay_l: The Payload Length used in the pseudo-header is the length of
                the upper-layer packet, including the upper-layer header.  It
                will be less than the Payload Length in the IPv6 header (or in
                the Jumbo Payload option) if there are extension headers
                between the IPv6 header and the upper-layer header.

    icmp_pay_l: ICMPV6 Payload lenght

    NEXT HEADER:    The Next Header value in the pseudo-header identifies the
                    upper-layer protocol (e.g., 6 for TCP, or 17 for UDP).  It will
                    differ from the Next Header value in the IPv6 header if there
                    are extension headers between the IPv6 header and the upper-
                    layer header.

 */


uint16_t icmpv6_cksum(struct in6_addr* src_ip, struct in6_addr* dst_ip,char* ipv6_header, int icmp_pay_l, u_int16_t ipv6_pay_l) {
    uint32_t sum = 0;
    uint16_t val = +0;
    unsigned char *i_aux;
    uint16_t val_aux = 0;

 /* IPv6 pseudo header */
    sum += cksum_sum( src_ip->s6_addr16, 16, 1 );
    sum += cksum_sum( dst_ip->s6_addr16, 16, 1 );

    val = ipv6_pay_l;
    sum += cksum_sum( &val, 2, 0 );
    val = 0x003a;                            // NEXT HEADER 0x003a = 58 -> ICMPV6
    sum += cksum_sum( &val, 2, 0 );

 /* ICMPv6 packet */
//    val = 0x8000;                            // TYPE & CODE 0x8000 -> TYPE = 128 ; CODE = 0
    val=htons(*((uint16_t *)(ipv6_header + 72)));


//    val = 0x8700;
    sum += cksum_sum( &val, 2, 0 );
    i_aux = (unsigned char *)(ipv6_header + 76);    //ID
    val = *((uint16_t *)i_aux);
    sum += cksum_sum( &val, 2, 1 );
    i_aux = (unsigned char *)(ipv6_header + 78);    //SEQUENCE
    val = *((uint16_t *)i_aux);
    sum += cksum_sum( &val, 2, 1 );
    sum += cksum_sum( (uint16_t *)(ipv6_header + 80), icmp_pay_l, 1 );      //ICMP PAYLOAD
                                                                            //ACA SE TIENE QUE SUMAR 48 SI EL PAQUETE NO FUE MODIFICADO
                                                                            //SINO SE TIENE QUE SUMAR 40 DE IP + EXTENSION HEADER + 8 HEADER ICMP
 /* perform 16-bit one's complement of sum */
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    val = ~sum;
    return val;
}




uint16_t udp_cksum(struct in6_addr* src_ip, struct in6_addr* dst_ip,char* ipv6_header, int udp_pay_l, u_int16_t ipv6_pay_l) {
    uint32_t sum = 0;
    uint16_t val = +0;
    unsigned char *i_aux;
    uint16_t val_aux = 0;

 /* IPv6 pseudo header */
    sum += cksum_sum( src_ip->s6_addr16, 16, 1 );
    sum += cksum_sum( dst_ip->s6_addr16, 16, 1 );

    val = ipv6_pay_l;
    sum += cksum_sum( &val, 2, 0 );
    val = 0x0011;                            // NEXT HEADER 0x003a = 22 -> UDP
    sum += cksum_sum( &val, 2, 0 );

 /* UDP packet */
    // SOURCE PORT
    val=htons(*((uint16_t *)(ipv6_header + 72)));
    sum += cksum_sum( &val, 2, 0 );
    // DESTINATION PORT
    val=htons(*((uint16_t *)(ipv6_header + 74)));
    sum += cksum_sum( &val, 2, 0 );
    // LENGTH
    val=htons(*((uint16_t *)(ipv6_header + 76)));
    sum += cksum_sum( &val, 2, 0 );
    
    sum += cksum_sum( (uint16_t *)(ipv6_header + 80), udp_pay_l, 1 );      //ICMP PAYLOAD
                                                                            //ACA SE TIENE QUE SUMAR 48 SI EL PAQUETE NO FUE MODIFICADO
                                                                            //SINO SE TIENE QUE SUMAR 40 DE IP + EXTENSION HEADER + 8 HEADER ICMP
 /* perform 16-bit one's complement of sum */
    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    val = ~sum;
    return val;
}
