/*
 *  Copyright (C) 1999 AT&T Laboratories Cambridge.  All Rights Reserved.
 *
 *  This is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 *  USA.
 *
 * 03-05-2002 tim@tjansen.de: removed Xt event processing for krdc
 */

/*
 * sockets.c - functions to deal with sockets.
 */

#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <assert.h>
#include "vncviewer.h"

void PrintInHex(char *buf, int len);

Bool errorMessageOnReadFailure = True;

#define BUF_SIZE 8192
static char buf[BUF_SIZE];
static char *bufoutptr = buf;
static unsigned int buffered = 0;

/* Wait duration of select in seconds */
#define SELECT_PERIOD 3


/*
 * ReadFromRFBServer is called whenever we want to read some data from the RFB
 * server.  
 */
Bool
ReadFromRFBServer(char *out, unsigned int n)
{
  fd_set fds;
  int e;
  struct timeval tx;

  if (isQuitFlagSet())
    return False;

  if (n <= buffered) {
    memcpy(out, bufoutptr, n);
    bufoutptr += n;
    buffered -= n;
    return True;
  }

  memcpy(out, bufoutptr, buffered);

  out += buffered;
  n -= buffered;

  bufoutptr = buf;
  buffered = 0;

  if (n <= BUF_SIZE) {

    while (buffered < n) {
      int i;
      if (isQuitFlagSet())
	return False;
      i = read(rfbsock, buf + buffered, BUF_SIZE - buffered);

      if (i <= 0) {
	if (i < 0) {
	  if (errno == EWOULDBLOCK || errno == EAGAIN) {
	    FD_ZERO(&fds);
	    FD_SET(rfbsock,&fds);

	    tx.tv_sec = SELECT_PERIOD;
	    tx.tv_usec = 0;
	    if ((e=select(rfbsock+1, &fds, NULL, &fds, &tx)) < 0) {
	      perror("krdc: select read");
	      return False;
	    }
	    i = 0;
	  } else {
	    perror("krdc: read");
	    return False;
	  }
	} else { 
	  fprintf(stderr,"VNC server closed connection\n");
	  return False;
	}
      }
      buffered += i;
    }

    memcpy(out, bufoutptr, n);
    bufoutptr += n;
    buffered -= n;
    return isQuitFlagSet() ? False : True;

  } else {

    while (n > 0) {
      int i;
      if (isQuitFlagSet())
	return False;
      i = read(rfbsock, out, n);
      if (i <= 0) {
	if (i < 0) {
	  if (errno == EWOULDBLOCK || errno == EAGAIN) {
	    FD_ZERO(&fds);
	    FD_SET(rfbsock,&fds);

	    tx.tv_sec = SELECT_PERIOD;
	    tx.tv_usec = 0;
	    if ((e=select(rfbsock+1, &fds, NULL, &fds, &tx)) < 0) {
	      perror("krdc: select");
	      return False;
	    }	    
	    i = 0;
	  } else {
	    perror("krdc: read");
	    return False;
	  }
	} else { 
	  fprintf(stderr,"VNC server closed connection\n");
	  return False;
	}
      }
      out += i;
      n -= i;
    }

    return isQuitFlagSet() ? False : True;
  }
}


/*
 * Write an exact number of bytes, and don't return until you've sent them.
 * Note: this should only be called by the WriterThread
 */

Bool
WriteExact(int sock, const char *_buf, int n)
{
  fd_set fds;
  int i = 0;
  int j;
  int e;
  struct timeval tx;

  while (i < n) {
    if (isQuitFlagSet())
      return False;
    j = write(sock, _buf + i, (n - i));
    if (j <= 0) {
      if (j < 0) {
	if (errno == EWOULDBLOCK || errno == EAGAIN) {
	  FD_ZERO(&fds);
	  FD_SET(rfbsock,&fds);

	  tx.tv_sec = SELECT_PERIOD;
	  tx.tv_usec = 0;
	  if ((e=select(rfbsock+1, NULL, &fds, NULL, &tx)) < 0) {
	    perror("krdc: select write");
	    return False;
	  }
	  j = 0;
	} else {
	  perror("krdc: write");
	  return False;
	}
      } else {
	fprintf(stderr,"write failed\n");
	return False;
      }
    }
    i += j;
  }
  return True;
}


/*
 * ConnectToTcpAddr connects to the given TCP port.
 */

int
ConnectToTcpAddr(unsigned int host, int port)
{
  int sock;
  struct sockaddr_in addr;
  int one = 1;

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = host;

  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("krdc: ConnectToTcpAddr: socket");
    return -(int)INIT_CONNECTION_FAILED;
  }

  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("krdc: ConnectToTcpAddr: connect");
    close(sock);
    return -(int)INIT_NO_SERVER;
  }

  if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		 (char *)&one, sizeof(one)) < 0) {
    perror("krdc: ConnectToTcpAddr: setsockopt");
    close(sock);
    return -(int)INIT_CONNECTION_FAILED;
  }

  if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
    perror(": AcceptTcpConnection: fcntl");
    close(sock);
    return -(int)INIT_CONNECTION_FAILED;
  }  

  return sock;
}


/*
 * StringToIPAddr - convert a host string to an IP address.
 */

Bool
StringToIPAddr(const char *str, unsigned int *addr)
{
  struct hostent *hp;

  if (strcmp(str,"") == 0) {
    *addr = 0; /* local */
    return True;
  }

  *addr = inet_addr(str);

  if (*addr != -1)
    return True;

  hp = gethostbyname(str);

  if (hp) {
    *addr = *(unsigned int *)hp->h_addr;
    return True;
  }

  return False;
}


/*
 * Print out the contents of a packet for debugging.
 */

void
PrintInHex(char *_buf, int len)
{
  int i, j;
  char c, str[17];

  str[16] = 0;

  fprintf(stderr,"ReadExact: ");

  for (i = 0; i < len; i++)
    {
      if ((i % 16 == 0) && (i != 0)) {
	fprintf(stderr,"           ");
      }
      c = _buf[i];
      str[i % 16] = (((c > 31) && (c < 127)) ? c : '.');
      fprintf(stderr,"%02x ",(unsigned char)c);
      if ((i % 4) == 3)
	fprintf(stderr," ");
      if ((i % 16) == 15)
	{
	  fprintf(stderr,"%s\n",str);
	}
    }
  if ((i % 16) != 0)
    {
      for (j = i % 16; j < 16; j++)
	{
	  fprintf(stderr,"   ");
	  if ((j % 4) == 3) fprintf(stderr," ");
	}
      str[i % 16] = 0;
      fprintf(stderr,"%s\n",str);
    }

  fflush(stderr);
}

void freeSocketsResources() {
  close(rfbsock);

  errorMessageOnReadFailure = True;
  bufoutptr = buf;
  buffered = 0;
}

