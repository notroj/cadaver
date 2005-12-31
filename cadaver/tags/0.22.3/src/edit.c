/* 
   cadaver, command-line DAV client
   Copyright (C) 1999-2002, Joe Orton <joe@manyfish.co.uk>, 

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
    editor = get_option(opt_editor);
    if (editor == NULL) {
	editor = getenv("EDITOR");
	if (editor == NULL) {
	    editor = "vi";
	}
    }
    snprintf(editcmd, BUFSIZ, "%s %s", editor, filename);
    if (stat(filename, &before_st)) {
	printf(_("Could not stat file: %s\n"), strerror(errno));
	return -1;
    }
    printf("Running editor: `%s'...\n", editcmd);
    system(editcmd);
    if (stat(filename, &after_st)) {
	printf(_("Error! Could not examine temporary file: %s\n"), 
		 strerror(errno));
	return -1;
    }
    if (before_st.st_mtime == after_st.st_mtime) {
	/* File not changed. */
	printf(_("No changes were made.\n"));
	return -1;
    } else {
	printf(_("Changes were made.\n"));
	return 0;
    }	
}

/* Returns true if resource at URI is lockable. */
static int is_lockable(const char *uri)
{
    ne_server_capabilities caps = {0};

    /* TODO: for the mo, just check we're on a Class 2 server.
     * Should check supportedlock property really. */

    if (ne_options(session.sess, uri, &caps) != NE_OK) {
	return 0;
    }

    return caps.dav_class2;
}

#ifndef _POSIX_PATH_MAX
#define _POSIX_PATH_MAX (256)
#endif

/* TODO: this is great big heap of steaming trout.  */
/* TODO: does this work under cygwin? mkstemp() may or may not open
   the file using O_BINARY, and then we *do* upload it using O_BINARY,
   so maybe this will screw things up. Maybe we should fcntl it and
   set O_BINARY, if that is allowed under cygwin? */
void execute_edit(const char *remote)
{
    char *real_remote;
    unsigned int can_lock; /* can we LOCK it? */
    struct ne_lock *lock = NULL;
    char fname[_POSIX_PATH_MAX] = "/tmp/cadaver-edit-XXXXXX";
    const char *pnt;
    int fd;
    int is_checkout, is_checkin;
    
    real_remote = resolve_path(session.uri.path, remote, false);

    /* Don't let them edit a collection, since PUT to a collection is
     * bogus. Really we want to be able to fetch a "DefaultDocument"
     * property, and edit on that instead: IIS does expose such a
     * property. Would be a nice hack to add the support to mod_dav
     * too. */
    if (getrestype(real_remote) == resr_collection) {
	printf(_("You cannot edit a collection resource (%s).\n"),
	       real_remote);
	goto edit_bail;
    }

    can_lock = is_lockable(real_remote);

    /* Give the local temp file the same extension as the remote path,
     * so the editor can have a stab at the content-type. */
    pnt = strrchr(real_remote, '.');
    if (pnt != NULL && strchr(pnt, '/') == NULL) {
	strncat(fname, pnt, _POSIX_PATH_MAX);
	fname[_POSIX_PATH_MAX-1] = '\0';
    }

    fd = cad_mkstemp(fname);
    if (fd == -1) {
	printf(_("Could not create temporary file %s:\n%s\n"), fname,
	       strerror(errno));
	goto edit_bail;
    }

    /* Sanity check on the file perms. */
    if (fchmod(fd, 0600) == -1) {
	printf(_("Could not set file permissions for %s:\n%s\n"), fname,
	       strerror(errno));
	goto edit_bail;
    }
   
    if (can_lock) {
	lock = ne_lock_create();
	ne_fill_server_uri(session.sess, &lock->uri);
	lock->uri.path = ne_strdup(real_remote);
	lock->owner = getowner();
	out_start("Locking", remote);
	if (out_handle(ne_lock(session.sess, lock))) {
	    ne_lockstore_add(session.locks, lock);
	} else {
	    ne_lock_destroy(lock);
	    goto edit_close;
	}
    } else {
	/* TODO: HEAD and get the Etag/modtime */
    }

    /* Return 1: Checkin, 2: Checkout, 0: otherwise */
    is_checkin = is_vcr(real_remote);
    if (is_checkin==1) {
    	execute_checkout(real_remote);
    }
    
    /* FIXME: copy'n'friggin'paste. */
    output(o_transfer, _("Downloading `%s' to %s"), real_remote, fname);

    /* Don't puke if get fails -- perhaps we are creating a new one? */
    out_result(ne_get(session.sess, real_remote, fd));
    
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
	    do {
		output(o_transfer, _("Uploading changes to `%s'"), 
		       real_remote);
		/* FIXME: conditional PUT using fetched Etag/modtime if
		 * !can_lock */
		if (out_handle(ne_put(session.sess, real_remote, fd))) {
		    upload_okay = 1;
		} else {
		    /* TODO: offer to save locally instead */
		    printf(_("Try uploading again (y/n)? "));
		    if (!yesno()) {
			upload_okay = 1;
		    }
		}
	    } while (!upload_okay);
	    close(fd);
	}
    }
    
    if (unlink(fname)) {
	printf(_("Could not delete temporary file %s:\n%s\n"), fname,
	       strerror(errno));
    }	       

    /* Return 1: Checkin, 2: Checkout, 0: otherwise */
    is_checkout = is_vcr(real_remote);
    if (is_checkout==2) {
    	execute_checkin(real_remote);
    }
    
    /* UNLOCK it again whether we succeed or failed in our mission */
    if (can_lock) {
	output(o_start, "Unlocking `%s':", remote);
	out_result(ne_unlock(session.sess, lock));
	ne_lockstore_remove(session.locks, lock);
	ne_lock_destroy(lock);
    }

    goto edit_bail;
edit_close:
    close(fd);
edit_bail:
    free(real_remote);
    return;
}
