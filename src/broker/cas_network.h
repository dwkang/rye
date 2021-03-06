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
 * cas_network.h -
 */

#ifndef	_CAS_NETWORK_H_
#define	_CAS_NETWORK_H_

#ident "$Id$"

#include <string.h>

#include "cas_net_buf.h"

#define NET_MIN_TIMEOUT		2
#define NET_DEFAULT_TIMEOUT	60

#ifndef MIN
#define MIN(X, Y)	((X) < (Y) ? (X) : (Y))
#endif

#define NET_SIZE_BYTE		((int) sizeof(char))
#define NET_SIZE_SHORT		((int) sizeof(short))
#define NET_SIZE_INT        	((int) sizeof(int))
#define NET_SIZE_DOUBLE     	((int) sizeof(double))
#define NET_SIZE_INT64      	((int) sizeof(INT64))
#define NET_SIZE_BIGINT     	NET_SIZE_INT64
#define NET_SIZE_DATE       	(NET_SIZE_SHORT + NET_SIZE_SHORT + NET_SIZE_SHORT)
#define NET_SIZE_TIME       	(NET_SIZE_SHORT + NET_SIZE_SHORT + NET_SIZE_SHORT)
#define NET_SIZE_OBJECT     	(NET_SIZE_INT + NET_SIZE_SHORT + NET_SIZE_SHORT)
#define NET_SIZE_DATETIME       (NET_SIZE_SHORT * 7)

#define NET_ARG_GET_SIZE(SIZE, ARG)                     \
	do {                                            \
	  int	tmp_i;                                  \
	  memcpy(&tmp_i, (char*) (ARG), NET_SIZE_INT);      \
	  SIZE = ntohl(tmp_i);                          \
	} while (0)

#define NET_ARG_GET_BIGINT(VALUE, ARG)                                     \
        do {                                                               \
          DB_BIGINT   tmp_i;                                               \
          memcpy(&tmp_i, (char*) (ARG) + NET_SIZE_INT, NET_SIZE_BIGINT);           \
          VALUE = ntohi64(tmp_i);                                          \
        } while (0)

#define NET_ARG_GET_INT(VALUE, ARG)                                        \
	do {                                                               \
	  int	tmp_i;                                                     \
	  memcpy(&tmp_i, (char*) (ARG) + NET_SIZE_INT, NET_SIZE_INT);              \
	  VALUE = ntohl(tmp_i);                                            \
	} while (0)

#define NET_ARG_GET_SHORT(VALUE, ARG)                                      \
	do {                                                               \
	  short	tmp_s;				                           \
	  memcpy(&tmp_s, (char*) (ARG) + NET_SIZE_INT, NET_SIZE_SHORT);            \
	  VALUE = ntohs(tmp_s);                                            \
	} while (0)

#define NET_ARG_GET_DOUBLE(VALUE, ARG)                                     \
	do {                                                               \
	  double	tmp_d;                                             \
	  memcpy(&tmp_d, (char*) (ARG) + NET_SIZE_INT, NET_SIZE_DOUBLE);           \
	  VALUE = net_ntohd(tmp_d);                                        \
	} while (0)

#define NET_ARG_GET_CHAR(VALUE, ARG)                                       \
	do {                                                               \
	  VALUE = (*((char*) (ARG) + NET_SIZE_INT));                           \
	} while (0)

#define NET_ARG_GET_STR(VALUE, SIZE, ARG)               \
	do {                                            \
	  int	_size;                                  \
	  char	*cur_p = (char*) ARG;                   \
	  memcpy(&_size, cur_p, NET_SIZE_INT);              \
	  _size = ntohl(_size);                         \
	  cur_p += NET_SIZE_INT;                            \
	  if (_size <= 0) {                             \
	    VALUE = NULL;                               \
	    SIZE = 0;                                 	\
	  } else {                                      \
	    VALUE = ((char*) cur_p);                    \
	    SIZE = _size;                               \
	  }                                             \
	} while (0)

#define NET_ARG_GET_DATE(YEAR, MON, DAY, ARG)   		\
	do {		                                        \
	  char		*cur_p = (char*) ARG;	                \
	  short		tmp_s;			                \
	  int           pos = NET_SIZE_INT;                         \
	  memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);		\
	  YEAR = ntohs(tmp_s);		                        \
	  pos += NET_SIZE_SHORT;                                    \
	  memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);		\
	  MON = ntohs(tmp_s);		                        \
          pos += NET_SIZE_SHORT;                                    \
	  memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);		\
	  DAY = ntohs(tmp_s);		                        \
	} while (0)

#define NET_ARG_GET_TIME(HH, MM, SS, ARG)                            \
	do {                                                         \
	  char		*cur_p = (char*) ARG;	                     \
	  short		tmp_s;			                     \
          int           pos = NET_SIZE_INT + NET_SIZE_SHORT * 3;             \
	  memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);                   \
	  HH = ntohs(tmp_s);                                         \
          pos += NET_SIZE_SHORT;                                         \
	  memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);                   \
	  MM = ntohs(tmp_s);                                         \
          pos += NET_SIZE_SHORT;                                         \
	  memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);                   \
	  SS = ntohs(tmp_s);                                         \
	} while (0)

#define NET_ARG_GET_TIMESTAMP(YR, MON, DAY, HH, MM, SS, ARG)         \
        do {                                                         \
          char          *cur_p = (char*) ARG;                        \
          short         tmp_s;                                       \
          int           pos = NET_SIZE_INT;                              \
          memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);                   \
          YR = ntohs(tmp_s);                                         \
          pos += NET_SIZE_SHORT;                                         \
          memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);                   \
          MON = ntohs(tmp_s);                                        \
          pos += NET_SIZE_SHORT;                                         \
          memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);                   \
          DAY = ntohs(tmp_s);                                        \
          pos += NET_SIZE_SHORT;                                         \
          memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);                   \
          HH = ntohs(tmp_s);                                         \
          pos += NET_SIZE_SHORT;                                         \
          memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);                   \
          MM = ntohs(tmp_s);                                         \
          pos += NET_SIZE_SHORT;                                         \
          memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);                   \
          SS = ntohs(tmp_s);                                         \
        } while (0)

#define NET_ARG_GET_DATETIME(YR, MON, DAY, HH, MM, SS, MS, ARG)      \
        do {                                                         \
          char          *cur_p = (char*) ARG;                        \
          short         tmp_s;                                       \
          int           pos = NET_SIZE_INT;                              \
          memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);                   \
          YR = ntohs(tmp_s);                                         \
          pos += NET_SIZE_SHORT;                                         \
          memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);                   \
          MON = ntohs(tmp_s);                                        \
          pos += NET_SIZE_SHORT;                                         \
          memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);                   \
          DAY = ntohs(tmp_s);                                        \
          pos += NET_SIZE_SHORT;                                         \
          memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);                   \
          HH = ntohs(tmp_s);                                         \
          pos += NET_SIZE_SHORT;                                         \
          memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);                   \
          MM = ntohs(tmp_s);                                         \
          pos += NET_SIZE_SHORT;                                         \
          memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);                   \
          SS = ntohs(tmp_s);                                         \
          pos += NET_SIZE_SHORT;                                         \
          memcpy(&tmp_s, cur_p + pos, NET_SIZE_SHORT);                   \
          MS = ntohs(tmp_s);                                         \
        } while (0)

#define NET_ARG_GET_CACHE_TIME(CT, ARG)             \
        do {                                        \
          char *cur_p = (char*) ARG;                \
          int sec, usec;                            \
          int pos = NET_SIZE_INT;                       \
          memcpy(&sec, cur_p + pos, NET_SIZE_INT);      \
          pos += NET_SIZE_INT;                          \
          memcpy(&usec, cur_p + pos, NET_SIZE_INT);     \
          (CT)->sec = ntohl(sec);                   \
          (CT)->usec = ntohl(usec);                 \
        } while (0)


extern SOCKET net_init_env (char *port_name);

typedef struct
{
  int *msg_body_size_ptr;
  char *info_ptr;
  char buf[MSG_HEADER_SIZE];
} MSG_HEADER;

extern SOCKET net_connect_client (SOCKET srv_sock_fd);

extern int net_read_stream (SOCKET sock_fd, char *buf, int size);
extern int net_write_stream (SOCKET sock_fd, const char *buf, int size);
extern int net_write_int (SOCKET sock_fd, int value);
extern int net_read_int (SOCKET sock_fd, int *value);
extern int net_decode_str (char *msg, int msg_size, char *func_code,
			   void ***ret_argv);

extern void net_timeout_set (int timeout_sec);
extern void init_msg_header (MSG_HEADER * header);
extern int net_read_header (SOCKET sock_fd, MSG_HEADER * header);
extern bool is_net_timed_out (void);

extern void net_write_error (int sock, int cas_info_size,
			     int indicator, int code, char *msg);
#endif /* _CAS_NETWORK_H_ */
