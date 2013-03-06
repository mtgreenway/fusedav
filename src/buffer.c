/* buffer.c
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
#include <errno.h>

#include "session.h"
#include "buffer.h"
#include "log.h"

#include <string.h>


/**
    The following is a mostly copied and pasted from the libneon code
    bypassing the need to write to a file when fetching a file to read
    using WebDav.
**/

int ne_read_response_to_buf(ne_request *req, char *buf, ssize_t *bytes_read)
{
    ssize_t len;
    *bytes_read = 0;

    while ((len = ne_read_response_block(req, buf,
                        sizeof buf)) > 0) {
        buf += len;
        (*bytes_read) += len;
    }

    return len == 0 ? NE_OK : NE_ERROR;
}


static int dispatch_to_buffer(ne_session *sess, ne_request *req, char *buf, const char *range, ssize_t *bytes_read)
{
    const ne_status *const st = ne_get_status(req);
    int ret;
    size_t rlen;

    /* length of bytespec after "bytes=" */
    rlen = range ? strlen(range + 6) : 0;

    do {
        const char *value;

        ret = ne_begin_request(req);
        if (ret != NE_OK) break;

        value = ne_get_response_header(req, "Content-Range");

        /* For a 206 response, check that a Content-Range header is
         * given which matches the Range request header. */
        if (range && st->code == 206
            && (value == NULL || strncmp(value, "bytes ", 6) != 0
                || strncmp(range + 6, value + 6, rlen)
                || (range[5 + rlen] != '-' && value[6 + rlen] != '/'))) {
            ne_set_error(sess, "Response did not include requested range");
            return NE_ERROR;
        }

        if ((range && st->code == 206) || (!range && st->klass == 2)) {
            ret = ne_read_response_to_buf(req, buf, bytes_read);
        } else {
            ret = ne_discard_response(req);
        }

        if (ret == NE_OK) ret = ne_end_request(req);
    } while (ret == NE_RETRY);

    return ret;
}


static int get_range_common(ne_session *sess, const char *req_uri,
                            const char *brange, char *buf, ssize_t *bytes_read)

{
    ne_request *req = ne_request_create(sess, "GET", req_uri);
    const ne_status *status;
    int ret;

    ne_add_request_header(req, "Range", brange);
    ne_add_request_header(req, "Accept-Ranges", "bytes");

    ret = dispatch_to_buffer(sess, req, buf, brange, bytes_read);

    status = ne_get_status(req);

    if (ret == NE_OK && status->code == 416) {
        /* connection is terminated too early with Apache/1.3, so we check
         * this even if ret == NE_ERROR... */
        ne_set_error(sess, "Range is not satisfiable");
        ret = NE_ERROR;
    }
    else if (ret == NE_OK) {
        if (status->klass == 2 && status->code != 206) {
            ne_set_error(sess, "Resource does not support ranged GET requests");
            ret = NE_ERROR;
        }
        else if (status->klass != 2) {
            ret = NE_ERROR;
        }
    }

    ne_request_destroy(req);

    return ret;
}

int buf_ne_get_range(ne_session *sess, const char *req_uri,
                 ne_content_range *range, char *buf, ssize_t *bytes_read)
{
    char brange[64];

    ne_snprintf(brange, sizeof brange,
    "bytes=%" FMT_NE_OFF_T "-%" FMT_NE_OFF_T, range->start,
    range->end);

    return get_range_common(sess, req_uri, brange, buf, bytes_read);
}

//static int buffer_load(struct file_info *fi, off_t l, char *buf) {
//
//    ne_content_range range;
//    ne_session *session;
//    ssize_t bytes_read;
//
//    assert(fi);
//
//    if (!(session = session_get(1))) {
//        errno = EIO;
//        return -1;
//    }
//
//    if (l > fi->server_length)
//        l = fi->server_length;
//
//    if (l <= fi->present)
//        return 0;
//
//    range.start = fi->present;
//    range.end = l-1;
//    range.total = 0;
//
//
//    if (buf_ne_get_range(session, fi->filename, &range, buf, &bytes_read) != NE_OK) {
//        fprintf(stderr, "GET failed: %s\n", ne_get_error(session));
//        errno = ENOENT;
//        return -1;
//    }
//
//    return bytes_read;
//}

//int file_buffer_read(void *f, char *buf, size_t size, off_t offset) {
//    struct file_info *fi = f;
//    ssize_t r = -1;
//
//    assert(fi && buf && size);
//    pthread_mutex_lock(&fi->mutex);
//
//    /* if read outside of buffer do a non buffered request */
//
//    /* This works because we assume that the first request will
//    be at offset 0 and this condition will fail starting a
//    buffered request.  However if the first file read does
//    not start at 0 we will do no buffering.
//    */
//    if (( offset > fi->offset + fi->size) || ( offset < fi->offset )) {
//        fi->present = offset;
//        if (fi->present > fi->server_length) {
//            fi->present = fi->server_length;
//        }
//
//        r = buffer_load(fi, offset + size, buf);
//    }
//
//    /* the amount buffered is than what we want to fetch */
//    else if (((fi->offset + fi->size) - offset) < size || offset < fi->offset) {
//
//        int buffered = (fi->offset + fi->size) - offset;
//        size_t need_to_fetch;
//
//        fi->present = offset + buffered;
//        if (fi->present > fi->server_length)
//            fi->present = fi->server_length;
//
//        /* copy the remaining buffer to the read buffer */
//        need_to_fetch = size - buffered;
//        memcpy(buf, fi->buf + (offset - fi->offset), buffered);
//
//        /* now the buffer can be filled with new data */
//        //if ((r = multi_buf_load(fi, offset + buffered  +(BUFFER_SIZE), fi->buf)) < 0)
//        if ((r = buffer_load(fi, offset + buffered  +(BUFFER_SIZE), fi->buf)) < 0)
//        {
//            goto finish;
//        }
//
//        fi->offset = offset + buffered;
//
//        fi->size = r;
//
//        if (r < need_to_fetch) {
//            need_to_fetch = r;
//        }
//        else {
//            r = size;
//        }
//
//        memcpy(buf + buffered, fi->buf, need_to_fetch);
//
//    }
//    else {
//        memcpy(buf, fi->buf + (offset - fi->offset), size);
//        r = size;
//    }
//
//finish:
//
//    pthread_mutex_unlock(&fi->mutex);
//
//    return r;
//}
