/*
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 * All Rights Reserved.
 */

#pragma ident	"@(#)parsetest.c	1.1	96/11/01 SMI"

/*
 * /src/NTP/REPOSITORY/v3/parse/util/parsetest.c,v 3.16 1995/06/18 12:11:00 kardel Exp
 *
 * parsetest.c,v 3.16 1995/06/18 12:11:00 kardel Exp
 *
 * Copyright (c) 1989,1990,1991,1992,1993,1994
 * Frank Kardel Friedrich-Alexander Universitaet Erlangen-Nuernberg
 *                                    
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Log: parsetest.c,v $
 * Revision 3.16  1995/06/18  12:11:00  kardel
 * removed dispersion calulation from parse subsystem
 *
 * Revision 3.15  1994/05/30  10:20:12  kardel
 * LONG cleanup
 *
 * Revision 3.14  1994/05/12  12:49:27  kardel
 * printf fmt/arg cleanup
 *
 * Revision 3.14  1994/05/11  09:25:43  kardel
 * 3.3r + printf fmt/arg fixes
 *
 * Revision 3.13  1994/02/20  13:04:46  kardel
 * parse add/delete second support
 *
 * Revision 3.12  1994/02/02  17:45:51  kardel
 * rcs ids fixed
 *
 */

#ifndef STREAM
ONLY STREAM OPERATION SUPPORTED
#endif

#define PARSESTREAM		/* there is no other choice - TEST HACK */

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/errno.h>
#include <fcntl.h>

#define P(X) ()

#include "ntp_fp.h"
#ifdef USE_PROTOTYPES
#include "ntp_stdlib.h"
#endif
#include "parse.h"

static char *strstatus(buffer, state)
  char *buffer;
  u_long state;
{
  static struct bits
    {
      u_long bit;
      char  *name;
    } flagstrings[] =
    {
      { PARSEB_ANNOUNCE, "DST SWITCH WARNING" },
      { PARSEB_POWERUP,  "NOT SYNCHRONIZED" },
      { PARSEB_NOSYNC,   "TIME CODE NOT CONFIRMED" },
      { PARSEB_DST,      "DST" },
      { PARSEB_UTC,      "UTC DISPLAY" },
      { PARSEB_LEAPADD,  "LEAP ADDITION WARNING" },
      { PARSEB_LEAPDEL,  "LEAP DELETION WARNING" },
      { PARSEB_LEAPSECOND, "LEAP SECOND" },
      { PARSEB_ALTERNATE,"ALTERNATE ANTENNA" },
      { PARSEB_TIMECODE, "TIME CODE" },
      { PARSEB_PPS,      "PPS" },
      { PARSEB_POSITION, "POSITION" },
      { 0 }
    };

  static struct sbits
    {
      u_long bit;
      char         *name;
    } sflagstrings[] =
    {
      { PARSEB_S_LEAP,     "LEAP INDICATION" },
      { PARSEB_S_PPS,      "PPS SIGNAL" },
      { PARSEB_S_ANTENNA,  "ANTENNA" },
      { PARSEB_S_POSITION, "POSITION" },
      { 0 }
    };
  int i;

  *buffer = '\0';
  
  i = 0;
  while (flagstrings[i].bit)
    {
      if (flagstrings[i].bit & state)
	{
	  if (buffer[0])
	    strcat(buffer, "; ");
	  strcat(buffer, flagstrings[i].name);
	}
      i++;
    }
  
  if (state & (PARSEB_S_LEAP|PARSEB_S_ANTENNA|PARSEB_S_PPS|PARSEB_S_POSITION))
    {
      register char *s, *t;
      
      if (buffer[0])
	strcat(buffer, "; ");

      strcat(buffer, "(");

      t = s = buffer + strlen(buffer);
      
      i = 0;
      while (sflagstrings[i].bit)
	{
	  if (sflagstrings[i].bit & state)
	    {
	      if (t != s)
		{
		  strcpy(t, "; ");
		  t += 2;
		}
	      
	      strcpy(t, sflagstrings[i].name);
	      t += strlen(t);
	    }
	  i++;
	}
      strcpy(t, ")");
    }
  return buffer;
}

/*--------------------------------------------------
 * convert a status flag field to a string
 */
static char *parsestatus(state, buffer)
  u_long state;
  char *buffer;
{
  static struct bits
    {
      u_long bit;
      char  *name;
    } flagstrings[] =
    {
      { CVT_OK,      "CONVERSION SUCCESSFUL" },
      { CVT_NONE,    "NO CONVERSION" },
      { CVT_FAIL,    "CONVERSION FAILED" },
      { CVT_BADFMT,  "ILLEGAL FORMAT" },
      { CVT_BADDATE, "DATE ILLEGAL" },
      { CVT_BADTIME, "TIME ILLEGAL" },
      { 0 }
    };
  int i;

  *buffer = '\0';
  
  i = 0;
  while (flagstrings[i].bit)
    {
      if (flagstrings[i].bit & state)
	{
	  if (buffer[0])
	    strcat(buffer, "; ");
	  strcat(buffer, flagstrings[i].name);
	}
      i++;
    }
  
  return buffer;
}

int
main(argc, argv)
  int argc;
  char **argv;
{
  if (argc != 2)
    {
      fprintf(stderr,"usage: %s <parse-device>\n", argv[0]);
      exit(1);
    }
  else
    {
      int fd;
      
      fd = open(argv[1], O_RDWR);
      if (fd == -1)
	{
	  perror(argv[1]);
	  exit(1);
	}
      else
	{
	  parsetime_t parsetime;
	  
	  printf("parsetest.c,v 3.16 1995/06/18 12:11:00 kardel Exp\n");
	  
	  while (ioctl(fd, I_POP, 0) == 0)
	    ;

	  if (ioctl(fd, I_PUSH, "parse") == -1)
	    {
	      perror("ioctl(I_PUSH,\"parse\")");
	      exit(1);
	    }

	  while (read(fd, &parsetime, sizeof(parsetime)) == sizeof(parsetime))
	    {
	      char tmp[200], tmp1[200], tmp2[60];

	      strncpy(tmp, asctime(localtime(&parsetime.parse_time.tv.tv_sec)), 30);
              strncpy(tmp1,asctime(localtime(&parsetime.parse_stime.tv.tv_sec)), 30);
              strncpy(tmp2,asctime(localtime(&parsetime.parse_ptime.tv.tv_sec)), 30);
	      tmp[24]  = '\0';
	      tmp1[24] = '\0';
	      tmp2[24] = '\0';

	      printf("%s (+%06ldus) %s PPS: %s (+%06ldus), ", tmp1, (long int)parsetime.parse_stime.tv.tv_usec, tmp, tmp2,
		     (long int)parsetime.parse_ptime.tv.tv_usec);

	      strstatus(tmp, parsetime.parse_state);
	      printf("state: 0x%lx (%s) error: %ldus, Status: 0x%lx (%s)\n",
		     (unsigned long)parsetime.parse_state,
		     tmp,
		     (long)parsetime.parse_usecerror,
		     (unsigned long)parsetime.parse_status,
		     parsestatus(parsetime.parse_status, tmp1));
	    }
	  
	  close(fd);
	}
    }
  return 0;
}
