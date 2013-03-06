#ifndef foobufferhfoo
#define foobufferhfoo

/* buffer.h
  Dec 9 2012
  Matt Greenway mgreenway@uchicago.edu
  Open Cloud Consortium 
  Laboratory for Advanced Computing at the University of Chicago
*/

/***
  This file is part of fusedav.

  fusedav is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  fusedav is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
  License for more details.
  
  You should have received a copy of the GNU General Public License
  along with fusedav; if not, write to the Free Software Foundation,
  Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***/


#include <sys/types.h>

#include <ne_basic.h>
#include <ne_request.h>
#include <ne_session.h>


#define FMT_NE_OFF_T  "ld"

#define BUFFER_SIZE 128 * 131072

int ne_read_response_to_buf(ne_request *req, char *buf, ssize_t *bytes_read);

static int dispatch_to_buffer(ne_session *sess, ne_request *req, char *buf, const char *range, ssize_t *bytes_read);

static int get_range_common(ne_session *sess, const char *uri,
                            const char *brange, char *buf, ssize_t *bytes_read);

int buf_ne_get_range(ne_session *sess, const char *uri,
                 ne_content_range *range, char *buf, ssize_t *bytes_read);

#endif
