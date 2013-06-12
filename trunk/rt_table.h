int data_attr_cb2(const struct nlattr *attr, void *data);
void attributes_show_ipv6(struct nlattr *tb[]);
int data_attr_cb(const struct nlattr *attr, void *data);
int data_cb(const struct nlmsghdr *nlh, void *data);
int handler_fn(struct nlmsghdr *hdr, void *arg);
static inline struct rtnl_handler * find_handler(struct rtnl_handle *rtnl_handle, u_int16_t type);
int call_handler(struct rtnl_handle *rtnl_handle, u_int16_t type, struct nlmsghdr *hdr);
struct rtnl_handle *rtnl_open(void);
int rtnl_parse_rta(struct rtattr *tb[], int max, struct rtattr *rta, int len);
int rtnl_handler_register(struct rtnl_handle *rtnl_handle, struct rtnl_handler *hdlr);
int rtnl_receive(struct rtnl_handle *rtnl_handle);
int rtnl_dump_type(struct rtnl_handle *rtnl_handle, unsigned int type);
int rt_table_dump();
int add_rt_route(struct in6_addr dst, struct in6_addr gtw, unsigned char mask, int distance);
int del_rt_route( struct in6_addr dst, struct in6_addr gw, unsigned char mask, int distance);
int del_rt_route2( struct rt_route_param *parameters);

