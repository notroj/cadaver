/* 
   cadaver, command-line DAV client
   Copyright (C) 1999-2008, Joe Orton <joe@manyfish.co.uk>, 

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
  
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include <stdio.h>
#include <fcntl.h>

#include <ne_basic.h>
#include <ne_alloc.h>

#include "cadaver.h"
#include "commands.h"
#include "utils.h"
#include "options.h"
#include "i18n.h"

static int run_editor(const char *filename)
{
    char editcmd[BUFSIZ];
    const char *editor;
    struct stat before_st, after_st;
    int ret;

    editor = get_option(opt_editor);
    if (editor == NULL) {
	editor = getenv("EDITOR");
	if (editor == NULL) {
	    editor = "vi";
	}
    }
    snprintf(editcmd, BUFSIZ, "%s %s", editor, filename);
    if (stat(filename, &before_st)) {
	printf(_("edit: Could not stat file: %s\n"), strerror(errno));
	return -1;
    }
    printf("edit: Running editor: `%s'...\n", editcmd);
    ret = system(editcmd);
    if (ret == -1) {
	printf(_("edit: Error executing editor: %s\n"), strerror(errno));
    }
    if (stat(filename, &after_st)) {
        printf(_("edit: Error: Could not examine temporary file: %s\n"),
               strerror(errno));
        return -1;
    }
    if (before_st.st_mtime == after_st.st_mtime) {
	/* File not changed. */
	printf(_("edit: No changes were made.\n"));
	return -1;
    } else {
	printf(_("edit: Changes were made.\n"));
	return 0;
    }	
}

/* Returns true if resource at URI is lockable. */
static int is_lockable(const char *uri_path)
{
    ne_server_capabilities caps = {0};
    ne_uri uri = session.uri; /* shallow copy */

    uri.path = (char *)uri_path;

    if (ne_lockstore_findbyuri(session.locks, &uri) != NULL)
        return 0;

    /* A proper test for "lockability" would be to check the
     * supportedlock property here, but this is sufficient. */
    if (ne_options(session.sess, uri_path, &caps) != NE_OK) {
	return 0;
    }

    return !!caps.dav_class2;
}

#ifndef PATH_MAX
#define PATH_MAX (256)
#endif

/* Pre-send hook to use a conditional PUT. */
static void edit_pre_send(ne_request *req, void *userdata, ne_buffer *header)
{
    char *etag = userdata;

    if (etag) ne_buffer_concat(header, "If-Match: ", etag, "\r\n", NULL);
}

/* Post-headers hook to fetch the etag for GET. */
static void edit_hdrs(ne_request *req, void *userdata, const ne_status *status)
{
    char **etag = userdata;
    const char *val;

    if (status->klass == 2
        && (val = ne_get_response_header(req, "Etag")) != NULL) {
        if (*etag) ne_free(*etag);
        *etag = ne_strclean(ne_strdup(val));
    }
}

/* Post-send hook to produce a descriptive failure for a conditional
 * PUT error. */
static int edit_post_send(ne_request *req, void *userdata, const ne_status *status)
{
    if (status->code == 412) {
        ne_set_error(session.sess, _("Resource was modified since download, "
                                     "upload refused"));
        return NE_ERROR;
    }

    return NE_OK;
}

void execute_edit(const char *native_path)
{
    char *uri_path, *etag = NULL;
    struct ne_lock *lock = NULL;
    char fname[PATH_MAX] = "/tmp/cadaver-edit-XXXXXX";
    const char *pnt;
    int fd;
    int is_checkout, is_checkin, can_lock;
    
    uri_path = uri_resolve_native(native_path);

    /* Don't let them edit a collection, since PUT to a collection is
     * bogus. Really we want to be able to fetch a "DefaultDocument"
     * property, and edit on that instead: IIS does expose such a
     * property. Would be a nice hack to add the support to mod_dav
     * too. */
    if (getrestype(uri_path) == resr_collection) {
	printf(_("You cannot edit a collection resource (%s).\n"),
	       uri_path);
	goto edit_bail;
    }

    can_lock = is_lockable(uri_path);

    /* Give the local temp file the same extension as the remote path,
     * so the editor can have a stab at the content-type. */
    pnt = strrchr(uri_path, '.');
    if (pnt != NULL && strchr(pnt, '/') == NULL) {
	strncat(fname, pnt, PATH_MAX-1);
	fname[PATH_MAX-1] = '\0';
    }

    fd = cad_mkstemp(fname);
    if (fd == -1) {
	printf(_("Could not create temporary file %s:\n%s\n"), fname,
	       strerror(errno));
	goto edit_bail;
    }

    /* Sanity check on the file perms. */
#ifdef HAVE_FCHMOD
    if (fchmod(fd, 0600) == -1) {
#else
    if (chmod(fname, 0600) == -1) {
#endif
	printf(_("Could not set file permissions for %s:\n%s\n"), fname,
	       strerror(errno));
	goto edit_bail;
    }
   
    if (can_lock) {
	lock = ne_lock_create();
	ne_fill_server_uri(session.sess, &lock->uri);
	lock->uri.path = ne_strdup(uri_path);
	lock->owner = getowner();
	out_start_uri(_("Locking"), uri_path);
	if (out_handle(ne_lock(session.sess, lock))) {
	    ne_lockstore_add(session.locks, lock);
	} else {
	    ne_lock_destroy(lock);
	    goto edit_close;
	}
    }

    /* Return 1: Checkin, 2: Checkout, 0: otherwise */
    if ((is_checkin = is_vcr(uri_path)) == 1) {
        execute_checkout(uri_path);
    }

    ne_hook_post_headers(session.sess, edit_hdrs, &etag);
    output(o_download, _("Downloading `%s' to %s"), uri_path, fname);
    /* Don't puke if get fails -- perhaps we are creating a new one? */
    out_result(ne_get(session.sess, uri_path, fd));

    ne_unhook_post_headers(session.sess, edit_hdrs, &etag);

    if (close(fd)) {
	output(o_finish, _("Error writing to temporary file: %s\n"), 
	       strerror(errno));
    } 
    else if (!run_editor(fname)) {
	int upload_okay = 0;

	fd = open(fname, O_RDONLY | OPEN_BINARY_FLAGS);
	if (fd < 0) {
	    output(o_finish, 
		   _("Could not re-open temporary file: %s\n"),
		   strerror(errno));
	} else {
            if (etag) {
                ne_hook_pre_send(session.sess, edit_pre_send, etag);
                ne_hook_post_send(session.sess, edit_post_send, NULL);
            }

            do {
		output(o_upload, _("Uploading changes to `%s'"), uri_path);

		if (out_handle(ne_put(session.sess, uri_path, fd))) {
		    upload_okay = 1;
		} else {
		    /* TODO: offer to save locally instead */
		    printf(_("Try uploading again (y/n)? "));
		    if (!yesno()) {
			upload_okay = 1;
		    }
		}
	    } while (!upload_okay);

            if (etag) {
                ne_unhook_pre_send(session.sess, edit_pre_send, etag);
                ne_unhook_post_send(session.sess, edit_post_send, NULL);
            }
	    close(fd);
	}
    }
    
    if (unlink(fname)) {
	printf(_("edit: Could not delete temporary file %s:\n%s\n"), fname,
	       strerror(errno));
    }	       

    /* Return 1: Checkin, 2: Checkout, 0: otherwise */
    is_checkout = is_vcr(uri_path);
    if (is_checkout==2) {
        execute_checkin(uri_path);
    }
    
    /* UNLOCK it again whether we succeed or failed in our mission */
    if (can_lock) {
	out_start_uri(_("Unlocking"), uri_path);
	out_result(ne_unlock(session.sess, lock));
	ne_lockstore_remove(session.locks, lock);
	ne_lock_destroy(lock);
    }

    if (etag) ne_free(etag);

    goto edit_bail;
edit_close:
    close(fd);
edit_bail:
    free(uri_path);
    return;
}
