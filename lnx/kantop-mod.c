//#include <linux/config.h>

#ifdef CONFIG_MODVERSIONS
#define MODVERSIONS
//#include <linux/modversions.h>
#include <linux/version.h>
#else
#define USE_OLD_ROUTE_ME_HARDER
#endif
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/fs.h>
//#include <linux/wrapper.h>
#include <linux/string.h>
#include <linux/if_ether.h>
#include <linux/kmod.h>
#include <linux/ctype.h>
#include <linux/moduleparam.h>

#include <linux/skbuff.h>
//#include <linux/netfilter.h>
//PL: left the <linux/netfilter_ipv4.h> for ip_route_me_harder
//#include <linux/netfilter_ipv4.h>
#include </usr/include/linux/netfilter_ipv6.h>
//#include <linux/ipv6.h>

//#include <net/ip.h>
//#include <net/dst.h>
#include <net/route.h>
#include <linux/udp.h>
#include <linux/ipv6.h>
//#include <net/neighbour.h>
//#include </usr/include/netinet/in.h>

static struct nf_hook_ops nf_hook1, nf_hook2, nf_hook3, nf_hook4, nf_hook5;



#define MAX_INTERFACES 10

unsigned int ifindices[MAX_INTERFACES];
int nif = 0;

/* This function is taken from the kernel ip_queue.c source file. It
 * seem to have been moved to net/core/netfilter.c in later kernel
 * versions (verified for 2.4.18). There it is called
 * ip_route_me_harder(). Old version is kept here for compatibility.
 */


unsigned int nf_antop_hook(unsigned int hooknum,
			  struct sk_buff *skb,
			  const struct net_device *in,
			  const struct net_device *out,
			  int (*okfn) (struct sk_buff *))
{
 
    switch (hooknum){
        case NF_IP6_FORWARD:
            goto queue;
            break;

    case NF_IP6_PRE_ROUTING:
        goto queue;
	break;    

    case NF_IP6_LOCAL_OUT:
        goto queue;
	break;

    case NF_IP6_POST_ROUTING:
        goto queue;
        break;

    case NF_IP6_LOCAL_IN:
        goto queue;
        break;
    }

  accept:
        return NF_ACCEPT;
  queue:
       return NF_QUEUE;
}

/*
 * Called when the module is inserted in the kernel.
 */

char *ifname= "eth0";
//module_param(ifname,charp,0);

//char *ifname= { "eth0" };
//module_param(ifname,charp);

static int init(){

    struct net_device *dev = NULL;
//    int i;

    nf_hook1.list.next = NULL;
    nf_hook1.list.prev = NULL;
    nf_hook1.hook = nf_antop_hook;
    nf_hook1.pf = PF_INET6;    
    nf_hook1.hooknum = NF_IP6_PRE_ROUTING;
    nf_register_hook(&nf_hook1);

    nf_hook2.list.next = NULL;
    nf_hook2.list.prev = NULL;
    nf_hook2.hook = nf_antop_hook;
    nf_hook2.pf = PF_INET6;  
    nf_hook2.hooknum = NF_IP6_LOCAL_OUT;
    nf_register_hook(&nf_hook2);

    nf_hook3.list.next = NULL;
    nf_hook3.list.prev = NULL;
    nf_hook3.hook = nf_antop_hook;
    nf_hook3.pf = PF_INET6;   
    nf_hook3.hooknum = NF_IP6_POST_ROUTING;
    nf_register_hook(&nf_hook3);

    nf_hook4.list.next = NULL;
    nf_hook4.list.prev = NULL;
    nf_hook4.hook = nf_antop_hook;
    nf_hook4.pf = PF_INET6;
    nf_hook4.hooknum = NF_IP6_FORWARD;
    nf_register_hook(&nf_hook4);

    nf_hook5.list.next = NULL;
    nf_hook5.list.prev = NULL;
    nf_hook5.hook = nf_antop_hook;
    nf_hook5.pf = PF_INET6;
    nf_hook5.hooknum = NF_IP6_LOCAL_IN;
    nf_register_hook(&nf_hook5);

 //   for (i = 0; i < MAX_INTERFACES; i++) {
	
	dev = dev_get_by_name(&init_net, "eth0");
            if (!dev) {
		printk("No device %s available, ignoring!\n");
			
		}
//	}
//	ifindices[nif++] = dev->ifindex;
	dev_put(dev);
    
    printk("TERMINANDO LA CARGA DEL MODULO \n");
    return 0;
}

/*
 * Called when removing the module from memory...
 */

static void cleanup(){
    
    nf_unregister_hook(&nf_hook1);
    nf_unregister_hook(&nf_hook2);
    nf_unregister_hook(&nf_hook3);
    nf_unregister_hook(&nf_hook4);
    nf_unregister_hook(&nf_hook5);
}

module_init(init);
module_exit(cleanup);
