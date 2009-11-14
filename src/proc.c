/* -*- mode: c; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: t -*-
 * vim: ts=4 sw=4 noet ai cindent syntax=c
 *
 * Conky, a system monitor, based on torsmo
 *
 * Any original torsmo code is licensed under the BSD license
 *
 * All code written since the fork of torsmo is licensed under the GPL
 *
 * Please see COPYING for details
 *
 * Copyright (c) 2004, Hannu Saransaari and Lauri Hakkarainen
 * Copyright (c) 2005-2009 Brenden Matthews, Philip Kovacs, et. al.
 *   (see AUTHORS)
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <logging.h>
#include "conky.h"
#include "proc.h"
#include <unistd.h>
#include <ctype.h>
#include <dirent.h>

void scan_pid_arg(struct text_object *obj, const char *arg, void* free_at_crash, const char *file)
{
	pid_t pid;

	if(sscanf(arg, "%d", &pid) == 1) {
		asprintf(&obj->data.s, PROCDIR "/%d/%s", pid, file);
	} else {
		CRIT_ERR(obj, free_at_crash, "syntax error: ${pid_%s pid}", file);
	}
}

void scan_pid_cmdline_arg(struct text_object *obj, const char *arg, void* free_at_crash)
{
	scan_pid_arg(obj, arg, free_at_crash, "cmdline");
}

void print_pid_cmdline(struct text_object *obj, char *p, int p_max_size)
{
	char buf[p_max_size];
	FILE* infofile;
	int i, bytes_read;

	infofile = fopen(obj->data.s, "r");
	if(infofile) {
		bytes_read = fread(buf, 1, p_max_size, infofile);
		for(i = 0; i < bytes_read-1; i++) {
			if(buf[i] == 0) {
				buf[i] = ' ';
			}
		}
		snprintf(p, p_max_size, "%s", buf);
		fclose(infofile);
	} else {
		NORM_ERR(READERR, obj->data.s);
	}
}

void scan_pid_cwd_arg(struct text_object *obj, const char *arg, void* free_at_crash)
{
	scan_pid_arg(obj, arg, free_at_crash, "cwd");
}

void print_pid_cwd(struct text_object *obj, char *p, int p_max_size)
{
	char buf[p_max_size];
	int bytes_read;

	memset(buf, 0, p_max_size);
	bytes_read = readlink(obj->data.s, buf, p_max_size);
	if(bytes_read != -1) {
		snprintf(p, p_max_size, "%s", buf);
	} else {
		NORM_ERR(READERR, obj->data.s);
	}
}

void scan_pid_environ_arg(struct text_object *obj, const char *arg, void* free_at_crash)
{
	pid_t pid;
	int i;
	struct environ_data* ed = malloc(sizeof(struct environ_data));

	ed->var = malloc(strlen(arg));
	if(sscanf(arg, "%d %s", &pid, ed->var) == 2) {
		asprintf(&ed->file, PROCDIR "/%d/environ", pid);
		for(i = 0; ed->var[i] != 0; i++) {
			ed->var[i] = toupper(ed->var[i]);
		}
		obj->data.opaque = ed;
	} else {
		free(ed->var);
		free(ed);
		CRIT_ERR(obj, free_at_crash, "${pid_environ pid varname}");
	}
}

void print_pid_environ(struct text_object *obj, char *p, int p_max_size)
{
	char *buf = NULL;
	char *searchstring;
	FILE* infofile;
	int bytes_read, total_read = 0;

	searchstring = malloc(strlen(((struct environ_data*) obj->data.opaque)->var) + strlen("=%[\1-\255]") + 1);
	strcpy(searchstring, ((struct environ_data*) obj->data.opaque)->var);
	strcat(searchstring, "=%[\1-\255]");
	infofile = fopen(((struct environ_data*) obj->data.opaque)->file, "r");
	if(infofile) {
		do {
			buf = realloc(buf, total_read + p_max_size + 1);
			bytes_read = fread(buf + total_read, 1, p_max_size, infofile);
			total_read += bytes_read;
			buf[total_read] = 0;
		}while(bytes_read != 0);
		for(bytes_read = 0; bytes_read < total_read; bytes_read += strlen(buf + bytes_read) + 1) {
			if(sscanf(buf + bytes_read, searchstring, p) == 1) {
				free(buf);
				free(searchstring);
				fclose(infofile);
				return;
			}
		}
		p[0] = 0;
		free(buf);
		free(searchstring);
		fclose(infofile);
	} else {
		NORM_ERR(READERR, ((struct environ_data*) obj->data.opaque)->file);
	}
}

void free_pid_environ(struct text_object *obj) {
	free(((struct environ_data*) obj->data.opaque)->file);
	free(((struct environ_data*) obj->data.opaque)->var);
	free(obj->data.opaque);
}

void scan_pid_environ_list_arg(struct text_object *obj, const char *arg, void* free_at_crash)
{
	scan_pid_arg(obj, arg, free_at_crash, "environ");
}

void print_pid_environ_list(struct text_object *obj, char *p, int p_max_size)
{
	char *buf = NULL;
	char *buf2;
	FILE* infofile;
	int bytes_read, total_read = 0;
	int i = 0;

	infofile = fopen(obj->data.s, "r");
	if(infofile) {
		do {
			buf = realloc(buf, total_read + p_max_size + 1);
			bytes_read = fread(buf + total_read, 1, p_max_size, infofile);
			total_read += bytes_read;
			buf[total_read] = 0;
		}while(bytes_read != 0);
		while(bytes_read < total_read) {
			buf2 = strdup(buf+bytes_read);
			bytes_read += strlen(buf2)+1;
			sscanf(buf2, "%[^=]", buf+i);
			free(buf2);
			i = strlen(buf) + 1;
			buf[i-1] = ';';
		}
		buf[i-1] = 0;
		snprintf(p, p_max_size, "%s", buf);
		free(buf);
		fclose(infofile);
	} else {
		NORM_ERR(READERR, obj->data.s);
	}
}

void scan_pid_exe_arg(struct text_object *obj, const char *arg, void* free_at_crash)
{
	scan_pid_arg(obj, arg, free_at_crash, "exe");
}

void print_pid_readlink(struct text_object *obj, char *p, int p_max_size)
{
	char buf[p_max_size];

	memset(buf, 0, p_max_size);
	if(readlink(obj->data.s, buf, p_max_size) >= 0) {
		snprintf(p, p_max_size, "%s", buf);
	} else {
		NORM_ERR(READERR, obj->data.s);
	}
}

void scan_pid_chroot_arg(struct text_object *obj, const char *arg, void* free_at_crash) {
	scan_pid_arg(obj, arg, free_at_crash, "root");
}

void print_pid_chroot(struct text_object *obj, char *p, int p_max_size) {
	print_pid_readlink(obj, p, p_max_size);
}

void print_pid_exe(struct text_object *obj, char *p, int p_max_size) {
	print_pid_readlink(obj, p, p_max_size);
}

void scan_pid_state_arg(struct text_object *obj, const char *arg, void* free_at_crash) {
	scan_pid_arg(obj, arg, free_at_crash, "status");
}

void print_pid_state(struct text_object *obj, char *p, int p_max_size) {
#define STATE_ENTRY "State:\t"
	char *begin, *end, *buf = NULL;
	FILE* infofile;
	int bytes_read, total_read = 0;

	infofile = fopen(obj->data.s, "r");
	if(infofile) {
		do {
			buf = realloc(buf, total_read + p_max_size + 1);
			bytes_read = fread(buf + total_read, 1, p_max_size, infofile);
			total_read += bytes_read;
			buf[total_read] = 0;
		}while(bytes_read != 0);
		begin = strstr(buf, STATE_ENTRY);
		if(begin != NULL) {
			begin += strlen(STATE_ENTRY) + 3;	// +3 will strip the char representing the short state and the space and '(' that follow
			end = strchr(begin, '\n');
			if(end != NULL) {
				*(end-1) = 0;
			}
			snprintf(p, p_max_size, "%s",begin);
		} else {
			NORM_ERR(STATENOTFOUND, obj->data.s);
		}
		free(buf);
		fclose(infofile);
	} else {
		NORM_ERR(READERR, obj->data.s);
	}
}

void scan_pid_stderr_arg(struct text_object *obj, const char *arg, void* free_at_crash) {
	scan_pid_arg(obj, arg, free_at_crash, "fd/2");
}

void print_pid_stderr(struct text_object *obj, char *p, int p_max_size) {
	print_pid_readlink(obj, p, p_max_size);
}

void scan_pid_stdin_arg(struct text_object *obj, const char *arg, void* free_at_crash) {
	scan_pid_arg(obj, arg, free_at_crash, "fd/0");
}

void print_pid_stdin(struct text_object *obj, char *p, int p_max_size) {
	print_pid_readlink(obj, p, p_max_size);
}

void scan_pid_stdout_arg(struct text_object *obj, const char *arg, void* free_at_crash) {
	scan_pid_arg(obj, arg, free_at_crash, "fd/1");
}

void print_pid_stdout(struct text_object *obj, char *p, int p_max_size) {
	print_pid_readlink(obj, p, p_max_size);
}

void scan_pid_openfiles_arg(struct text_object *obj, const char *arg, void* free_at_crash) {
	scan_pid_arg(obj, arg, free_at_crash, "fd");
}

struct ll_string {
	char *string;
	struct ll_string* next;
};

struct ll_string* addnode(struct ll_string* end, char* string) {
	struct ll_string* current = malloc(sizeof(struct ll_string));
	current->string = strdup(string);
	current->next = NULL;
	if(end != NULL) end->next = current;
	return current;
}

void freelist(struct ll_string* front) {
	if(front != NULL) {
		free(front->string);
		if(front->next != NULL) {
			freelist(front->next);
		}
		free(front);
	}
}

int inlist(struct ll_string* front, char* string) {
	struct ll_string* current;

	for(current = front; current != NULL; current = current->next) {
		if(strcmp(current->string, string) == 0) {
			return 1;
		}
	}
	return 0;
}

void print_pid_openfiles(struct text_object *obj, char *p, int p_max_size) {
	DIR* dir;
	struct dirent *entry;
	char buf[p_max_size];
	int length, totallength = 0;
	struct ll_string* files_front = NULL;
	struct ll_string* files_back = NULL;

	dir = opendir(obj->data.s);
	if(dir != NULL) {
		while ((entry = readdir(dir))) {
			if(entry->d_name[0] != '.') {
				snprintf(buf, p_max_size, "%s/%s", obj->data.s, entry->d_name);
				length = readlink(buf, buf, p_max_size);
				buf[length] = 0;
				if(inlist(files_front, buf) == 0) {
					files_back = addnode(files_back, buf);
					snprintf(p + totallength, p_max_size - totallength, "%s; " , buf);
					totallength += length + strlen("; ");
				}
				if(files_front == NULL) {
					files_front = files_back;
				}
			}
		}
		closedir(dir);
		freelist(files_front);
		p[totallength - strlen("; ")] = 0;
	} else {
		p[0] = 0;
	}
}