/*
 * Copyright (C) 2008 Search Solution Corporation. All rights reserved by Search Solution.
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */


/*
 * connection_cl.h -
 */

#ifndef _CONNECTION_CL_H_
#define _CONNECTION_CL_H_

#ident "$Id$"

#include "connection_defs.h"
#include "connection_support.h"

extern void css_shutdown_conn (CSS_CONN_ENTRY * conn);
extern CSS_CONN_ENTRY *css_make_conn (SOCKET fd);
extern void css_free_conn (CSS_CONN_ENTRY * conn);

extern CSS_CONN_ENTRY *css_find_exception_conn (void);

extern CSS_CONN_ENTRY *css_connect_to_rye_server (const char *host_name,
						  const char *server_name);
extern CSS_CONN_ENTRY *css_connect_to_master_for_info (const char *host_name,
						       unsigned short *rid);
extern CSS_CONN_ENTRY *css_connect_to_master_timeout (const char *host_name,
						      int timeout,
						      unsigned short *rid);
extern int css_send_request_to_master (CSS_CONN_ENTRY * conn,
				       CSS_MASTER_REQUEST request,
				       int timeout,
				       int num_send_buffers,
				       int num_recv_buffers, ...);

extern bool css_does_master_exist (void);

extern int css_send_close_request (CSS_CONN_ENTRY * conn);

extern int css_test_for_open_conn (CSS_CONN_ENTRY * conn);
extern CSS_CONN_ENTRY *css_find_conn_from_fd (SOCKET fd);
extern unsigned short css_get_request_id (CSS_CONN_ENTRY * conn);

extern int css_common_connect_cl (const char *host_name,
				  CSS_CONN_ENTRY * conn,
				  int connect_type,
				  const char *server_name,
				  const char *packed_name,
				  int packed_name_len, int timeout,
				  unsigned short *rid, bool send_magic);


#endif /* _CONNECTION_CL_H_ */
