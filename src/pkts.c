/* This file is part of Netsukuku
 * (c) Copyright 2004 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Public License as published 
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This source code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * Please refer to the GNU Public License for more details.
 *
 * You should have received a copy of the GNU Public License along with
 * this source code; if not, write to:
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <string.h>
#include "pkts.h"
#include "xmalloc.h"
#include "log.h"

/*Handy functions to build the PACKET*/
void pkt_addfrom(PACKET *pkt, inet_prefix *from)
{
	memcpy(pkt->from, from, sizeof(inet_prefix));
}

void pkt_addto(PACKET *pkt, inet_prefix *to)
{
	memcpy(pkt->to, to, sizeof(inet_prefix));
}

void pkt_addsk(PACKET *pkt, int sk, int sk_type)
{
	pkt->sk=sk;
	pkt->sk_type=sk_type;
}

void pkt_addport(PACKET *pkt, u_short port)
{
	pkt->port=port;
}

void pkt_addflags(PACKET *pkt, int flags)
{
	pkt->flags=flags;
}

void pkt_addhdr(PACKET *pkt, struct pkt_hdr *hdr)
{
	memcpy(pkt->hdr, hdr, sizeof(struct pkt_hdr));
}

void pkt_addmsg(PACKET *pkt, char *msg)
{
	pkt->msg=msg;
}
/*End of handy stupid functions (^_+)*/

void pkt_free(PACKET *pkt, int close_socket)
{
	if(pkt->sk && close_socket) {
		close(pkt->sk);
		pkt->sk=0;
	}
	
	if(pkt->msg) {
		xfree(pkt->msg);
		pkt->msg=0;
	}
	memset(pkt, '\0', sizeof(PACKET));
}

char *pkt_pack(PACKET *pkt)
{
	char *buf;
	
	buf=(char *)xmalloc(PACKET_SZ(pkt->hdr.sz));
	memcpy(buf, &pkt->hdr, sizeof(struct pkt_hdr));
	if(!pkt->hdr.sz)
		return buf;
	
	memcpy(buf+sizeof(struct pkt_hdr), &pkt->msg, pkt->hdr.sz);
	return buf;
}

PACKET *pkt_unpack(char *pkt)
{
	PACKET *pk;

	pk=(PAKET *)xmalloc(sizeof(PACKET));
	
	/*Now, we extract the pkt_hdr...*/
	memcpy(pk->hdr, pkt, sizeof(struct pkt_hdr));
	/*and verify it...*/
	if(pkt_verify_hdr(*pk)) {
		error("Error while unpacking the PACKET. Malformed header");
		return 0;
	}
	
	pk->msg=pkt+sizeof(struct pkt_hdr);
	return pk;
}
	
int pkt_verify_hdr(PACKET pkt)
{
	if(strncmp(pkt.hdr.ntk_id, NETSUKUKU_ID, 3))
		return 1;
	if(pkt.hdr.sz > MAXMSGSZ)
		return 1;
	
	return 0;
}

ssize_t pkt_send(PACKET *pkt)
{
	char *buf;

	buf=pkt_pack(pkt);

	if(pkt->sk_type==SKT_UDP || pkt->sk_type==SKT_BCAST) {
		struct sockaddr to;
		socklen_t tolen;

		if(inet_to_sockaddr(&pkt->to, pkt->port, &to, &tolen)) {
			error("Cannot pkt_send(): %d Family not supported", pkt->to.family);
			return -1;
		}
		return inet_sendto(pkt->sk, buf, PACKET_SZ(pkt->hdr.sz), pkt->flags, &to, tolen);
	} else if(pkt->sk_type==SKT_TCP)
		return inet_send(pkt->sk, buf, PACKET_SZ(pkt->hdr.sz), pkt->flags);
	else
		fatal("Unkown socket_type. Something's very wrong!! Be aware");

	/*not reached*/
	return -1;
}

ssize_t pkt_recv(PACKET *pkt)
{
	ssize_t err=-1;
	char *buf;

	if(pkt->sk_type==SKT_UDP || pkt->sk_type==SKT_BCAST) {
		struct sockaddr from;
		socklen_t fromlen;
		
		/*we get the hdr...*/
		err=inet_recvfrom(pkt->sk, pkt->hdr, sizeof(struct pkt_hdr), pkt->flags, &from, &fromlen);
		if(err != sizeof(struct pkt_hdr)) {
			error("inet_recvfrom() of the hdr aborted!");
			return -1;
		}
		/*...and verify it*/
		if(pkt_verify_hdr(*pkt)) {
			error("Error while unpacking the PACKET. Malformed header");
			return -1;
		}

		/*let's get the body*/
		buf=xmalloc(pkt->hdr.sz);
		err=inet_recvfrom(pkt->sk, buf, pkt->hdr.sz, pkt->flags, &from, &fromlen);
		if(err != pkt->hdr.sz) {
			error("Cannot inet_recvfrom() the pkt's body");
			return -1;
		}
		
		/*Now we store in the PACKET what we got*/
		pkt->msg=buf;
		if(sockaddr_to_inet(&from, pkt->from, pkt->port)) {
			error("Cannot pkt_recv(): %d Family not supported", from.family);
			return -1;
		}
	} else if(pkt->sk_type==SKT_TCP) {
		/*we get the hdr...*/
		err=inet_recv(pkt->sk, pkt->hdr, sizeof(struct pkt_hdr), pkt->flags);
		if(err != sizeof(struct pkt_hdr)) {
			error("inet_recv() of the hdr aborted!");
			return -1;
		}
		/*...and verify it*/
		if(pkt_verify_hdr(*pkt)) {
			error("Error while unpacking the PACKET. Malformed header");
			return -1;
		}

		/*let's get the body*/
		buf=xmalloc(pkt->hdr.sz);
		err=inet_recv(pkt->sk, buf, pkt->hdr.sz, pkt->flags);
		if(err != pkt->hdr.sz) {
			error("Cannot inet_recv() the pkt's body");
			return -1;
		}
		
		/*Now we store in the PACKET what we got*/
		pkt->msg=buf;
		if(sockaddr_to_inet(&from, pkt->from, pkt->port)) {
			error("Cannot pkt_recv(): %d Family not supported", from.family);
			return -1;
		}
	} else
		fatal("Unkown socket_type. Something's very wrong!! Be aware");
	
	return err;
}

void pkt_fill_hdr(struct pkt_hdr *hdr, int id, u_char op, size_t sz)
{
	hdr->ntk_id[0]='n';
	hdr->ntk_id[1]='t';
	hdr->ntk_id[2]='k';

	if(!id)
		srandom(time(NULL));
		id=random();
		
	hdr->id=id;
	hdr->op=op;
	hdr->sz=sz;
}

/* send_rq: This functions send a `rq' request, with an id set to `rq_id', to `pkt->to'. 
 * If `pkt->hdr.sz` is > 0 it includes the `pkt->msg' in the packet otherwise it will be NULL. 
 * If `rpkt' is not null it will receive and store the reply pkt in `rpkt'.
 * If `check_ack' is set send_rq, send_rq attempts to receive the reply pkt and it checks its
 * ACK and its id; if the test fails it gives an appropriate error message.
 * If `re'  is not null send_rq confronts the REPLY_ID of the received reply pkt 
 * with `re'; if the test fails it gives an appropriate error message.
 * If an error occurr send_rq returns -1 otherwise it returns 0.
 */
int send_rq(PACKET *pkt, int flags, u_char rq, u_int rq_id, u_char re, int check_ack, PACKET *rpkt)
{
	PACKET pkt;
	u_short port;
	char *ntop, *rq_str, *re_str;
	ssize_t err;
	int ret=0;

	rq_str=rq_to_str(rq);
	if(rq_verify(rq)) {
		error("\"%s\" request is not valid!", rq_str);
		return -1;
	}
	if(re && re_verify(re)) {
		error("\"%s\" reply is not valid!", re_str);
		return -1;
	}

	if(rpkt)
		memset(&rpkt, '\0', sizeof(PACKET));

	ntop=inet_to_str(&pkt->to);
	debug(DBG_NOISE, "Sending the %s request to %s", rq_str, ntop);

	/* * * the request building process * * */
	pkt_fill_hdr(&pkt->hdr, rq_id, rq, pkt->hdr.sz);
	if(!pkt->hdr.sz)
		pkt->msg=0;
	
	if(pkt->sk_type==SKT_TCP) {
		pkt_addport(pkt, ntk_tcp_port);
		if(rpkt)
			pkt_addport(&rpkt, ntk_tcp_port);
	} else if(pkt->sk_type==SKT_UDP || pkt->sk_type==SKT_BCAST)
		pkt_addport(pkt, ntk_udp_port);

	pkt_addflags(pkt, flags);

	if(!pkt->sk) {
		if(!pkt->to.family || !pkt->to.len) {
			error("pkt->to isn't set. I can't create the new connection");
			ret=-1;
			goto finish;
		}
		
		if(pkt->sk_type==SKT_TCP)
			pkt->sk=new_tcp_conn(pkt->to, pkt->port);
		else if(pkt->sk_type==SKT_UDP)
			pkt->sk=new_udp_conn(pkt->to, pkt->port);
		else if(pkt->sk_type==SKT_BCAST)
			pkt->sk=new_bcast_conn(pkt->to, pkt->port);
		else
			fatal("Unkown socket_type. Something's very wrong!! Be aware");

		if(pkt->sk==-1) {
			error("Couldn't connect to %s to launch the %s request", ntop, rq_str);
			ret=-1; 
			goto finish;
		}
	}
	if(rpkt)
		pkt_addsk(rpkt, pkt->sk, pkt->sk_type);
	
	/*Let's send the request*/
	err=pkt_send(pkt);
	if(err==-1) {
		error("Cannot send the %s request to %s. Skipping...", rq_str, ntop);
		ret=-1; 
		goto finish;
	}

	/* * * the reply * * */
	if(rpkt) {
		debug(DBG_NOISE, "Trying to receive the reply for the %s request with %d id", rq_str, pkt->hdr.id);
		if(pkt->sk_type==SKT_UDP)
			memcpy(&rpkt->from, &pkt->to, sizeof(inet_prefix));

		if((err=pkt_recv(rpkt))==-1) {
			error("Error while receving the reply for the %s request from %s.", rq_str, ntop);
			ret=-1; 
			goto finish;
		}

		if(rpkt->hdr.op==ACK_NEGATIVE && check_ack) {
			int err_ack;
			memcpy(&err_ack, rpkt->buf, rpkt->hdr.sz);
			error("%s failed. The node %s replied: %s", rq_str, ntop, rq_strerror(err_ack));
			ret=-1; 
			goto finish;
		} else if(re && rpkt->hdr.op!=re && check_ack) {
			error("The node %s replied %s but we asked %s!", ntop, re_to_str(rpkt->hdr.op), re_str);
			ret=-1;
			goto finish;
		}

		if(check_ack && rpkt->hdr.id != pkt->hdr.id) {
			error("The id (%d) of the reply (%s) doesn't match the id of our request (%d)", rpkt->hdr.id, 
					re_str, pkt->hdr.id);
			ret=-1;
			goto finish;
		}
	}

finish:
	xfree(ntop);
	return ret;
}

/* pkt_err: Sends back to "pkt.from" an error pkt, with ACK_NEGATIVE, containing the "err" code.
 */
int pkt_err(PACKET pkt, int err)
{
	char *msg;
	int err;
	
	memcpy(&pkt.to, &pkt.from, sizeof(inet_prefix));
	pkt_fill_hdr(&pkt.hdr, pkt.hdr.id, ACK_NEGATIVE, sizeof(int));
	
	msg=xmalloc(sizeof(int));
	memcpy(msg, err, sizeof(int));
	
	err=pkt_send(&pkt);
	pkt_free(&pkt, 0);
	return err;
}

	
int pkt_exec(PACKET pkt)
{
	char *ntop;
	int err=0;

	if((err=add_rq(pkt.hdr.type, &accept_tbl[accept_idx].rq_tbl))) {
		ntop=inet_to_str(&pkt.from);
		error("From %s: Cannot process the %s request: %s", ntop, rq_to_str(pkt.hdr.type), rq_strerror(err));
		xfree(ntop);
		pkt_err(pkt, err);
		return -1;
	}

	switch(pkt.hdr.type) 
	{
		case ECHO_ME:
			err=radard(pkt);
			break;
		case ECHO_REPLY:
			err=radar_recv_reply(pkt);
			break;
		case GET_FREE_IPS:
			err=put_free_ips(pkt);
			break;
		case GET_INT_MAP:
			err=put_int_map(pkt);
		default:
			/*never reached*/
			break;
	}
	
	return 0;
}