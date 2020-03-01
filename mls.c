
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
#include "help.c"


#define PATH_BUF_SIZE      2 * PATH_MAX
#define GETDENTS_BUF_SIZE  8 * 1024
#define ITEMS_BUFFER       128 * 1024


char option_all = 0;
char option_inode = 0;
char option_mix = 0;
char option_sort = 'T';
char option_tree = 0;
char option_collapse = 0;


int handle_args(int argc, char *argv[], int start) {
	char onlyfiles = 0;

	for (int i = start; i < argc; i++) {

		if (argv[i][0] != '-' || onlyfiles) {
			return i;

		} else for (int ci = 1; argv[i][ci] != 0; ci++) {

			if (argv[i][ci] == '-' && argv[i][ci+1] == 0) onlyfiles = 1;

			else if (argv[i][ci] == 'a') option_all = 1;
			else if (argv[i][ci] == 't') option_tree = 1;
			else if (argv[i][ci] == 'c') option_collapse = 1;
			else if (argv[i][ci] == 'U') option_sort = 0;
			else if (argv[i][ci] == 'S') option_sort = 'S';
			else if (argv[i][ci] == 'N') option_sort = 'N';
			else if (argv[i][ci] == 'X') option_sort = 'X';
			else if (argv[i][ci] == 'm') option_mix = 1;
			else if (argv[i][ci] == 'i') option_inode = 1;

			else {
				printf("%s", help);
				exit(1);
			}
		}
	}
	return -1;
}

struct tm tnow;
const char* type_symbol[256];
const char* type_color[256];
const char* executable_colortext;

const char* get_color_entry_or(char* name, const char* def) {
	struct color_entry *ce = get_color_entry(name, strlen(name));
	return ce ? ce->colortext : def;
}

void fill_consts() {

    tzset();
	time_t now = time(NULL);
    localtime_r(&now, &tnow);

	for (int i=0; i<256; i++) type_symbol[i] = "?", type_color[i] = "1;31";

	type_symbol[DT_REG]  = "     ";
	type_symbol[DT_DIR]  = "\e[1;34md\e[0m    ";
	type_symbol[DT_FIFO] = "fifo ";
	type_symbol[DT_SOCK] = "sock ";
	type_symbol[DT_LNK]  = "\e[1;36ml\e[0m    ";
	type_symbol[DT_BLK]  = "blk  ";
	type_symbol[DT_CHR]  = "chr  ";

	type_color[DT_REG]  = get_color_entry_or("FILE", "");
	type_color[DT_DIR]  = get_color_entry_or("DIR", "1;34");
	type_color[DT_FIFO] = get_color_entry_or("FIFO", "1;33");
	type_color[DT_SOCK] = get_color_entry_or("SOCK", "1;33");
	type_color[DT_LNK]  = get_color_entry_or("LINK", "1;33");
	type_color[DT_BLK]  = get_color_entry_or("BLK", "1;33");
	type_color[DT_CHR]  = get_color_entry_or("CHR", "1;33");

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

void show(const char* path, char* depth);

int main(int argc, char *argv[]) {
	fill_consts();

	int shows = 0;
	char depth[PATH_MAX];
	depth[0] = 0;

	int pos = 1;
	while(1) {
		pos = handle_args(argc, argv, pos);
		if (pos == -1) break;

		show(argv[pos++], depth);
		shows++;
	}

	if (!shows) show(".", depth);

	//printf("sizeof(struct item) :: %d\nPATH_MAX :: %d\ngitems_max :: %d\n", sizeof(struct item), PATH_MAX, gitems_max);
}


int  load_items(const char* path, struct item *items);
void load_stats(struct item *i);
void sort_items(struct item *items, int items_count);
void print_list(const char* path, struct item *items, int items_count);
void print_tree(const char* path, struct item *items, int items_count, char* depth);

void show(const char* path, char* depth) {

	struct item* items = gitems + gitems_count;
	int items_count = load_items(path, items);

	gitems_count += items_count;
	if (gitems_count > gitems_max) gitems_max = gitems_count;

	for (int index=0; index<items_count; index++) {
		load_stats(items + index);
	}

	sort_items(items, items_count);

	if (option_tree) {
		print_tree(path, items, items_count, depth);
	} else {
		print_list(path, items, items_count);
	}

	gitems_count -= items_count;
}

int load_items(const char* path, struct item *items) {
	int items_count = 0;

	int fd = open(path, O_RDONLY|O_DIRECTORY);
	if (fd == -1) return 0;

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

			struct item *i = &(items[items_count++]);
			const char type = *(buf + bpos + d->d_reclen - 1);

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


void load_stats(struct item *i) {

	// find the extension
	i->extension = i->name;
	if (i->type == DT_REG || 0) {
		char *pos = strrchr(i->name, '.');
		if (pos && pos != i->name && pos[1]) {
			i->extension = pos;
		}
	}

	/*
	struct stats {
	   dev_t     st_dev;         // ID of device containing file
	   ino_t     st_ino;         // Inode number
	   mode_t    st_mode;        // File type and mode
	   nlink_t   st_nlink;       // Number of hard links
	   uid_t     st_uid;         // User ID of owner
	   gid_t     st_gid;         // Group ID of owner
	   dev_t     st_rdev;        // Device ID (if special file)
	   off_t     st_size;        // Total size, in bytes
	   blksize_t st_blksize;     // Block size for filesystem I/O
	   blkcnt_t  st_blocks;      // Number of 512B blocks allocated

	   struct timespec st_atim;  // Time of last access
	   struct timespec st_mtim;  // Time of last modification
	   struct timespec st_ctim;  // Time of last status change
	};
	*/

	// load stat
	struct stat s;
	int re = lstat(i->fullname, &s);
	if (re) { 
		printf("%s: %s\n", i->fullname, strerror(errno));
		s.st_size = 0;
	}
	i->size = i->type == DT_DIR ? -1 : s.st_size;
	i->time_a = s.st_atim;
	i->time_m = s.st_mtim;

	i->executable = (i->type != DT_DIR) && (s.st_mode & S_IXUSR);

	char extra[PATH_MAX];
	extra[0] = 0;
	if (option_inode) {
		sprintf(extra, "%d", s.st_ino);
	} else if (i->type == DT_LNK) {
		// read the link
		int ret = readlink(i->fullname, extra, sizeof(extra));
		if (ret != -1) {
			extra[ret] = 0;
		} else {
			printf("%s: %s\n", i->fullname, strerror(errno)); // TODO handle
		}
	}
	i->extra = extra[0] ? strdup(extra) : 0;

}

int item_cmp_time(const void * a, const void * b);
int item_cmp_size(const void * a, const void * b);
int item_cmp_name(const void * a, const void * b);
int item_cmp_exte(const void * a, const void * b);

void sort_items(struct item *items, int items_count) {

	     if (option_sort == 'T') qsort(items, items_count, sizeof(struct item), item_cmp_time);
	else if (option_sort == 'S') qsort(items, items_count, sizeof(struct item), item_cmp_size);
	else if (option_sort == 'N') qsort(items, items_count, sizeof(struct item), item_cmp_name);
	else if (option_sort == 'X') qsort(items, items_count, sizeof(struct item), item_cmp_exte);
}


#define CMP_FUNC(X) int X (const void * ia, const void * ib)
#define CMP_EXTRACT struct item *a = (struct item*) ia, *b = (struct item*) ib;

#define CMP(X, Y) { if ((X) > (Y)) return 1; if ((X) < (Y)) return -1; }
#define IGN_DOT(X) (X[0] == '.' ? X + 1 : X)

CMP_FUNC( item_cmp_time ) { CMP_EXTRACT

	if (!option_mix) CMP(b->ltype == DT_DIR, a->ltype == DT_DIR)
	return (a->time_m.tv_sec - b->time_m.tv_sec);
}

CMP_FUNC( item_cmp_size ) { CMP_EXTRACT

	if (!option_mix) CMP(b->ltype == DT_DIR, a->ltype == DT_DIR)
	CMP(a->size, b->size);
}

CMP_FUNC( item_cmp_name ) { CMP_EXTRACT

	if (!option_mix) CMP(b->ltype == DT_DIR, a->ltype == DT_DIR)
	return strcasecmp(IGN_DOT(a->name), IGN_DOT(b->name));
}

CMP_FUNC( item_cmp_exte ) { CMP_EXTRACT

	if (!option_mix) CMP(b->ltype == DT_DIR, a->ltype == DT_DIR)
	return strcmp(a->extension, b->extension);
}


void print_u_printname(char *s, const struct item *i);
void print_u_size(char *s, off_t bytes);
void print_u_time(char *s, uint len, struct timespec *ts);
const char* print_u_colortext(const struct item *i);

void print_list(const char* path, struct item *items, int items_count) {

	printf("\e[2m%s\e[0m\n", path);

	for (int index=0; index<items_count; index++) {
		struct item *i = &(items[index]);

		char time_a[128], time_m[128	];
		print_u_time(time_a, sizeof(time_a), &(i->time_a));
		print_u_time(time_m, sizeof(time_m), &(i->time_m));

		char size[256];
		size[0] = 0;
		if (i->type == DT_REG) {
			print_u_size(size, i->size);
		}

		const char* color = print_u_colortext(i);
		char printname[2*PATH_MAX];
		print_u_printname(printname, i);

		char hidden = i->name[0] == '.';

		printf(" %s %s %s %s\e[%sm%s\e[0m%s%s%s\n", 
			strcmp(time_a, time_m) ? time_a : "              ", time_m, 
			i->type == DT_REG ? size : type_symbol[i->type],
			hidden ? "" : " ",
			(i->type != DT_LNK && i->executable) ? executable_colortext : color,
			printname[0] ? printname : i->name,
			i->type == DT_LNK ? " \e[2m->\e[0m " : option_inode ? "  \e[2;4m" : "",
			i->extra ? i->extra : "",
			option_inode ? "\e[0m": "");

	}

	printf("\e[2m%d\e[0m\n", items_count);
}


void print_tree(const char* path, struct item *items, int items_count, char* depth) {
	int depth_len = strlen(depth);

	if (!depth[0]) printf("%s", path);

	char single_dir = (option_collapse && items_count == 1 && items[0].type == DT_DIR && depth[0]);
	if (!single_dir) printf("\n");

	for (int index=0; index<items_count; index++) {
		struct item *i = &(items[index]);

		const char* color = print_u_colortext(i);
		char printname[2*PATH_MAX];
		print_u_printname(printname, i);

		if (single_dir) {
			printf("/\e[%sm%s\e[0m", color, printname[0] ? printname : i->name);

		} else {
			printf("%s%s\e[%sm%s\e[0m%s%s%s",
				depth, index + 1 < items_count ? "├── " : "└── ",
				(i->type != DT_LNK && i->executable) ? executable_colortext : color,
				printname[0] ? printname : i->name,
				i->type == DT_LNK ? " \e[2m->\e[0m " : option_inode ? "  \e[2;4m" : "",
				i->extra ? i->extra : "",
				option_inode ? "\e[0m": "");
		}


		if (i->type == DT_DIR) {
			strcat(depth, index + 1 < items_count ? "│   " : "    ");

			show(i->fullname, depth);

			depth[depth_len] = 0;
		} else {
			printf("\n");
		}
	}

}

void print_u_printname(char *s, const struct item *i) {
	s[0] = 0;

	if (i->name != i->extension) {
		*i->extension = 0;
		sprintf(s, "%s\e[2m.%s\e[0m", i->name, i->extension+1);
		*i->extension = '.';
	}
}

const char* print_u_colortext(const struct item *i) {
	const char* color = type_color[i->type];

	if (i->type == DT_DIR) {

		char dirname[PATH_MAX];
		strcpy(dirname, i->name);
		strcat(dirname, "/");
		color = get_color_entry_or(dirname, color);

	} else if (i->type == DT_REG && i->extension[0]) {
		color = get_color_entry_or(i->extension, color);
	}

	return color;
}

void print_u_time(char *s, uint len, struct timespec *ts) {

    struct tm t;
    localtime_r(&(ts->tv_sec), &t);

	char* format = "\e[90m%y-%m-%d %H:%M\e[0m";
	if (t.tm_year == tnow.tm_year) {
		if (t.tm_mon == tnow.tm_mon) {
			if (t.tm_mon == tnow.tm_mon) {
				format = "\e[30m%y-%m-%d \e[90m%H:%M\e[0m";
			} else format = "\e[30m%y-%m-\e[90m%d %H:%M\e[0m";
		} else format = "\e[30m%y-\e[90m%m-%d %H:%M\e[0m";
	}

    strftime(s, len, format, &t);
}

void print_u_size(char *s, off_t bytes) {
	static const char* unit_names[] = {" ", "K", "M", "G", "T", "P", "E", "Z", "Y"};
	
	off_t deci = 0; 
	int units = 0;

	if (bytes == 0) {
		strcpy(s, "   - ");
		return;
	}
	
	while (bytes >= 1000) {
		deci = bytes % 1024;
		bytes /= 1024; 
		units++;
	}

	if (units == 0) {
		sprintf(s, " %3d ", bytes);
	} else if (bytes < 10 && units > 2) {
		sprintf(s, " %d.%d\e[2m%s\e[0m", bytes, deci / 102, unit_names[units]);
	} else {
		if (deci > 1024/2) bytes++;
		sprintf(s, " %3d\e[2m%s\e[0m", bytes, unit_names[units]);
	}
}
