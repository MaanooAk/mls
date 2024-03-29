// Copyright (c) 2020 Akritas Akritidis, see LICENSE for license details

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <linux/limits.h>

#include "colors.c"
#include "tags.c"
#include "help.c"

#include "mls.h"


#define PATH_BUF_SIZE      2 * PATH_MAX
#define GETDENTS_BUF_SIZE  8 * 1024
#define ITEMS_BUFFER       128 * 1024


char option_all = 0;
char option_short = 0;
char option_mix = 0;
char option_sort = 'T';
char option_rev = 0;
char option_tree = 0;
char option_collapse = 0;
char option_files = 1;


int handle_args(int argc, char *argv[], int start) {

	for (int i = start; i < argc; i++) {

		if (argv[i][0] != '-') {
			return i;

		} else for (int ci = 1; argv[i][ci] != 0; ci++) {

			     if (argv[i][ci] == 'a') option_all = 1;
			else if (argv[i][ci] == 't') option_tree = 1;
			else if (argv[i][ci] == 'c') option_collapse += 1;
			else if (argv[i][ci] == 'U') option_sort = 0;
			else if (argv[i][ci] == 'S') option_sort = 'S';
			else if (argv[i][ci] == 'N') option_sort = 'N';
			else if (argv[i][ci] == 'X') option_sort = 'X';
			else if (argv[i][ci] == 'T') option_sort = 'T';
			else if (argv[i][ci] == 'r') option_rev = !option_rev;
			else if (argv[i][ci] == 'm') option_mix = 1;
			else if (argv[i][ci] == 's') option_short += 1;
			else if (argv[i][ci] == 'd') option_files = 0;

			else { printf("%s", help); exit(1); }
		}
	}
	return -1;
}

struct tm tnow;
const char* type_symbol[256];
const char* type_color[256];
const char* executable_colortext;

const char* get_color_entry_or(const char* name, const char* def) {
	struct color_entry *ce = get_color_entry(name, strlen(name));
	return ce ? ce->colortext : def;
}

const char* get_tag_entry_or(const char* name, const char* def) {
	struct tag_entry *e = get_tag_entry(name, strlen(name));
	return e ? e->tag : def;
}

void fill_consts() {

	tzset();
	time_t now = time(NULL);
	localtime_r(&now, &tnow);

	type_symbol[DT_REG]  = "     ";
	type_symbol[DT_DIR]  = "\e[1;34md\e[0m    ";
	type_symbol[DT_LNK]  = "\e[1;36ml\e[0m    ";
	type_symbol[DT_FIFO] = "fifo ";
	type_symbol[DT_SOCK] = "sock ";
	type_symbol[DT_BLK]  = "blk  ";
	type_symbol[DT_CHR]  = "chr  ";
	type_symbol[DT_UNKNOWN] = "unkn ";

	type_color[DT_REG]  = get_color_entry_or("FILE", "");
	type_color[DT_DIR]  = get_color_entry_or("DIR", "1;34");
	type_color[DT_LNK]  = get_color_entry_or("LINK", "1;33");
	type_color[DT_FIFO] = get_color_entry_or("FIFO", "1;33");
	type_color[DT_SOCK] = get_color_entry_or("SOCK", "1;33");
	type_color[DT_BLK]  = get_color_entry_or("BLK", "1;33");
	type_color[DT_CHR]  = get_color_entry_or("CHR", "1;33");
	type_color[DT_UNKNOWN]  = get_color_entry_or("UNKNOWN", "0;91");

	executable_colortext = get_color_entry_or("EXE", "1;32");
}

//#define STACK_FULLNAME

struct item {
	char type;
	off_t size;
	struct timespec time_a;
	struct timespec time_m;

	char ltype;
	char executable;
	struct item* link;
	char* extra;

	char* name;
	char* extension;
#ifdef STACK_FULLNAME
	char fullname[PATH_MAX];
#else
	char* fullname;
#endif
};


struct item gitems[ITEMS_BUFFER];
int gitems_count = 0;
int gitems_max = 0;

struct stats {
	unsigned long dirs;
	unsigned long files;
	char has[PATH_MAX];

	char depth[PATH_MAX];
};


int main(int argc, char *argv[]) {
	fill_consts();

	int shows = 0;

	// if the name of the binary starts with a 't' (eg. tree, tmls) switch to tree mode
	if (argv[0][0] == 't') option_tree = 1;

	int pos = 1;
	while(1) {
		pos = handle_args(argc, argv, pos);
		if (pos == -1) break;

		struct stats stats = { 0, 0, "", "" };
		show(argv[pos++], &stats);
		shows++;
	}

	if (!shows) {
		struct stats stats = { 0, 0, "", "" };
		show(".", &stats);
	}

	// printf("sizeof(struct item) :: %d\nPATH_MAX :: %d\ngitems_max :: %d\n", sizeof(struct item), PATH_MAX, gitems_max);
}

int show(const char* path, struct stats* stats) {

	struct item* items = gitems + gitems_count;
	int items_count = load_items(path, items);

	if (items_count < 0) return items_count;

	gitems_count += items_count;
	if (gitems_count > gitems_max) gitems_max = gitems_count;

	for (int index=0; index<items_count; index++) {
		load_stats(items + index, stats);
	}

	sort_items(items, items_count);

	if (option_tree) {
		print_tree(path, items, items_count, stats);
	} else {
		print_list(path, items, items_count, stats);
	}

	gitems_count -= items_count;
	return items_count;
}

int load_items(const char* path, struct item *items) {
	int items_count = 0;

	int fd = open(path, O_RDONLY|O_DIRECTORY);
	if (fd == -1) {
		fprintf(stderr, "mls: %s: %s\n", strerror(errno), path);
		return -1;
	}

	char fullname[PATH_MAX+1] = {0};
	strcpy(fullname, path);
	strcat(fullname, "/");

	struct linux_dirent {
		unsigned long  d_ino;     // Inode number
		unsigned long  d_off;     // Offset to next linux_dirent
		unsigned short d_reclen;  // Length of this linux_dirent
		char           d_name[];  // Filename (null-terminated)
		/*
		char           pad;       // Zero padding byte
		char           d_type;    // File type, offset is (d_reclen - 1))
		*/
	} *d;

	char buf[GETDENTS_BUF_SIZE];

	while (1) {
		int nread = syscall(SYS_getdents, fd, buf, GETDENTS_BUF_SIZE);

		//if (nread == -1) handle_error("getdents");
		if (nread == -1) return items_count;

		if (nread == 0) break;

		for (int bpos = 0; bpos < nread; bpos += d->d_reclen) {
			d = (struct linux_dirent *) (buf + bpos);

			const char* name = d->d_name;

			if (!option_all && name[0] == '.') continue;
			if (name[0] == '.' && name[1] == 0) continue;
			if (name[0] == '.' && name[1] == '.' && name[2] == 0) continue;

			const char type = *(buf + bpos + d->d_reclen - 1);

			if (!option_files && type != DT_DIR) continue;

			struct item *i = &(items[items_count++]);

			fullname[strlen(path) + 1] = 0;
			strcat(fullname, name);

			i->type = type;
			i->ltype = type;
#ifdef STACK_FULLNAME
			strcpy(i->fullname, fullname);
#else
			i->fullname = strdup(fullname);
#endif
			i->name = i->fullname + strlen(path) + 1;
		}

	}

	close(fd);
	return items_count;
}


void load_stats(struct item *i, struct stats* stats) {

	if (!i->name) {
		char *pos = strrchr(i->fullname, '/');
		i->name = pos + 1;
	}
	i->extension = i->name;

	// load stat
	struct stat s;
	int re = lstat(i->fullname, &s);
	if (re) {
		fprintf(stderr, "mls: %s: link %s \n", strerror(errno), i->fullname);
		i->type = i->ltype = DT_UNKNOWN;
		i->executable = 0;
		i->extra = strerror(errno);
		return;
	}

	if (i->type == DT_UNKNOWN) {
		     if (S_ISREG(s.st_mode)) i->type = DT_REG;
		else if (S_ISDIR(s.st_mode)) i->type = DT_DIR;
		else if (S_ISLNK(s.st_mode)) i->type = DT_LNK;
		else if (S_ISBLK(s.st_mode)) i->type = DT_BLK;
		else if (S_ISCHR(s.st_mode)) i->type = DT_CHR;
		else if (S_ISFIFO(s.st_mode)) i->type = DT_FIFO;
		else if (S_ISSOCK(s.st_mode)) i->type = DT_SOCK;
	}


	if (i->type == DT_REG) {
		char *pos = strrchr(i->name, '.');
		if (pos && pos != i->name && pos[1]) {
			i->extension = pos;
		}
	}

	i->size = i->type == DT_DIR ? -1 : s.st_size;
	i->time_a = s.st_atim;
	i->time_m = s.st_mtim;

	i->executable = (i->type != DT_DIR) && (s.st_mode & S_IXUSR);

	if (i->type == DT_LNK && stats) {
		load_link(i);
	} else {
		i->extra = 0;
	}

	if (stats) { // stats is optional

		if (i->type == DT_DIR) stats->dirs++;
		else stats->files++;

		if (stats->depth[0] == 0) { // root level
			const char* tag = get_tag_entry_or(i->name + (i->type == DT_DIR ? -1 : 0), 0);
			if (tag) {
				strcat(stats->has, tag);
				strcat(stats->has, " ");
			}
		}
	}
}

void load_link(struct item *i) {
	char rpath[PATH_MAX];

	int ret = readlink(i->fullname, rpath, sizeof(rpath));
	if (ret == -1) {
		i->link = 0;
		i->extra = strerror(errno);
		return;
	}
	rpath[ret] = 0;

	struct item* litem = i->link = malloc(sizeof(struct item));
	litem->type = DT_UNKNOWN;
	litem->name = 0;

	if (rpath[0] == '/') {
#ifdef STACK_FULLNAME
		strcpy(litem->fullname, rpath);
#else
		litem->fullname = strdup(rpath);
#endif
	} else {
#ifdef STACK_FULLNAME
		strncpy(litem->fullname, i->fullname, i->name - i->fullname);
		strcat(litem->fullname, rpath);
#else
		char apath[PATH_MAX];
		strncpy(apath, i->fullname, i->name - i->fullname);
		apath[i->name - i->fullname] = 0;
		strcat(apath, rpath);
		litem->fullname = strdup(apath);
#endif
	}

	load_stats(litem, 0);

	i->ltype = litem->type;
	i->extra = strdup(rpath);
}


// Compare methods for struct item with item_cmp_ prefix with no and _rev postfix each.
// Interface function sort_items sorts a struct item array based on option_sort, option_rev
// and option_mix.

#define CMP_HEADER(X) int X (const void * ia, const void * ib)
#define CMP_FUNC(X) CMP_HEADER(X); CMP_HEADER(X ## _rev) { return X(ib, ia); } CMP_HEADER(X)

#define CMP_EXTRACT const struct item *a = (const struct item*) ia, *b = (const struct item*) ib;

#define CMP(X, Y) { if ((X) > (Y)) return 1; if ((X) < (Y)) return -1; }
#define IGN_DOT(X) (X[0] == '.' ? X + 1 : X)

// if not option_mix, move directories at the start
#define CMP_DIRS if (!option_mix) CMP(b->ltype == DT_DIR, a->ltype == DT_DIR)

CMP_FUNC( item_cmp_time ) { CMP_EXTRACT // compare last modification times
	CMP_DIRS
	return (a->time_m.tv_sec - b->time_m.tv_sec);
}

CMP_FUNC( item_cmp_size ) { CMP_EXTRACT // compare sizes
	CMP_DIRS
	CMP(a->size, b->size); // size subtraction may overflow
	return 0;
}

CMP_FUNC( item_cmp_name ) { CMP_EXTRACT // compare names
	CMP_DIRS
	return strcasecmp(IGN_DOT(a->name), IGN_DOT(b->name));
}

CMP_FUNC( item_cmp_exte ) { CMP_EXTRACT // compare extension names
	CMP_DIRS
	return strcmp(a->extension, b->extension);
}

// sort struct item array based on option_sort, option_rev and option_mix
void sort_items(struct item *items, int items_count) {

#define SORT(X) qsort(items, items_count, sizeof(struct item), option_rev ? X ## _rev : X)

	     if (option_sort == 'T') SORT(item_cmp_time);
	else if (option_sort == 'S') SORT(item_cmp_size);
	else if (option_sort == 'N') SORT(item_cmp_name);
	else if (option_sort == 'X') SORT(item_cmp_exte);

#undef SORT
}


void print_root(const char* path, const char* postfix) {
	char root[PATH_MAX];
	// the path prepened with a slash and no trailing slash is needed to find the corrent color
	int last = sprintf(root, "/%s", path) - 1;
	if (root[last] == '/') root[last] = 0;
	const char* color = get_color_entry_or(root, type_color[DT_DIR]);
	// prints no new line, postfix may contain new lines
	printf("\e[%sm%s\e[0m%s", color, root + 1, postfix);
}

void print_list(const char* path, struct item *items, int items_count, struct stats* stats) {
	char buffer[256], size[128], printname[2*PATH_MAX];

	print_root(path, "\n");

	for (int index=0; index<items_count; index++) {
		struct item *i = &(items[index]);

		if (option_short < 1) {
			char sametime = i->time_a.tv_sec /60 == i->time_m.tv_sec /60; // seconds res

			printf(" %s", sametime ?
				"              " :
				print_u_time(buffer, &(i->time_a)));
		}

		if (option_short < 2) {

			printf(" %s", print_u_time(buffer, &(i->time_m)));
		}

		printf(" %s %s\e[%sm%s\e[0m",
			(i->type == DT_REG) ? print_u_size(size, i->size) : type_symbol[(int)i->type],
			(i->name[0] == '.') ? "" : " ",
			(i->type != DT_LNK && i->executable) ? executable_colortext : print_u_colortext(i),
			print_u_printname(printname, i));

		if (i->type == DT_LNK) {
			printf(" \e[2m->\e[0m \e[%sm%s\e[0m",
				(!i->link) ? "0;91" : (i->link->type != DT_LNK && i->link->executable) ? executable_colortext : print_u_colortext(i->link),
				i->extra ? i->extra : "?");
		}

		printf("\n");
	}

	print_stats(stats);
}


void print_tree(const char* path, struct item *items, int items_count, struct stats* stats) {
	int depth_len = strlen(stats->depth);

	if (!stats->depth[0]) print_root(path, "");

	char single_dir = (option_collapse > 0 && items_count == 1 && items[0].type == DT_DIR && stats->depth[0]);
	char single_file = (option_collapse > 1 && items_count == 1 && items[0].type != DT_DIR && stats->depth[0]);
	if (!single_dir && !single_file) printf("\n");

	for (int index=0; index<items_count; index++) {
		struct item *i = &(items[index]);

		const char* color = print_u_colortext(i);
		char printname[2*PATH_MAX];
		print_u_printname(printname, i);

		if (single_dir) {
			printf("/\e[%sm%s\e[0m", color, printname[0] ? printname : i->name);

		} else {

			printf("%s%s\e[%sm%s\e[0m",
				single_file ? "" : stats->depth,
				single_file ? " ── " : index + 1 < items_count ? "├── " : "└── ",
				(i->type != DT_LNK && i->executable) ? executable_colortext : color,
				printname[0] ? printname : i->name);

			if (i->type == DT_LNK) {
				printf(" \e[2m->\e[0m \e[%sm%s\e[0m",
					(!i->link) ? "0;91" : (i->link->type != DT_LNK && i->link->executable) ? executable_colortext : print_u_colortext(i->link),
					i->extra ? i->extra : "?");
			}
		}

		if (i->type == DT_DIR) {
			strcat(stats->depth, index + 1 < items_count ? "│   " : "    ");

			if (show(i->fullname, stats) == -1) printf(" ── ?\n");

			stats->depth[depth_len] = 0;
		} else {
			printf("\n");
		}
	}

	if (depth_len == 0) print_stats(stats);
}

void print_stats(const struct stats *stats) {

	printf("\e[2;%sm%ld \e[0;2;37m%ld\e[0m \e[0;90m%s\e[0m\n",
		type_color[DT_DIR], stats->dirs,
		stats->files,
		stats->has);
}


const char* print_u_printname(char *s, const struct item *i) {
	s[0] = 0;

	if (i->name != i->extension) {
		*i->extension = 0;
		sprintf(s, "%s\e[2m.%s\e[0m", i->name, i->extension+1);
		*i->extension = '.';
		return s;
	} else {
		return i->name;
	}
}

const char* print_u_colortext(const struct item *i) {
	const char* color = 0;

	if (i->type == DT_DIR) {
		color = get_color_entry_or(i->name - 1, color);

	} else {

		color = get_color_entry_or(i->name, color);
		if (!color) color = get_color_entry_or(i->extension, color);
	}

	return color ? color : type_color[(int)i->type];
}

const char* print_u_time(char *s, struct timespec *ts) {

	struct tm t;
	localtime_r(&(ts->tv_sec), &t);

	char* format;
	if (t.tm_year == tnow.tm_year) {
		if (t.tm_mon == tnow.tm_mon) {
			if (t.tm_mday == tnow.tm_mday) {
				format = "\e[30m%y-%m-%d \e[90m%H:%M\e[0m";
			} else format = "\e[30m%y-%m-\e[90m%d %H:%M\e[0m";
		} else format = "\e[30m%y-\e[90m%m-%d %H:%M\e[0m";
	} else format = "\e[90m%y-%m-%d %H:%M\e[0m";

	strftime(s, 200, format, &t);
	return s;
}

const char* print_u_size(char *s, off_t bytes) {

	off_t deci = 0;
	int units = 0;

	while (bytes >= 1000) {
		deci = bytes % 1024;
		bytes /= 1024;
		units++;
	}

	static const char* unit_names = " KMGTPEZY";

	if (bytes == 0 && units == 0) {
		strcpy(s, "   - ");
	} else if (units == 0) {
		sprintf(s, " %3ld ", bytes);
	} else if (bytes < 10 && units > 2) { // show decimals for greater than M
		sprintf(s, " %ld.%ld\e[2m%c\e[0m", bytes + (deci / 102) / 10, (deci / 102) % 10, unit_names[units]);
	} else {
		if (deci > 1024/2) bytes++;
		sprintf(s, " %3ld\e[2m%c\e[0m", bytes, unit_names[units]);
	}
	return s;
}

