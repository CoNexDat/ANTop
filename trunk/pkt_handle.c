/*
#define INT_MIN 1
#define INT_MAX 100
#include <stdlib.h>
#include <stdio.h>
#include <linux/netlink.h>
#include <netinet/icmp6.h>
#include <netinet/ip6.h>
#include </usr/include/linux/netfilter_ipv6.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
*/


#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#define INT_MIN 1
#define INT_MAX 100
#define BUFLEN 256
#define BUFF_SIZE		(64 * 1024)
#define TEMP_SIZE		50
#define MAX_RTA 100
#include </usr/include/linux/netfilter_ipv6.h>
#include <netinet/icmp6.h>
#include <netinet/ip6.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/route.h>
#include <netinet/udp.h>
#include "rt_table.h"
#include "utils.h"
#include "labels.h"
#include "defs.h"
#include "timer_queue.h"
#include "antop_packet.h"
#include "heart_beat.h"


static int add_local_out;
static int add_post_routing;
static int add_pre_routing;



extern struct neighbor *neigh_table;
extern struct in6_addr prim_addr;
extern struct in6_addr sec_addr;
extern unsigned char prim_mask;
extern unsigned char sec_mask;
extern int ifindex;
extern struct route *route_t;
extern int socket_global;
extern struct recovered_addr *rcvd_addr;
struct src_nat *nat_table = NULL;
extern int sec_addr_flag;

/* This is additional info to the routing table
 */


struct rt_aux_info{
    struct in6_addr src, dst, gtw;
    struct rt_aux_info *next;

    // Timer for the visited bitmap

    struct timer vb_t;
    struct timer route_t;
    unsigned char visited_bitmap[MAX_NEIGHBORS];

    // The number of visited neighbors

    unsigned int count;

    // The route entry number in ther kernel table
    unsigned char route_id;
};

// Pointer to the first position of the auxiliar routing table info structure

static struct rt_aux_info *rt_info = NULL;
int check_addr_space(struct in6_addr *addr, struct in6_addr *addr2, unsigned char mask_addr);


/*struct rt_aux_info **/void rt_info_init(){    //This should return a pointer to the first element in the aux routing table

    // I should probably delete a routing table entry, like fe80/16

    // Create an entry in the list for each one in the routing table
    // Set the visited bitmap to zero for each entry

    struct route *rt_aux;
    int i,j;
    struct rt_aux_info *rt_inf, *previous;
    char print;

    print = 0;
    rt_aux = rt_table_dump(print);
    rt_inf = malloc(sizeof(struct rt_aux_info));
        if(rt_inf == NULL){
            fprintf(stderr, "rt_info_init: Could not locate space for rt_table_info \n");
            return;
        }

    rt_info=rt_inf;
    for(i=0; rt_aux!=NULL; i++, rt_aux=rt_aux->next, rt_inf=rt_inf->next){

        //Initialize the rt_aux_info structure
        //No timer here, since this are stable routes
        rt_inf->count=0;
        memcpy(&(rt_inf->dst), &(rt_aux->dst), sizeof(struct in6_addr));
        memset(&(rt_inf->src), 0, sizeof(struct in6_addr));
        rt_inf->route_id=i;
        for(j=0; j<MAX_NEIGHBORS; j++)
            rt_inf->visited_bitmap[j]=0;
        previous=rt_inf;
        rt_inf->next = malloc(sizeof(struct rt_aux_info));
        if(rt_inf->next == NULL){
            fprintf(stderr, "rt_info_init: Could not locate space for rt_table_info \n");
            return;
        }
    }

    //Last entry is empty...
    free(rt_inf);
    //End the aux rt info with NULL
    previous->next=NULL;


}


struct ipq_handle *h;

      struct nlsock {
	int sock;
	int seq;
	struct sockaddr_nl local;
        };

        struct sockaddr_nl peer = { AF_NETLINK, 0, 0, 0 };

        struct nlsock antopnl;
        struct nlsock rtnl;

int handle_packet(int fd){


         char buf[4096] __attribute__ ((aligned));
         int rv;
         rv = recv(fd, buf, sizeof(buf), 0);
/*
         fprintf(stderr,"\n -------------------------------------------------------------------\n");
         fprintf(stderr,"\n pkt received\n");
*/
         nfq_handle_packet(h, buf, rv);
         return 0;


}

void clear_vb(struct rt_route_param *rt_params){

    int aux;
    struct rt_aux_info *rt_aux;

    fprintf(stderr,"The visited bitmap for DST: %s will be deleted\n", ip6_to_str(rt_params->dst));

    for(rt_aux = rt_info;rt_aux != NULL && memcmp(&(rt_aux->dst), &(rt_params->dst), sizeof(struct in6_addr)); rt_aux = rt_aux->next, aux++);

    if(rt_aux == NULL){
        fprintf(stderr, "The requested visited bitmap does not exist \n");
        return;
    }else{
        fprintf(stderr,"The bitmap for DST %s\n", ip6_to_str(rt_aux->dst));
        for(aux = 0; aux < MAX_NEIGHBORS; aux++)
            rt_aux->visited_bitmap[aux] = 0;
    }

    return;
}

void del_rt_info(struct in6_addr *dst, struct in6_addr *gtw){
    struct rt_aux_info *curr;
    struct rt_aux_info *prev;

    fprintf(stderr, "An auxiliary entry will be deleted\n");

    for(curr = rt_info, prev = NULL; curr != NULL; prev = curr, curr = curr->next){
        if(!memcmp(dst, &(curr->dst), sizeof(struct in6_addr))){
            if(!memcmp(gtw, &(curr->gtw), sizeof(struct in6_addr))){
                //We found the entry to be deleted
                break;
            }
        }
    }

    if(curr == NULL){
        fprintf(stderr, "Could not find the entry to be deleted\n");
        return;
    }

    if(prev == NULL){
        //The entry to be deleted is the first one
        rt_info = curr->next;
        free(curr);
        fprintf(stderr,"The first entry in the auxiliary routing table was deleted\n");
        fprintf(stderr,"The auxiliary entry with DST: %s and GTW: %s was deleted \n", ip6_to_str(curr->dst), ip6_to_str(curr->gtw));
        return;
    }

    prev->next = curr->next;
    free(curr);
    fprintf(stderr,"The auxiliary entry with DST: %s and GTW: %s was deleted \n", ip6_to_str(curr->dst), ip6_to_str(curr->gtw));
    return;

}

/* Returns the ID of the route additional info entry */

struct rt_aux_info *add_rt_info(struct in6_addr *dst, struct in6_addr *src, int id){

    int i;
    struct rt_aux_info * rt_aux;
    struct rt_route_param *rt_params;
    struct rt_route_param *vb_params;

    vb_params = malloc(sizeof(struct rt_route_param));

    if(rt_info==NULL)
        fprintf(stderr, "The pointer to the auxiliary routing table in user space in NULL\n");



    // First we need to find the end of the list

    for(rt_aux = rt_info, i = 0; (rt_aux->next) != NULL; rt_aux = rt_aux->next, i++){
        fprintf(stderr,"RT AUX ENTRY %d. DST: %s, SRC: %s, ID: %d\n", i, ip6_to_str(rt_aux->dst), ip6_to_str(rt_aux->src), rt_aux->route_id);
        if(!memcmp(&(rt_aux->dst), &(dst), sizeof(struct in6_addr))){
            //There was already an entry so update it
            fprintf(stderr,"An entry for this route already existed in the auxiliary table. Update it\n");
            fprintf(stderr, "New SRC: %s; New DST: %s\n", ip6_to_str(rt_aux->src), ip6_to_str(rt_aux->dst));
            if(dst!=NULL)
            memcpy(&(rt_aux->dst), dst, sizeof(struct in6_addr));
            if(src!=NULL)
            memcpy(&(rt_aux->src), src, sizeof(struct in6_addr));
            fprintf(stderr, "New SRC: %s; New DST: %s\n", ip6_to_str(rt_aux->src), ip6_to_str(rt_aux->dst));
            memcpy(&(vb_params->dst), &(rt_aux->dst), sizeof(struct in6_addr));
            clear_vb(vb_params);
            break;
        }
    }

    if(rt_aux->next != NULL)
        return rt_aux;

    (rt_aux->next) = malloc(sizeof(struct rt_aux_info));

    if((rt_aux->next) == NULL) {
        fprintf(stderr, "Could not locate space for routing auxiliary information \n");
        return -1;
    }
    rt_aux = rt_aux->next;
    rt_aux->next=NULL;
    rt_aux->count=0;
    memcpy(&(rt_aux->dst), dst, sizeof(struct in6_addr));

    if(src != NULL)
        memcpy(&(rt_aux->src), src, sizeof(struct in6_addr));
    else
        memset(&(rt_aux->src), 0, sizeof(struct in6_addr));
    rt_aux->next = NULL;
    rt_aux->count = 0;
    rt_aux->route_id=id;
    for(i = 0; i < MAX_NEIGHBORS; i++)
        rt_aux->visited_bitmap[i] = 0;


    memcpy(&(vb_params->dst), &(rt_aux->dst), sizeof(struct in6_addr));
    rt_aux->vb_t.handler = &clear_vb;
    rt_aux->vb_t.data = vb_params;
    rt_aux->vb_t.used = 0;
    timer_add_msec(&(rt_aux->vb_t), T_VB);



    rt_params = malloc(sizeof(struct rt_route_param));

    memcpy(&(rt_params->dst), &(rt_aux->dst), sizeof(struct in6_addr));

    rt_params->mask = 128;


    rt_aux->route_t.handler = &del_rt_route2;
    rt_aux->route_t.data = rt_params;
    rt_aux->route_t.used = 0;
    timer_add_msec(&(rt_aux->route_t), T_ROUTE);

    return rt_aux;
}

int dist_w_mask(struct in6_addr *dst, struct neighbor neigh){
    int count = 0;
    int i, j, words, bits;
    uint16_t mask, aux1, aux2;


    // First 64 bits in the address for Link Local host.
    // First we need to know how many 16 bits words we need to compare.
    words = 4 + (neigh.prim_mask / 16);
    // Then the number of bits in the following word.
//    bits = 15 - (prim_mask % 16);
    bits = 15 - (neigh.prim_mask % 16);

    fprintf(stderr,"WORDS: %d\n",words);
    fprintf(stderr,"BITS: %d\n",bits);

    // Compare the whole words
    for(i=4; i<words; i++){
        for(j=0; j<16; j++){
            mask = htons(pow(2,j));
            aux1 = dst->s6_addr16[i] & mask;
            aux2 = neigh.prim_addr.s6_addr16[i] & mask;
            if(memcmp(&aux1, &aux2, sizeof(uint16_t))){
                count++;
            }
        }
    }
    // Now compare the bits remaining in the last word
    for(j=15; j>bits; j--){
            mask = htons(pow(2,j));
            aux1 = dst->s6_addr16[words] & mask;
            aux2 = neigh.prim_addr.s6_addr16[words] & mask;
            if(memcmp(&aux1, &aux2, sizeof(uint16_t))){
                count++;
            }
    }
    return count;
}



int dist(struct in6_addr *dst, struct neighbor neigh){
    int i, j;
    int count = 0;
    uint16_t mask, aux1, aux2;

    for(i=4; i<8; i++)
        for(j=0; j<16; j++){
            mask = htons(pow(2,j));
            aux1 = dst->s6_addr16[i] & mask;
            aux2 = neigh.prim_addr.s6_addr16[i] & mask;
            if(memcmp(&aux1, &aux2, sizeof(uint16_t)))
                count++;
        }
    return count;
}


void add_src_nat(struct in6_addr *dst, struct in6_addr *src){

    struct src_nat *src_nat_aux;
    if(nat_table == NULL){
        nat_table = malloc(sizeof(struct src_nat));
        src_nat_aux = nat_table;
    }else
        for(src_nat_aux = nat_table;src_nat_aux->next != NULL; src_nat_aux = src_nat_aux->next){
            if(!memcmp(&(src_nat_aux->dst), dst, sizeof(struct in6_addr)))
                break;
        }
    if(src_nat_aux->next == NULL){
        src_nat_aux->next = malloc(sizeof(struct src_nat));
        src_nat_aux = src_nat_aux->next;
        memcpy(&(src_nat_aux->dst), dst, sizeof(struct in6_addr));
        memcpy(&(src_nat_aux->src), src, sizeof(struct in6_addr));
        src_nat_aux->next = NULL;
    }
    return;

}

void send_to_next_neighbor(unsigned char *pkt /* ,reverseEntry */ ){

    struct route *rt;
    int aux, nh_idx, best_dist, best_mask, d, rt_id;
    struct route *rt_aux;
    int min_dist, best, i;
    struct rt_aux_info *rt_info_aux;
    struct neighbor *neigh_t_aux;
    unsigned char all_visited;
    struct in6_addr nh, aux_addr;
    unsigned char aux_c;
    char print;

    rt_info_aux = NULL;
    print =0;
    rt = rt_table_dump(print);
    best = -1;
    aux=0;
    rt_id=-1;
    struct rt_route_param params;
    // If there are no neighbors, use the default gateway


    fprintf(stderr,"El paquete con destino %s entro a send_to_next_neighbor\n",ip6_to_str(*((struct in6_addr *)(pkt + DST_ADDR_OFFSET))));

    if(neigh_table == NULL){
        fprintf(stderr,"There are no neighbors to send the packet\n");
        return;
    }

    //Look for an entry in the rt with this dst
    for(;rt != NULL; rt = rt->next){
            if(!memcmp(&(rt->dst), pkt + DST_ADDR_OFFSET, sizeof(struct in6_addr)) && memcmp(&(rt->gtw), pkt + DST_ADDR_OFFSET, sizeof(struct in6_addr))){
                fprintf(stderr,"There is an entry for the route DST: %s\n",ip6_to_str(rt->dst));
                fprintf(stderr,"GTW: \n", ip6_to_str(rt->gtw));
                fprintf(stderr,"TABLE: %d\n",rt->table);

                aux = 1;
                break;
            }
    }
    if(aux == 0){
        // No entries found, we should add a new one.
//        memset(&nh, 0, sizeof(struct in6_addr));
        memcpy(&(aux_addr), pkt + DST_ADDR_OFFSET, sizeof(struct in6_addr));
        // Add a rt entry to this dst, without nh. It will be completed lately
        // Use the primary addr as temporary nh to identify this incomplete entry
        add_rt_route(aux_addr, prim_addr, 128, MAX_TTL);


        fprintf(stderr,"add rt route next 1, DST: %s; GW: %s\n",ip6_to_str(aux_addr), ip6_to_str(prim_addr));
        //Get the entry id for this new route
        print=0;
        rt = rt_table_dump(print);
        for(i=0; rt!=NULL; rt=rt->next, i++){
            if(!memcmp(&(rt->dst), pkt + DST_ADDR_OFFSET, sizeof(struct in6_addr))){
                rt_id=i;
                fprintf(stderr,"The entry was added in the %d position of the rt\n", rt_id);
            }
        }
        if(rt_id == -1){
            fprintf(stderr,"route: Could not find the requested entry in the aux rt info table DST: %s\n", ip6_to_str(*((struct in6_addr *)(pkt + DST_ADDR_OFFSET))));
            return;
        }
    }else{
        // We should find the entry with the shortest distance.
        min_dist = MAX_TTL +1;
        print =0;
        for(rt = rt_table_dump(print), aux = 0; rt != NULL; rt = rt->next, aux++){
            if(rt->distance < min_dist && !memcmp(&(rt->dst), pkt + DST_ADDR_OFFSET, sizeof(struct in6_addr))){
                best = aux;
                min_dist = rt->distance;
                rt_aux = rt;
            }
        }
    }

    // Reverse Entry is checked here

    // If the visited bitmap is new, we should add a timer to clear it. This could happen
    // either when the route entry is new or when we didn't visit any neighbors yet.

    if(best > -1){
        // This means that we already had an entry in routing table
        // so we need to find the auxiliary entry for this route.
        for(rt_info_aux = rt_info, aux = 0; (rt_info_aux != NULL) && ((rt_info_aux->route_id) != best) ; rt_info_aux = rt_info_aux->next, aux++);
        rt_id=best;
    }
    //We create the new auxiliary route info entry, adding a timer to clear the visited bitmap
    if(rt_info_aux == NULL){
        fprintf(stderr,"Entry will be created for the aux table. DST: %s\n",ip6_to_str(*(struct in6_addr *)(pkt + DST_ADDR_OFFSET)));
        rt_info_aux = add_rt_info(pkt + DST_ADDR_OFFSET, NULL, rt_id);

    }

    // Mark as visited the neighbor where the packet came from.

    for(neigh_t_aux = neigh_table, i=0; neigh_t_aux != NULL && memcmp(&(neigh_t_aux->prim_addr), pkt+FROM_ADDR_OFFSET, sizeof(struct in6_addr));
            neigh_t_aux = neigh_t_aux->next, i++);

    if(neigh_t_aux == NULL){
        fprintf(stderr, "route: could not find the neighbor \n");
    }
    else{
        rt_info_aux->visited_bitmap[i] = 1;
        rt_info_aux->count = rt_info_aux->count +1;
    }



    // Check if all the neighbors are visited

    all_visited = 1;
    // We must check if the neighbor is available and then we check if it is visited.

    for(neigh_t_aux = neigh_table, i=0; (i < MAX_NEIGHBORS) && (neigh_t_aux != NULL); i++, neigh_t_aux = neigh_t_aux->next)
        if(neigh_t_aux->available == 1 && rt_info_aux->visited_bitmap[i] == 0)
            all_visited = 0;

    // If all neighbors were visited, we should return the packet where it came from

    if(all_visited == 1){
        fprintf(stderr, "All neighbors were visited, so we send the packet back to the source\n");
        print=0;
        rt = rt_table_dump(print);

        // We look for the reverse Entry.
        for(i = 0;rt != NULL; rt = rt->next, i++){
            //Check that MASK != 64. Since this could be the default gateway for fe80::0
            if(!memcmp(&(rt->dst), pkt + SRC_ADDR_OFFSET, sizeof(struct in6_addr)) && rt->dst_mask !=64){
                fprintf(stderr,"There is a route for DST: %s; GW: %s; MASK: %d\n", ip6_to_str(rt->dst), ip6_to_str(rt->gtw), rt->dst_mask);

                    // It has the same destination, now check the src addr in the aux info table (Reverse Entry)
                    // The source address is not actually stored in the rt.
                for(rt_info_aux = rt_info; (rt_info_aux != NULL) && (rt_info_aux->route_id != i); rt_info_aux = rt_info_aux->next)

                if(rt_info_aux == NULL)
                    fprintf(stderr, "route: Could not locate the auxilliary entry\n");
                else{
                    // This aux entry is related to the one in the routing table
                    // Check if the source is the same
                    if(!memcmp(&(rt_info_aux->src), pkt + DST_ADDR_OFFSET, sizeof(struct in6_addr))){
                        // We return the packet where it came from
                        set_returned(pkt);
                        *(pkt + TTL_OFFSET) = *(pkt + TTL_OFFSET) +1;
                        memcpy(&nh, &(rt->gtw), sizeof(struct in6_addr));
                        break;
                    }
                    else{
                        fprintf(stderr,"The reverse Entry has not the same src. DST: %s; SRC: %s\n", ip6_to_str(rt_info_aux->src),ip6_to_str(rt_info_aux->dst));

                    }
                }
            }
        }

        // If there is no reverse entry with this src and dst, then set nh as from addr

        if(rt == NULL){
            fprintf(stderr,"There is no Reverse Entry, so send to packet back to the from address: %s\n", ip6_to_str(*(struct in6_addr *)(pkt + FROM_ADDR_OFFSET)));
            memcpy(&nh, pkt + FROM_ADDR_OFFSET, sizeof(struct in6_addr));
        }

        // Mark that there is no route for this destination by changing the nh in the rt
        print=0;
        rt = rt_table_dump(print);
        // First we find the entries with this dst

        for(i = 0; rt != NULL; rt = rt->next, i++){
            if(!memcmp(&(rt->dst), pkt + DST_ADDR_OFFSET, sizeof(struct in6_addr))){
                // We create an entry with the new gtw
//                add_rt_route(rt->dst, nh, rt->dst_mask, rt->distance);

                // HERE DST->MASK IS WRONG !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1
                // CORRECT THIS, IT SHOULD START WITH 64 BY NOW, BUT THIS SHOULD BE DIFERENT WHEN WE USE MORE BITS FOR THE HOSTS
                // USE 128 BY NOW

                add_rt_route(rt->dst, nh, 128, rt->distance);
                


                fprintf(stderr,"add rt route next 2\n");
                // We delete the old one
                del_rt_route(rt->dst, rt->gtw, rt->dst_mask, rt->distance);
                // I asume that the new route and the old one are on the same place in the routing table. Then
                // the ROUTE_ID is the same. Check This.
            }
        }
        return; // Check wheter the packet goes through the routing table again
    }
    // We are here, so not all the neighbors where visited

    nh_idx = -1;

    if(rt_info_aux->count < NEIGHBORS_BEFORE_PARENT){
        // Find the neighbor closest to the destination
        best_dist = 10000;
        best_mask = 10000;
        for(neigh_t_aux = neigh_table, i=0; (i < MAX_NEIGHBORS) && (neigh_t_aux != NULL); i++, neigh_t_aux = neigh_t_aux->next)
            if(!rt_info_aux->visited_bitmap[i] && neigh_t_aux->available){
                // This neighbor is available and has not been visited yet
                // Get the distance to the destination
                // Is rendez-vous?
                aux_c = *(pkt + FLAGS_OFFSET);
                d = (aux_c & MASK_RV) ? dist_w_mask(pkt+DST_ADDR_OFFSET, *neigh_t_aux) : dist(pkt+DST_ADDR_OFFSET, *neigh_t_aux);

                fprintf(stderr,"Distance to neighbor INDEX %d %s; %d\n", i, ip6_to_str(neigh_t_aux->prim_addr), d);

//                if(d < best_dist || (d == best_dist && (neigh_t_aux->prim_mask < best_mask))){
                if(d < best_dist || (d == best_dist && (!check_addr_space(pkt + DST_ADDR_OFFSET, &(neigh_t_aux->prim_addr), neigh_t_aux->prim_mask)))){
                    nh_idx = i;
                    best_dist = d;
                    best_mask = neigh_t_aux->prim_mask;
                    fprintf(stderr,"Best neighbor until now is %s\n", ip6_to_str(neigh_t_aux->prim_addr));
                }
            }
    }
    else{
        if(!rt_info_aux->visited_bitmap[0] && neigh_table->available)
            nh_idx = 0;
        else{
        //Closest neighbor and parent where visited, so find the next available
            for(i=0, neigh_t_aux = neigh_table;(neigh_table != NULL) && i<MAX_NEIGHBORS; i++, neigh_t_aux = neigh_t_aux->next){
                if(!rt_info_aux->visited_bitmap[i] && neigh_t_aux->available){
                    nh_idx=i;
                    break;
                }
            }
        }
    }



    if(nh_idx<0){
        fprintf(stderr,"Send to Next Neighbor: Expected to find a next hop\n");
        return;
    }

    //We found the next neighbor to send the packet to, so install this next hop in the rt.
    print=0;

    rt=rt_table_dump(print);
    for(i=0; i!=rt_id; i++, rt=rt->next);
    if(memcmp(&(rt->dst), pkt+DST_ADDR_OFFSET, sizeof(struct in6_addr))){
        fprintf(stderr,"The entry we have is for %s with index %d\n",ip6_to_str(rt->dst), rt_id);
        fprintf(stderr,"route: could not find the requested entry in the rt for DST: %s\n", ip6_to_str(*(struct in6_addr *)(pkt+DST_ADDR_OFFSET)));
        return;
    }
    //nh_idx is the index to the best nh, in the neighbor map. Get the nh




    for(i=0, neigh_t_aux=neigh_table; i!=nh_idx; i++, neigh_t_aux=neigh_t_aux->next);
    memcpy(&(nh), &(neigh_t_aux->prim_addr), sizeof(struct in6_addr));

//    add_rt_route(rt->dst, nh, rt->dst_mask, rt->distance);


    // HERE DST->MASK IS WRONG !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1
                // CORRECT THIS, IT SHOULD START WITH 64 BY NOW, BUT THIS SHOULD BE DIFERENT WHEN WE USE MORE BITS FOR THE HOSTS
                // USE 128 BY NOW
    //Delete the old one
    del_rt_route(rt->dst, rt->gtw, rt->dst_mask, rt->distance);

    add_rt_route(rt->dst, nh, 128, rt->distance);
    if(neigh_t_aux->sec_addr_conn)
        add_src_nat(&(rt->dst), &sec_addr);

/*
    memcpy(&(rt_params.dst), &(rt->dst), sizeof(struct in6_addr));
    memcpy(&(rt_params.gtw), &(nh), sizeof(struct in6_addr));

    rt_params.distance = rt->distance;
    rt_params.mask = 128;
*/


//    rt_route_t.handler = &del_rt_route2;
/*
    rt_route_t.handler = &check_neighbors;
 //   rt_route_t.data = &rt_params;
    rt_route_t.data = NULL;

    rt_route_t.used = 0;

    timer_add_msec(&rt_route_t, 3004);
*/



    fprintf(stderr,"We found a neighbor to send the packet to. Route installed\n");
    fprintf(stderr,"DST: %s; GTW: %s\n", ip6_to_str(rt->dst), ip6_to_str(nh));
    

    rt_info_aux->visited_bitmap[nh_idx] = 1;
    *(pkt + TTL_OFFSET) = *(pkt + TTL_OFFSET) - 1;
    // Check this, the operating system will modify this automatically
    return;
}


void return_from(unsigned char *pkt){

    struct in6_addr addr_aux;
    struct route *rt;
    char print;
    // First we get the from address...

        memcpy(&addr_aux, pkt + FROM_ADDR_OFFSET, sizeof(struct in6_addr));
        // Now we look for this dst in the routing table
        print=0;
        rt = rt_table_dump(print);
        for(;rt != NULL; rt = rt->next){
            // If we already had an entry for this dst, delete it
            if(!memcmp(&(rt->dst), pkt + DST_ADDR_OFFSET, sizeof(struct in6_addr))){
                del_rt_route(rt->dst, rt->gtw, 128, MAX_TTL);    // Perhaps the 128 is not necessary, check this. Same with distance
                fprintf(stderr,"return_from function will delete the route DST: %s; GTW: %s\n", ip6_to_str(rt->dst), ip6_to_str(rt->gtw));
            }
        }
        // Add a new route to the destination of the packet with the from address as the gtw
        add_rt_route(*(struct in6_addr *)(pkt + DST_ADDR_OFFSET), addr_aux, 128, MAX_TTL);
        fprintf(stderr,"return_from function will add the route DST: %s; GTW: %s\n", ip6_to_str(*(struct in6_addr *)(pkt + DST_ADDR_OFFSET)), ip6_to_str(*(struct in6_addr *)(pkt + FROM_ADDR_OFFSET)));
        return;

}

struct route *rt_pairs_find(unsigned char *pkt){

    struct route *rt;
    char print;

    print =0;
    rt = rt_table_dump(print);

    for(;rt != NULL; rt = rt->next){
        //This is a reverse entry, so the destination in the routing table,
        //will be the source in the packet
        if(!memcmp(&(rt->dst), pkt + SRC_ADDR_OFFSET, sizeof(struct in6_addr)))
//            if(!memcmp(&(rt->src), pkt + DST_ADDR_OFFSET, sizeof(struct in6_addr)))
                return rt;
    }
    return NULL;
}

struct route *rt_entry_find(unsigned char *pkt){




};

void debug_pkt(unsigned char *pkt){

    struct in6_addr addr_aux;
    int aux_int;
    unsigned char aux_char;
    fprintf(stderr,"\n");
    fprintf(stderr,"\n");
    memcpy(&addr_aux, pkt+DST_ADDR_OFFSET, sizeof(struct in6_addr));
    fprintf(stderr,"\nDESTINATION: %s\n", ip6_to_str(addr_aux));
    memcpy(&addr_aux, pkt+SRC_ADDR_OFFSET, sizeof(struct in6_addr));
    fprintf(stderr,"\nSRC: %s\n", ip6_to_str(addr_aux));
    aux_int=(int)*(pkt+NXT_HDR_OFFSET);
    fprintf(stderr,"NEXT HEADER: %d\n", aux_int);
    if(aux_int==60){
        fprintf(stderr,"Destination Options\n");
        aux_int=(int)*(pkt+40);
        fprintf(stderr,"NEXT HEADER: %d\n", aux_int);
        if(aux_int==58){
            fprintf(stderr,"ICMPv6\n");
            aux_int=(int)*(pkt+72);
            fprintf(stderr,"ICMP6 Type: %d\n", aux_int);
            aux_int=(int)*(pkt+73);
            fprintf(stderr,"ICMP6 Code: %d\n", aux_int);
        }else if(aux_int==17){
            fprintf(stderr,"UDP Packet \n");
            aux_char=(unsigned char)*(pkt+80);
            if(aux_char == RV_REG){
                fprintf(stderr,"the packet is a RV Registration request\n");
            }
            aux_char= (unsigned char)*(pkt+72);
            fprintf(stderr,"Primer byte de source port: %x\n", aux_char);

            aux_char= (unsigned char)*(pkt+73);
            fprintf(stderr,"Segundo byte de source port: %x\n", aux_char);
            if(aux_char == 53)
                fprintf(stderr,"This is a DNS response\n");
            aux_char= (unsigned char)*(pkt+74);
            fprintf(stderr,"Primer byte de destination port: %x\n", aux_char);
            aux_char= (unsigned char)*(pkt+75);
            fprintf(stderr,"Segundo byte de destination port: %x\n", aux_char);
            if(aux_char == 53)
                fprintf(stderr,"This is a DNS query\n");
            aux_char= (unsigned char)*(pkt+76);
            fprintf(stderr,"Primer byte de Length: %x\n", aux_char);
            aux_char= (unsigned char)*(pkt+77);
            fprintf(stderr,"Segundo byte de Length: %x\n", aux_char);
            aux_char= (unsigned char)*(pkt+78);
            fprintf(stderr,"primer byte de checksum: %x\n", aux_char);
            aux_char= (unsigned char)*(pkt+79);
            fprintf(stderr,"segundo byte de checksum: %x\n", aux_char);
            aux_char= (unsigned char)*(pkt+80);
            fprintf(stderr,"primer byte de ID: %x\n", aux_char);
            aux_char= (unsigned char)*(pkt+81);
            fprintf(stderr,"segundo byte de ID: %x\n", aux_char);
            aux_char= (unsigned char)*(pkt+82);
            fprintf(stderr,"primer byte de flags: %x\n", aux_char);
            aux_char= (unsigned char)*(pkt+83);
            fprintf(stderr,"segundo byte de flags: %x\n", aux_char);
            }

    }else if(aux_int==58){
        aux_int=(int)*(pkt+40);
        fprintf(stderr,"ICMP6 Type: %d\n", aux_int);
    }
    fprintf(stderr,"\n");
    fprintf(stderr,"\n");
}



void set_returned(unsigned char *pkt){

    *(pkt + FLAGS_OFFSET) = (*(pkt + FLAGS_OFFSET)) | MASK_IR;
    return;

}

void set_is_antop(unsigned char *pkt){
    unsigned char aux;
    memcpy(&aux, pkt+FLAGS_OFFSET, 1);
    aux=aux|MASK_IA;
    memcpy(pkt+FLAGS_OFFSET, &aux, 1);
    return;

}

//Check if addr1, is under the address space of addr2/mask

int check_addr_space(struct in6_addr *addr, struct in6_addr *addr2, unsigned char mask_addr){
    struct in6_addr *address1 = NULL;
    struct in6_addr *address2 = NULL;
    uint16_t mask, j;
    int words, bits, i;
    unsigned char aux;

    address1 = malloc(sizeof(struct in6_addr));
    address2 = malloc(sizeof(struct in6_addr));

    if(address1 == NULL || address2 == NULL){
        fprintf(stderr,"Check_addr_space: Could not locate space\n");
        return -1;
    }

    memcpy(address1, addr, sizeof(struct in6_addr));
    memcpy(address2, addr2, sizeof(struct in6_addr));

    fprintf(stderr,"check_addr_space: Máscara: %d\n", mask_addr);
    fprintf(stderr,"check_addr_space: DST: %s\n", ip6_to_str(*address1));
    fprintf(stderr,"check_addr_space: ADDRESS: %s\n", ip6_to_str(*address2));

    words = mask_addr / 16;
    bits = mask_addr % 16;
    mask = pow(2,15);

    fprintf(stderr,"WORDS: %d\n", words);
    fprintf(stderr,"BITS: %d\n", bits);
    fprintf(stderr,"MASK: %d\n", mask);

//    fprintf(stderr,"ADDRESS 2: %x\n", address2->s6_addr16[i+4]);
 //   fprintf(stderr,"ADDRESS 1: %x\n", address1->s6_addr16[i+4]);

    for(i = 0; i < words; i++){
        if(memcmp(&(address2->s6_addr16[i+4]), &(address1->s6_addr16[i+4]), sizeof(uint16_t))){
            fprintf(stderr,"check_addr_space: There are differences in the network bits\n");
        // This means that there are differences in the network bits, so we should do nothing
        // since it is not in our address space.
            return 1;
        }
    }
//    fprintf(stderr,"PRUEBA\n");
    //Now compare the bits for the last word.
    for(j=0; j < bits; j++, mask = mask>>1){
        if(htons(htons(address2->s6_addr16[i+4]) & mask) != htons(htons(address1->s6_addr16[i+4]) & mask)){
            fprintf(stderr,"The destination address for the RV is not in our address space\n");
            //If any of these bits are different, the destination address is not in this address space.
            return 1;
        }
    }
    return 0;

}


int rv_route(unsigned char *pkt){

    struct in6_addr addr_aux;
    uint16_t mask, j;
    int words, bits, i;
    unsigned char aux;
    struct recovered_addr *rcvd_addr_aux;

    aux=*(pkt + UDP_PKT_OFF);


    // For RV pkts, if the destination belongs to the current address space, then we should process the packet.
    // otherwise do nothing and let it go through the routing process.
    memcpy(&addr_aux, pkt + DST_ADDR_OFFSET, sizeof(struct in6_addr));
    // Compare the network bits in prim addr with the same number in the destination address. If they are all the same,
    // then we should process the packet.
    //First determine how many words should be equal.
/*
    words = prim_mask / 16;
    bits = prim_mask % 16;
    mask = htons(pow(2,15));

    for(i = 0; i < words; i++){
        if(memcmp(&(prim_addr.s6_addr16[i+4]), &(addr_aux.s6_addr16[i+4]), sizeof(uint16_t))){
        // This means that there are differences in the network bits, so we should do nothing
        // since it is not in our address space.
            for(rcvd_addr_aux = rcvd_addr; rcvd_addr_aux != NULL; rcvd_addr_aux = rcvd_addr_aux->next);
            if(rcvd_addr_aux == NULL){
                fprintf(stderr,"The destination address is not a recovered address\n");
                return RV_ACCEPT;
            }
        }
    }
    //Now compare the bits for the last word.
    for(j=0; j < bits; j++, mask = mask>>1){
 //       fprintf(stderr,"WORD NUMBER %d: %x\n",i,htons(addr_aux.s6_addr16[i+4]));
//        addr_aux.s6_addr16[i+4] = htons(htons(addr_aux.s6_addr16[i+4]) & htons(mask));
 //       fprintf(stderr,"WORD NUMBER %d: %x\n",i,htons(addr_aux.s6_addr16[i+4]));

        if(htons(htons(prim_addr.s6_addr16[i+4]) & htons(mask)) != htons(htons(addr_aux.s6_addr16[i+4]) & htons(mask))){
            fprintf(stderr,"The destination address for the RV is not in our address space\n");
            //If any of these bits are different, the destination address is not in this address space.
            for(rcvd_addr_aux = rcvd_addr; rcvd_addr_aux != NULL; rcvd_addr_aux = rcvd_addr_aux->next);
            if(rcvd_addr_aux == NULL){
                fprintf(stderr,"The destination address is not a recovered address\n");
                return RV_ACCEPT;
            }
        }
    }

*/
    rcvd_addr_aux = NULL;
    if(check_addr_space(&addr_aux, &prim_addr, prim_mask)){
        fprintf(stderr,"RV_ROUTE: the destination address is not in our address space: %s\n", ip6_to_str(addr_aux));
        fprintf(stderr,"Will now check recovered addresses\n");
        //This address is not in our address space
        for(rcvd_addr_aux = rcvd_addr; rcvd_addr_aux != NULL; rcvd_addr_aux = rcvd_addr_aux->next){
            if(!check_addr_space(&addr_aux, &(rcvd_addr_aux->prim_addr), rcvd_addr_aux->prim_mask)){
                fprintf(stderr,"The destination address is under the address space of the recovered address space %s, with mask %d\n", ip6_to_str(rcvd_addr_aux->prim_addr), rcvd_addr_aux->prim_mask);
                break;
            }
        }
        /*
            if(rcvd_addr_aux == NULL){
                // Nor is a recovered address
                fprintf(stderr,"The destination address is not a recovered address\n");
                return RV_ACCEPT;
            }
*/
        if(rcvd_addr_aux == NULL){
            fprintf(stderr,"No recovered addresses\n");
            if(!sec_addr_flag){
                fprintf(stderr,"No secondary address to check for\n");
                return RV_ACCEPT;
            }
        }
        //Now we check for the secondary address
        if(sec_addr_flag){
            if(check_addr_space(&addr_aux, &sec_addr, sec_mask)){
                fprintf(stderr,"The destination is not under the address space of the secondary address\n");
                return RV_ACCEPT;
            }
        }
    }




    fprintf(stderr,"AUX VALE %d\n", aux);

    //If we didn't leave the function it means that we need to process the packet.

    switch (aux){

        case RV_REG:
            fprintf(stderr,"An RV_REG pkt with a succesor address has been received or is being sent\n");
            rv_table_add((struct rv_ctrl_pkt *)(pkt + UDP_PKT_OFF));
            print_rv_table();
            break;

        case RV_ADDR_LOOKUP:
            fprintf(stderr,"An RV_ADDR_LOOKUP pkt with a succesor address has been received or is being sent\n");
            answer_rv_query((struct ctrl_pkt *)(pkt + UDP_PKT_OFF), socket_global);
            break;
    }

    //----------------------------------------------------------------------------------
    // THIS ORIGINAL IDEA IS NOT WORKING. THE PACKET IS NOT ENTERING THE UDP SOCKET, SO ||
    // WE PROCESS THE PACKET DIRECTLY                                                   ||
    //----------------------------------------------------------------------------------

    //Change the destination address to be this node

/*
    fprintf(stderr,"The destination address for this packet will be changed to be this primary address \n");
    memcpy(pkt + DST_ADDR_OFFSET, &prim_addr, sizeof(struct in6_addr));
    fprintf(stderr,"The new destination address for this packet is %s\n", ip6_to_str(*((struct in6_addr*)(pkt + DST_ADDR_OFFSET))));



*/
    // The pkt dest is within our address space, so we process it and then drop it
    return RV_DROP;

}



int route(unsigned char *pkt, u_int8_t hook){

    unsigned char aux;
    struct in6_addr addr_aux, addr_aux1;
    struct route *rt;
    int distance, i, needs_join = 0;
    struct route *reverse_entry;
    struct rt_aux_info *rt_info_aux;
    struct neighbor *neigh_t_aux;
    unsigned char src_neigh;
    reverse_entry = NULL;
    char print;

    //If the packet is multicast it should use the default gateway



    //If this are the packets involved in getting a primary address, let them
    // go through the default gateway

    if(*(pkt + UDP_PKT_OFF) > 0 && *(pkt + UDP_PKT_OFF) < 5){
        return RV_ACCEPT;
    }

    // This is for MDNS query response. It's an auto generated message for RV
    if(!memcmp(pkt+DST_ADDR_OFFSET, &prim_addr, sizeof(struct in6_addr)) && !memcmp(pkt+SRC_ADDR_OFFSET, &prim_addr, sizeof(struct in6_addr)))
        return RV_ACCEPT;


    fprintf(stderr,"El paquete con destino %s entro a route, en el hook %d\n",ip6_to_str(*((struct in6_addr *)(pkt + DST_ADDR_OFFSET))), hook);
    // Check that this is a local address. Compare the first two octects
    // of the address. They sholud be fe80 as in the primary address.

    if(memcmp(pkt+DST_ADDR_OFFSET, &prim_addr, sizeof(uint16_t)))
        return RV_ACCEPT;

    aux = *(pkt + FLAGS_OFFSET);

    // Check if the packet was returned.
    if(hook == NF_IP6_PRE_ROUTING){

        if((aux & MASK_IR) != 0){
            //Is returned...
            //Turn off IS flag
            fprintf(stderr,"The packet is returned, send it to the next neighbor\n");
            *(pkt + FLAGS_OFFSET) = *(pkt + FLAGS_OFFSET) & (~MASK_IR);
            send_to_next_neighbor(pkt);
            return RV_ACCEPT;
        }

        // If the packet came back to the source and the hook is Pre Routing, return it.


        if(!memcmp(pkt + SRC_ADDR_OFFSET, &prim_addr, sizeof(struct in6_addr)) && hook == NF_IP6_PRE_ROUTING){
            fprintf(stderr,"The packet came back to the source, so send it back to the from addr: %s\n", ip6_to_str(*(struct in6_addr *)(pkt + FROM_ADDR_OFFSET)));
            set_returned(pkt);
            *(pkt + TTL_OFFSET) += 1;   //CHEQUEAR ESTO, EL SO TAMBIEN LO MODIFICARA
            return_from(pkt);
            return RV_ACCEPT;
        }

    }



    //******************************************************************************************
    // AQUI HAY QUE CHEQUEAR QUE EFECTIVAMENTE LOS PAQUETES RV QUE SALEN CON UN DESTINO DENTRO DE NUESTRO ESPACIO
    // DIRECCIONES, SEAN PROCESADOS LOCALMENTE.
    //******************************************************************************************



    fprintf(stderr,"We will check if the packet is RV and needs to be procesed by local node\n");

    //If the packet is RV, and we are not the destination node, check if we need to process it anyway.
        if((aux & MASK_RV) != 0 && (memcmp(pkt + DST_ADDR_OFFSET, &prim_addr, sizeof(struct in6_addr)))){
            fprintf(stderr, "The packet is RV\n");
            fprintf(stderr,"The primary mask is %d\n", prim_mask);
            // We must check if the destination is a RV server in our address space. If that is the case,
            // change the destination address to be this node, and process the packet.
            if(rv_route(pkt) == RV_DROP){
                return RV_DROP;
            }
        }

    // Insert or update the reverse entry, if the from address differs from the primary one (i.e. is not the starting node)
    // and if the source is not a neighbor

    src_neigh=0;

    for(neigh_t_aux=neigh_table; neigh_t_aux!=NULL; neigh_t_aux=neigh_t_aux->next)
        if(!memcmp(&neigh_t_aux->prim_addr, pkt+SRC_ADDR_OFFSET,sizeof(struct in6_addr)))
            src_neigh=1;


    struct in6_addr d_a;
    memcpy(&d_a, pkt + FROM_ADDR_OFFSET, sizeof(struct in6_addr));
    fprintf(stderr,"FROM ADDR: %s\n", ip6_to_str(d_a));

    if(memcmp(pkt + FROM_ADDR_OFFSET, &prim_addr, sizeof(struct in6_addr)))
        fprintf(stderr,"Las direcciones son distintas\n");
    fprintf(stderr,"PRIM ADDR :%s\n", ip6_to_str(prim_addr));
    fprintf(stderr,"src neighbor: %d\n", src_neigh);
    fprintf(stderr,"HOOK: %d\n", hook);

    if(memcmp(pkt + FROM_ADDR_OFFSET, &prim_addr, sizeof(struct in6_addr)) && src_neigh == 0 && hook == NF_IP6_PRE_ROUTING ){
        // The TTL field (Hop Count) is initialized to MAX_TTL when we get the packet from the local out hook
        fprintf(stderr,"Prueba1\n");
        distance = MAX_TTL - *(pkt + TTL_OFFSET);

        // We look for the reverse Entry.
        print=0;
        rt = rt_table_dump(print);

        for(i = 0;rt != NULL; rt = rt->next, i++){
            fprintf(stderr,"Prueba2\n");

            // ARE WE LOOKING IN ALL OF THE ENTRIES??? CHECK THIS

            if(!memcmp(&(rt->dst), pkt + SRC_ADDR_OFFSET, sizeof(struct in6_addr))){

                fprintf(stderr,"Prueba3\n");

                // It has the same destination, now check the src addr in the aux info table (Reverse Entry)    CHEQUEAR ESTO!!!!!!!
                for(rt_info_aux = rt_info; (rt_info_aux != NULL) && (rt_info_aux->route_id != i); rt_info_aux = rt_info_aux->next);
                fprintf(stderr,"Prueba4\n");
                    if(rt_info_aux != NULL){
                        // It could happen that the reverse entry exists, but the index is not the right one. CHECK THIS
                        if(!memcmp(&(rt_info_aux->src), pkt + DST_ADDR_OFFSET, sizeof(struct in6_addr))){
                            //We found a reverseEntry
                            if((distance > rt->metric) && hook == NF_IP6_PRE_ROUTING){
                                // This means that the packet is looping, send it back
                                fprintf(stderr,"The packet DST: %s; SRC: %s is in a loop, so send it back where it came from\n", ip6_to_str(*((struct in6_addr *)(pkt + SRC_ADDR_OFFSET))), ip6_to_str(rt->dst));
                                set_returned(pkt);
                                *(pkt + TTL_OFFSET) += 1;
                                return_from(pkt);
                                return RV_ACCEPT;
                            }else{
                                needs_join=1;
                            }
                //The distance to the source is better than we have in rt so update the route
                        }
                    }


                reverse_entry = malloc(sizeof(struct route));
                if(reverse_entry == NULL){
                    fprintf(stderr,"Could not locate memory for reverse entry\n");
                    return RV_ACCEPT;
                }
                fprintf(stderr,"Prueba5\n");
                memcpy(reverse_entry, rt, sizeof(struct route));
//                reverse_entry = rt;

                //**************************************************************************************************************************
                                                                                                                                         //*
                // AQUI SE ASUME QUE LA RUTA VIEJA Y LA NUEVA QUEDAN EN EL MISMO LUGAR DE LA TABLA, CON LO CUAL NO SE MODIFICA EL ORDEN EN *
                // LA TABLA DE ENTRADAS AUXILIARES. CHEQUEAR ESTO !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!                     *
                                                                                                                                         //*
                //***************************************************************************************************************************
                fprintf(stderr,"Prueba6\n");
                memcpy(&(addr_aux), pkt + FROM_ADDR_OFFSET, sizeof(struct in6_addr));
                fprintf(stderr,"Prueba7\n");
                fprintf(stderr,"The Reverse Entry will be updated. So delete the old one\n");
                fprintf(stderr,"DST: %s; GTW: %s ", ip6_to_str(reverse_entry->dst), ip6_to_str(reverse_entry->gtw));
                fprintf(stderr,"And add the new one\n");
                fprintf(stderr,"GTW: %s\n", ip6_to_str(addr_aux));
                del_rt_route(reverse_entry->dst, reverse_entry->gtw, 128, reverse_entry->distance);
                add_rt_route(reverse_entry->dst, addr_aux, 128, distance);
             }

        }
        if(rt == NULL){
            //There is no entry with the pkt SRC as DST
            needs_join = 1;

// I think what comes next is wrong.

/*
            for(i = 0;rt != NULL; rt = rt->next, i++){
                if(!memcmp(&(rt->dst), pkt + SRC_ADDR_OFFSET, sizeof(struct in6_addr)))
                    if(!memcmp(&(rt->gtw), pkt + FROM_ADDR_OFFSET, sizeof(struct in6_addr))){
                        reverse_entry = malloc(sizeof(struct route));
                        if(reverse_entry == NULL){
                            fprintf(stderr,"Could not locate memory for reverse entry\n");
                            return;
                        }
                        memcpy(reverse_entry, rt, sizeof(struct route));
//                        reverse_entry = rt;
                        //**************************************************************************************************************************
                                                                                                                                         //*
                        // AQUI SE ASUME QUE LA RUTA VIEJA Y LA NUEVA QUEDAN EN EL MISMO LUGAR DE LA TABLA, CON LO CUAL NO SE MODIFICA EL ORDEN EN *
                        // LA TABLA DE ENTRADAS AUXILIARES. CHEQUEAR ESTO !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!                     *
                                                                                                                                         //*
                        //***************************************************************************************************************************
                        memcpy(&addr_aux, pkt + FROM_ADDR_OFFSET, sizeof(struct in6_addr));
//                      del_rt_route(reverse_entry->dst, reverse_entry->gtw, 128, reverse_entry->distance);
//                      add_rt_route(reverse_entry->dst, addr_aux, 128, distance);
                        break;
                    }
            }
*/
            if(reverse_entry == NULL){
                reverse_entry = malloc(sizeof(struct route));
                if(reverse_entry == NULL){
                    fprintf(stderr,"Could not locate space for \n");
                    return RV_ACCEPT;
                }
                //No entries found with dst = src pkt & gtw = from pkt. Add it
                memcpy(&(reverse_entry->dst), pkt + SRC_ADDR_OFFSET, sizeof(struct in6_addr));
                memcpy(&(reverse_entry->gtw), pkt + FROM_ADDR_OFFSET, sizeof(struct in6_addr));
                add_rt_route(reverse_entry->dst, reverse_entry->gtw, 128, distance);


//                memcpy(&addr_aux, pkt + SRC_ADDR_OFFSET, sizeof(struct in6_addr));
//                memcpy(&addr_aux1, pkt + FROM_ADDR_OFFSET, sizeof(struct in6_addr));
//                fprintf(stderr,"route function will add the route DST: %s\n",ip6_to_str(addr_aux));
//                add_rt_route(addr_aux, addr_aux1, 128, distance);
            }
        }
    }

    // If the destination address is the current node, it arrived
    if(!memcmp(pkt + DST_ADDR_OFFSET, &prim_addr, sizeof(struct in6_addr)))
        return RV_ACCEPT;

    // If the destination address is an available neighbor, let it go through the default gateway.
    for(neigh_t_aux=neigh_table; neigh_t_aux!=NULL; neigh_t_aux=neigh_t_aux->next)
        if(!memcmp(&neigh_t_aux->prim_addr, pkt+DST_ADDR_OFFSET,sizeof(struct in6_addr)) && neigh_t_aux->available){
            fprintf(stderr, "\nThe destination is a neighbor !!!!\n");
            return RV_ACCEPT;
        }

    // Find if there are entries to the packet destination
    print = 0;
    rt = rt_table_dump(print);

    for(; rt != NULL; rt = rt->next){
        if(!memcmp(&(rt->dst), pkt + DST_ADDR_OFFSET, sizeof(struct in6_addr)) && memcmp(&(rt->gtw), pkt + DST_ADDR_OFFSET, sizeof(struct in6_addr))){
            fprintf(stderr,"There is an entry in the rt for destination %s; GW: %s\n",ip6_to_str(*((struct in6_addr *)(pkt + DST_ADDR_OFFSET))), ip6_to_str(rt->gtw));
            break;
        }
    }
    // If there aren't, send to next neighbor
    if(rt == NULL){
        fprintf(stderr,"El paquete con destino %s será enviado al siguiente vecino\n",ip6_to_str(*((struct in6_addr *)(pkt + DST_ADDR_OFFSET))));
        send_to_next_neighbor(pkt);
        return RV_ACCEPT;
    }
        // There are entries for this destination,
        // so find the one with shortest path
        // The distance will be the "metrics" field in the routing table.
        // The kernel will use the route with the lower value automatically

    // If the entry is empty, there is no route to host from this node, so send it back
    // CHEQUEAR ESTO !!!!!
    // If the next hop is the address where the packet came from, send it back as returned
    if(!memcmp(&(rt->gtw), pkt + FROM_ADDR_OFFSET, sizeof(struct in6_addr)) && hook == NF_IP6_PRE_ROUTING){
        set_returned(pkt);
        *(pkt + TTL_OFFSET) += 1;
        return_from(pkt);
    }
    // If the next hop is not available anymore, send the packet to the next neighbor
    for(neigh_t_aux=neigh_table; neigh_t_aux!=NULL; neigh_t_aux=neigh_t_aux->next)
        if(!memcmp(&(neigh_t_aux->prim_addr), &(rt->gtw), sizeof(struct in6_addr)) && !neigh_t_aux->available )
            send_to_next_neighbor(pkt);

    // Add the reverse entry to the aux rt table in case it doesn't exist
    // If it is NULL by this point, it means that the from address is the primary
    // or the source is a neighbor

    print =0;

    if(needs_join){
        //Get the route id
        //Search for the reverse entry id
        for(rt=rt_table_dump(print), i=0; rt!=NULL; rt=rt->next, i++){
            if(!memcmp(&(rt->dst), &(reverse_entry->dst), sizeof(struct in6_addr)))
                if(!memcmp(&(rt->gtw), &(reverse_entry->gtw), sizeof(struct in6_addr)))
                    break;
        }
        if(rt!=NULL)
            add_rt_info(&(rt->dst), pkt+SRC_ADDR_OFFSET, i);
        else{
            fprintf(stderr, "route: Could not find the requested reverse entry\n");
            return RV_ACCEPT;
        }
    }
}



unsigned char *print_pkt (struct nfq_data *tb, int *pkt_size, int *id, unsigned char* accept)
{

	struct nfqnl_msg_packet_hdr *ph;
	struct nfqnl_msg_packet_hw *hwph;
	u_int32_t mark,ifi;
	int ret;
	unsigned char *data;
        unsigned char *data_aux;
        unsigned char aux;
        uint16_t cksum;
        struct in6_addr addr_aux;

        struct ip6_hdr *ip;
        struct icmp6_hdr *icmp = NULL;
        struct udphdr *udp = NULL;
        char *tcp_hdr = NULL;
        struct src_nat *src_nat_aux;

        // By default we accept the packet to go through the IPv6 hooks

        *accept = 1;

	ph = nfq_get_msg_packet_hdr(tb);
	if (ph) {
		*id = ntohl(ph->packet_id);
//		fprintf(stderr,"hw_protocol=0x%04x hook=%u id=%u \n", ntohs(ph->hw_protocol), ph->hook, *id);
	}

        int option_value, option_len;

	hwph = nfq_get_packet_hw(tb);
	if (hwph) {
		int i, hlen = ntohs(hwph->hw_addrlen);
	}
	mark = nfq_get_nfmark(tb);
	if (mark)
		fprintf(stderr,"mark=%u ", mark);
        // data will point to the IPv6 packet.
        // ret holds the entire IPv6 packet length i.e. header + payload
	ret = nfq_get_payload(tb, &data_aux);
        if(data_aux==NULL){
            fprintf(stderr,"pkt_handle: could not locate space for packet in user space\n");
            return NULL;
        }
/*
	if (ret >= 0)
		fprintf(stderr,"IPv6 Packet Len=%d \n", ret);
*/
        ip = (struct ip6_hdr *) data_aux;

        memcpy(&addr_aux, data_aux+DST_ADDR_OFFSET, sizeof(struct in6_addr));
//        fprintf(stderr,"\nDST : %s \n", ip6_to_str(addr_aux));
        memcpy(&addr_aux, data_aux+SRC_ADDR_OFFSET, sizeof(struct in6_addr));
//        fprintf(stderr,"\nSRC : %s \n", ip6_to_str(addr_aux));

        data=malloc((size_t)ret+32);
        memset(data, 0, ret+32);
        memcpy(data, data_aux, (size_t)ret);

//        icmp = (struct icmp6_hdr *) (data_aux +40);

        switch(ph->hook){
            case NF_IP6_FORWARD:
                fprintf(stderr,"El paquete entro en FORWARD y serà debuggeado\n");
//                debug_pkt(data);
                (*pkt_size) = ret;
                break;
            case NF_IP6_LOCAL_IN:
                fprintf(stderr,"El paquete entro en LOCAL IN y sera debuggeado\n");
                debug_pkt(data);
                (*pkt_size) = ret;

                break;

            case NF_IP6_PRE_ROUTING:
                aux = *(data + FLAGS_OFFSET);
                if((aux & MASK_IA) != 0){
 //                   (*pkt_size)=ret;
                    if(route(data, ph->hook)==RV_DROP){
                        *accept = 0;
                    }
                }
                if(!memcmp(data+SRC_ADDR_OFFSET, data+DST_ADDR_OFFSET, sizeof(struct in6_addr))){
 //                   debug_pkt(data);
                }
                (*pkt_size)=ret;

            break;
            case NF_IP6_LOCAL_OUT:
                (*pkt_size)=ret;
//                switch (ip->ip6_nxt) {
  //                  case IPPROTO_ICMPV6:
                if(ip->ip6_nxt==IPPROTO_ICMPV6){
                    icmp = (struct icmp6_hdr *) (data_aux +40);
                    if (icmp->icmp6_type == ND_NEIGHBOR_SOLICIT || icmp->icmp6_type ==ND_NEIGHBOR_ADVERT){
                        break;
                    }

                }
                else if(ip->ip6_nxt==IPPROTO_TCP){

                    tcp_hdr = data_aux + 53;
                    fprintf(stderr,"El valor de flags TCP: %d\n", *tcp_hdr);
                    if(*tcp_hdr == 2){
                        fprintf(stderr,"SYN packet. MSS will be modified in it\n");
                        *(tcp_hdr + 10) = 0x80;
                        *(data_aux + 59) = *(data_aux+59)+32;
                    }
                    

                    

                }
                else if(ip->ip6_nxt==IPPROTO_UDP){


//                    udp=(struct udphdr *)(data_aux+40);
                    udp=(struct udphdr *)(data+72);
//                    fprintf(stderr, "SOURCE PORT : %u\n", htons(udp->source));
                 }
//                     if (icmp->icmp6_type != ND_NEIGHBOR_SOLICIT && icmp->icmp6_type !=ND_NEIGHBOR_ADVERT){
                        set_is_antop(data);
                         memcpy(data+IPV6_HDR_LEN+32 , data_aux+IPV6_HDR_LEN, ret-IPV6_HDR_LEN);
                         ip = (struct ip6_hdr *) data;
//                         icmp = (struct icmp6_hdr *) (data +40);
                        //Set the payload lenght
                         uint8_t aux_2;
                         uint16_t aux_3;

                         aux_3=ret+32-IPV6_HDR_LEN;
                         aux_2 = (uint8_t)(aux_3>>8);

                        *(data+4)=aux_2;
                        *(data+5)=ret+32-IPV6_HDR_LEN;
                        //Set the hop count
                        *(data+7)= MAX_TTL;


                        // ******************** /

                        *(data+40)=ip->ip6_ctlun.ip6_un1.ip6_un1_nxt;


                        // ************* /


                        //Set next_header as destination option
                        ip->ip6_ctlun.ip6_un1.ip6_un1_nxt=60;
                        //Set next header as ICMPv6
 //                       *(data+40)=IPPROTO_ICMPV6;
                        //Set Option Len for dest option. 3*8+8
                        *(data+41)=3;
                        // Set PADN
                        *(data+42)=1;
                        //Ten zero bytes following
                        *(data+43)=10;
                        int c;
                        for(c=44;c<54;c++)
                            *(data+c) = 0;
                        //Set the option type for this destination option (from address)
                        //The Option Type identifiers are internally encoded such that their
                        //highest-order two bits specify the action that must be taken if the
                        //processing IPv6 node does not recognize the Option Type:
                        //00 - skip over this option and continue processing the header.
                        *(data+54)=16;
                        //Set the option data length (1 IPv6 address)
                        *(data+55)=16;
                        //Since this is local out, the packet is created in this host, so
                        //copy the primary address in the from address field.
                        memcpy(data+56, &prim_addr, sizeof(struct in6_addr));
                        //ICMPv6 needs a checksum
                        if(icmp!=NULL){
                            uint16_t cksum_aux=icmpv6_cksum(&(ip->ip6_src), &(ip->ip6_dst),(char *)(data) , ret-IPV6_HDR_LEN-ICMPV6_HDR_LEN, ret-IPV6_HDR_LEN);
                            *((uint16_t *)(data + 74)) = ntohs(cksum_aux);
//                            fprintf(stderr,"\n \n EL CKSUM CALCULADO: %x \n \n",cksum_aux);
                        }

                        if(udp!=NULL){
                            debug_pkt(data);
//                            fprintf(stderr, "SOURCE PORT : %u\n", htons(udp->source));
                            if(htons(udp->source) != MDNS_PORT && htons(udp->source) != ANTOP_PORT && htons(udp->source) != MDNS_PORT_AUX){
                                if((htons(udp->dest) == MDNS_PORT) || (htons(udp->dest) == DNS_PORT)){
                                    if(htons(udp->dest) == MDNS_PORT)
                                        fprintf(stderr, "Es un MDNS\n");
                                    else if(htons(udp->dest) == DNS_PORT)
                                        fprintf(stderr, "Es un DNS\n");
                                    //Only if we have one question
                                    if((*(data+85) == 1) && (*(data+82) == 1 && (*(data+80+MDNS_QUERY_OFFSET + strlen(data + 80 + MDNS_QUERY_OFFSET) + 2) == 28))  ){
                                    //In this case, the first param is the dns query
                                        fprintf(stderr,"INTENTO: %d\n",(*(data+80+MDNS_QUERY_OFFSET + strlen(data + 80 + MDNS_QUERY_OFFSET) + 2)));
                                        fprintf(stderr,"It is a standar query\n");
                                        // Force the source address to be the this node's prim_addr
                                        memcpy(data+SRC_ADDR_OFFSET, &prim_addr, sizeof(struct in6_addr));
                                        answer_dns_query(data, ret+32, udp->source, 0, udp->dest);
                                    }
                                }
                            }

                            // THIS IS FOR PROCESSING A RECORDS. NOT USED RIGTH NOW !!!!

/*
                            else if(htons(udp->source) == MDNS_PORT && htons(udp->dest) == MDNS_PORT){
                                //Only if we have one question
                                if(*(data+85) == 1){
 //                               if(*(int *)(udp+UDP_HDR_LEN+MDNS_QUERY_OFFSET+strlen((char *)(udp+UDP_HDR_LEN+MDNS_QUERY_OFFSET)) +1) == 1)
//                                    fprintf(stderr," EL TAMAÑO DE ESTA MIERDA ES %d \n", strlen((char *)(udp+UDP_HDR_LEN+MDNS_QUERY_OFFSET)));
                                    fprintf(stderr," SIZE IS: %d \n", strlen((char *)data+93));

                                    fprintf(stderr, "VALUE::  %d!!!! \n",*(data+95+strlen((char *)data+93)));
                                    // Aqui chequeo que el type sea A (01)
                                    if(*(data+95+strlen((char *)data+93)) == 1){
                                        answer_dns_query_a(data, ret+32, udp->source);
                                    }

                                }
                            }
*/


  //                          else if(htons(udp->source) == MDNS_PORT_AUX && htons(udp->dest) != MDNS_PORT){



//                              if(htons(udp->source) == MDNS_PORT_AUX && htons(udp->dest) != MDNS_PORT_AUX){
                            // This is the query response generated by the RV server. So change the source port to be 5353
                                if(htons(udp->source) == DNS_PORT_AUX && htons(udp->dest) != DNS_PORT_AUX){

                                fprintf(stderr, "ESTO SUFRIRÁ UN DNS 'PORT TRANSLATION'\n");
                                // We will change the source port, so UDP checksum should be recalculated
                                udp->source=htons(DNS_PORT);

                                struct in6_addr address_auxiliar;

                                //This is the address of the DNS server.

                                address_auxiliar.s6_addr16[0]=0x0120;
                                address_auxiliar.s6_addr16[1]=0x0000;
                                address_auxiliar.s6_addr16[2]=0x0000;
                                address_auxiliar.s6_addr16[3]=0x0000;
                                address_auxiliar.s6_addr16[4]=0x0000;
                                address_auxiliar.s6_addr16[5]=0x0000;
                                address_auxiliar.s6_addr16[6]=0x0000;
                                address_auxiliar.s6_addr16[7]=0x0c00;

                                memcpy(data+SRC_ADDR_OFFSET, &(address_auxiliar), sizeof(struct in6_addr));


                                //********************************************
                                //PRUEBA



                                //********************************************
                                uint16_t cksum_aux=udp_cksum(&(ip->ip6_src), &(ip->ip6_dst),(char *)(data) , ret-IPV6_HDR_LEN-ICMPV6_HDR_LEN, ret-IPV6_HDR_LEN);
                                *((uint16_t *)(data + 78)) = ntohs(cksum_aux);
//                                *((uint16_t *)(data + 78)) = 0x0000;
                                fprintf(stderr,"UDP PAYLOAD LENGTH: %d\n",ret-IPV6_HDR_LEN-ICMPV6_HDR_LEN );
                                fprintf(stderr,"IPv6 PAYLOAD LENGTH: %d\n", ret-IPV6_HDR_LEN);
                                fprintf(stderr,"\n \n EL CKSUM CALCULADO UDP: %x \n \n",cksum_aux);


                            }

                            //If the outgoing packet is a RV, set the flag
                            if(*(data + IPV6_HDR_LEN + DEST_OPT_HDR + 2) == 0x11 && *(data + IPV6_HDR_LEN + DEST_OPT_HDR + 3) == 0x75)
                            if(*(data + UDP_PKT_OFF) == RV_REG || *(data + UDP_PKT_OFF) == RV_ADDR_LOOKUP || *(data + UDP_PKT_OFF) == RV_ADDR_SOLVE){
                                fprintf(stderr, "The outgoing packet is a RV, so it will be marked like that\n");
                                *(data + FLAGS_OFFSET) = *(data + FLAGS_OFFSET) | (MASK_RV);
                            }

                        }
/*
                        else if(tcp_hdr != NULL){
                            uint16_t cksum_aux=udp_cksum(&(ip->ip6_src), &(ip->ip6_dst),(char *)(data) , ret-IPV6_HDR_LEN-20, ret-IPV6_HDR_LEN);
                                *((uint16_t *)(data + 88)) = ntohs(cksum_aux);

                        }
*/

                        // NAT the src address if there is an entry for it in the table.
                        if(nat_table!=NULL){
                            for(src_nat_aux = nat_table; src_nat_aux != NULL; src_nat_aux = src_nat_aux->next){
                                if(!memcmp(data+DST_ADDR_OFFSET, &(src_nat_aux->dst), sizeof(struct in6_addr))){
                                    fprintf(stderr,"The SRC address will be nated for this host\n");
                                    memcpy(data+SRC_ADDR_OFFSET, &(src_nat_aux->src), sizeof(struct in6_addr));
                                    break;
                                }
                            }
                        }
                        (*pkt_size)=ret+32;
                        add_local_out = 32;
                        route(data, ph->hook);

//*********************************************************************************************
//*********************************************************************************************
//      PRUEBA SSH

                   

//*********************************************************************************************
//*********************************************************************************************


/*
                    }else
                        (*pkt_size)=ret;
*/
  //              }
            break;
            case NF_IP6_POST_ROUTING:
                *pkt_size=ret;
/*
                fprintf(stderr, "El paquete entro en POST ROUTING\n");
                debug_pkt(data);
*/

            break;
        }
        return data;
}





static int cb(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg, struct nfq_data *nfa, void *data)
{


        int pkt_size;
        int id=0;
        unsigned char *data_aux;
        struct icmp6_hdr *icmp;
        unsigned char accept;



        data_aux = print_pkt(nfa, &pkt_size, &id, &accept);  // ACA SE MODIFICA EL PAQUETE.

        icmp = (struct icmp6_hdr *) (data_aux +40);

        struct nfqnl_msg_packet_hdr *ph_aux;
        ph_aux = nfq_get_msg_packet_hdr(nfa);

// In data_aux we will find the modified packet, but nfq_get_payload returns the size of the original message, so we have to return
// the modified size to the kernel.

//        pkt_size = nfq_get_payload(nfa, &data_aux);



 //       struct ip6_hdr *ip_aux;
        int ret;

        switch(ph_aux->hook){
            case NF_IP6_FORWARD:
 //               fprintf(stderr,"ENTRO UN PAQUETE AL HOOK FORWARD\n");
                ret = nfq_set_verdict(qh, id, NF_ACCEPT, pkt_size, data_aux);
                break;
            case NF_IP6_LOCAL_IN:
                ret = nfq_set_verdict(qh, id, NF_ACCEPT, pkt_size, data_aux);
                break;
            case NF_IP6_LOCAL_OUT:
//              ret= nfq_set_verdict(qh, id, NF_ACCEPT, 0,NULL);
//              ret= nfq_set_verdict(qh, id, NF_ACCEPT, pkt_size + add_local_out, data_aux); // EN ESTE CASO SUMAMOS 16 BYTES DE DESTINATION OPTIONS
 //               fprintf(stderr,"ENTRO UN PAQUETE AL HOOK LOCAL OUT\n");
                ret = nfq_set_verdict(qh, id, NF_ACCEPT, pkt_size, data_aux); // EN ESTE CASO SUMAMOS 16 BYTES DE DESTINATION OPTIONS

 //               ret= nfq_set_verdict(qh, id, NF_ACCEPT, 130,data_aux);
 //               fprintf(stderr,"NFQ_SET_VERDICT MODIFIED PAYLOAD VALUE LOCAL OUT: %d \n",ret);
 //               fputc('\n', stdout);
                return ret;
                break;

                case NF_IP6_POST_ROUTING:
 //               ret = nfq_set_verdict(qh, id, NF_ACCEPT, pkt_size + add_post_routing, data_aux);

                                   ret = nfq_set_verdict(qh, id, NF_ACCEPT, pkt_size , data_aux);


 //               fprintf(stderr,"NFQ_SET_VERDICT MODIFIED PAYLOAD VALUE POST ROUTING: %d \n",ret);
                break;

            case NF_IP6_PRE_ROUTING:
//                ret = nfq_set_verdict(qh, id, NF_ACCEPT, pkt_size + add_pre_routing, data_aux);
 //               if (icmp->icmp6_type != ND_NEIGHBOR_SOLICIT && icmp->icmp6_type !=ND_NEIGHBOR_ADVERT)
                if(accept){
                ret = nfq_set_verdict(qh, id, NF_ACCEPT, pkt_size, data_aux);
                }else{
                    ret = nfq_set_verdict(qh, id, NF_DROP, pkt_size, data_aux);
                }

                break;

            default:
            return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);

        }
}




int pkt_hdlr_init(){

    int aux_opt=1;
    struct nfq_q_handle *qh;
    int retval;
    int sock;

    add_local_out = 0;
    add_post_routing = 0;
    add_pre_routing = 0;

    h = nfq_open();
	if (!h) {
		fprintf(stderr, "error during nfq_open()\n");
		return -1;
	}

	fprintf(stderr,"unbinding existing nf_queue handler for AF_INET (if any)\n");
	if (nfq_unbind_pf(h, AF_INET6) < 0) {
		fprintf(stderr, "error during nfq_unbind_pf()\n");
		return -1;
	}

	fprintf(stderr,"binding nfnetlink_queue as nf_queue handler for AF_INET\n");
	if (nfq_bind_pf(h, AF_INET6) < 0) {
		fprintf(stderr, "error during nfq_bind_pf()\n");
		return -1;
	}


    // QUEUE HANDLING

    fprintf(stderr,"binding this socket to queue '0'\n");
	qh = nfq_create_queue(h,  0, &cb, NULL);
	if (!qh) {
		fprintf(stderr, "error during nfq_create_queue()\n");
		return -1;
	}

        // 60 header IP + 64 ICMP

	fprintf(stderr,"setting copy_packet mode\n");
	if (nfq_set_mode(qh, NFQNL_COPY_PACKET, 0xffff) < 0) {
		fprintf(stderr, "can't set packet_copy mode\n");
		return -1;
	}

        sock = nfq_fd(h);

        retval = attach_callback_func(sock, handle_packet, NULL);

	if (retval < 0) {
	    perror("ANTop Packet Handler: register input handler failed ");
	    return -1;
	}

        return sock;

}




