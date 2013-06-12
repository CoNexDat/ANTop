void copy_in6_addr(struct in6_addr *tgt, struct in6_addr *src);
char *ip6_to_str(struct in6_addr addr);
uint32_t cksum_sum(uint16_t *addr, int len, int ntoh);
uint16_t icmpv6_cksum(struct in6_addr* src_ip, struct in6_addr* dst_ip,char* ipv6_header, int icmp_pay_l, u_int16_t ipv6_pay_l);

//int attach_callback_func(int fd, callback_func_t func);

