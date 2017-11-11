#ifndef SYN_FLOOD_H
#define SYN_FLOOD_H

#define MAXCHILD 128
#define BUF_SIZE 100

/* ip header */
typedef struct ip_hdr {
	unsigned char       hl;
	unsigned char       tos;
	unsigned short      total_len;
	unsigned short      id;
	unsigned short      frag_and_flags;
	unsigned char       ttl;
	unsigned char       proto;
	unsigned short      checksum;
	unsigned int        src_ip;
	unsigned int        dest_ip;
} ip_hdr;

/* tcp header */
typedef struct tcp_hdr {
 	unsigned short      src_port;
 	unsigned short      dest_port;
	unsigned int        seq;
	unsigned int        ack;
	unsigned char       lenres;
 	unsigned char       flag;
	unsigned short      win;
	unsigned short      sum;
	unsigned short      urp;
} tcp_hdr;

/* fake header */
typedef struct fake_hdr {
	unsigned int 		src_addr;
	unsigned int 		dest_addr;
	char				zero;
	char				protocol;
	unsigned short		len;
} fake_hdr;

#endif	/* SYN_FLOOD_H */