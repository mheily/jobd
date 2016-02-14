/*
 * Copyright (c) 2015 Mark Heily <mark@heily.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef SOCKET_H_
#define SOCKET_H_

struct job;


/** An element in the Sockets dictionary */
struct job_manifest_socket {
	SLIST_ENTRY(job_manifest_socket) entry;

	/** The socket descriptor */
	int sd;

	/** The port number, based on the value of <sock_service_name> */
	int port;

	/* Key/value pairs from the manifest: */
	char *	label;				/* the unique key in the Sockets dictionary for this structure */
	int		sock_type;			/*  default: SOCK_STREAM values: SOCK_STREAM, SOCK_DGRAM, SOCK_SEQPACKET */
	bool	sock_passive;		/* default: true */
	char *	sock_node_name;		/* optional */
	char *	sock_service_name;	/* optional */
	int		sock_family;		/* optional; default: PF_INET ; allowed: PF_UNIX, PF_INET, PF_INET6 */
	char	sock_protocol;		/* FIXME: duplicate of sock_type?? default: 'TCP'; */
	char *	sock_path_name;		/* optional; only for PF_UNIX */
	char *	secure_socket_with_key;		/* optional; only for PF_UNIX */
	int		sock_path_mode;		/* optional; only for PF_UNIX */
	/* Not implemented: Bonjour */
	char *	multicast_group;	/* optional */
};

void setup_socket_activation();
int socket_activation_handler();

struct job_manifest_socket * job_manifest_socket_new();
void job_manifest_socket_free(struct job_manifest_socket *);
int job_manifest_socket_open(struct job *, struct job_manifest_socket *);
int job_manifest_socket_close(struct job_manifest_socket *);
int job_manifest_socket_get_port(struct job_manifest_socket *);

/**
 * Prepare the socket to be exported to a child process when the associated
 * job is run.
 */
int job_manifest_socket_export(struct job_manifest_socket *, cvec_t, size_t);

#endif /* SOCKET_H_ */
