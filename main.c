#include <unistd.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>

#define MAX_VALUE 8192

enum ftype { FT_FILE = 'F', FT_DIR = 'D', FT_DIREND = 0xFF };

struct fheader {
	unsigned char type;
	int mode;
	long size;
	char name[256];
};

int min(int x, int y)
{
	if (x < y) {
		return x;
	} else {
		return y;
	}
}

int pack(const char *path, int fd)
{
	static int status = 0;
	static int depth = 0;

	static char buf[MAX_VALUE];
	struct fheader fh;

	DIR *dp;
	struct dirent *entry;
	struct stat statbuf;
	ssize_t nread;

	if ((dp = opendir(path)) == NULL) {
		perror("opendir");
		status = 1;
		return status;
	}
	if (lstat(path, &statbuf)) {
		perror("lstat");
		status = 1;
	}

	chdir(path);

	if (depth != 0) {
		fh.type = FT_DIR;
		fh.mode = statbuf.st_mode & 0777;
		fh.size = 0;
		strncpy(fh.name, path, sizeof(fh.name));

		if (write(fd, &fh, sizeof(fh)) != sizeof(fh)) {
			printf("Error");
		}
	}

	while ((entry = readdir(dp)) != NULL) {
		if (lstat(entry->d_name, &statbuf)) {
			perror("lstat");
			status = 1;
			continue;
		}

		switch (statbuf.st_mode & S_IFMT) {
		case S_IFDIR:
			if (strcmp(".", entry->d_name) == 0 ||
			    strcmp("..", entry->d_name) == 0)
				continue;
			++depth;
			pack(entry->d_name, fd);
			break;
		case S_IFREG:

			fh.type = FT_FILE;
			fh.mode = statbuf.st_mode & 0777;
			fh.size = statbuf.st_size;
			strncpy(fh.name, entry->d_name, sizeof(fh.name));

			if (write(fd, &fh, sizeof(fh)) != sizeof(fh)) {
				printf("Error");
			}

			if (open(entry->d_name, O_RDONLY, 0) == -1) {
				printf("Error");
			}

			int fd_in = open(entry->d_name, O_RDONLY, 0);

			while ((nread = read(fd_in, buf, MAX_VALUE)) > 0) {
				if (write(fd, buf, nread) != nread) {
					printf("Error");
				}
			}
			close(fd_in);
			break;
		}
	}

	if (depth != 0) {
		fh.type = FT_DIREND;
		write(fd, &fh.type, 1);
	}

	chdir("..");
	closedir(dp);
	--depth;
	return status;
}

int unpack(const char *path, int fd)
{
	static char buf[MAX_VALUE];
	struct fheader fh;

	if (mkdir(path, 0775) && errno != EEXIST) {
		perror("mkdir");
		return 1;
	}

	if (chdir(path)) {
		perror("chdir");
		return 1;
	}

	off_t fpos = lseek(fd, 0, SEEK_CUR);
	while (read(fd, &fh, sizeof(fh)) > 0) {
		switch (fh.type) {
		case FT_FILE:;
			int fd_out = creat(fh.name, fh.mode);
			if (fd_out == -1) {
				perror("creat");
				return 1;
			}
			while (fh.size > 0) {
				ssize_t toread = min(fh.size, MAX_VALUE);
				ssize_t nread = read(fd, buf, toread);
				ssize_t nwrite;

				if (nread == -1) {
					perror("read");
					close(fd_out);
					return 1;
				}

				if ((nwrite = write(fd_out, buf, nread)) !=
				    nread) {
					printf("Error");
				}

				if (nwrite == -1) {
					perror("write");
					close(fd_out);
					return 1;
				}
				fh.size -= nwrite;
			}
			close(fd_out);
			break;

		case FT_DIR:
			if (mkdir(fh.name, fh.mode)) {
				perror("mkdir");
				return 1;
			}
			if (chdir(fh.name)) {
				perror("chdir");
				return 1;
			}
			break;
		case FT_DIREND:
			lseek(fd, fpos + 1, SEEK_SET);
			chdir("..");
		}
		fpos = lseek(fd, 0, SEEK_CUR);
	}

	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 4) {
		puts("Error");
		exit(EXIT_FAILURE);
	}

	int resultcode;
	int archive;

	if (strcmp(argv[1], "pack") == 0) {
		archive = creat(argv[3], 0666);
		if (archive == -1) {
			perror("creat");
			exit(EXIT_FAILURE);
		}
		resultcode = pack(argv[2], archive);
	} else {
		if (strcmp(argv[1], "unpack") == 0) {
			archive = open(argv[3], O_RDONLY, 0);
			if (archive == -1) {
				perror("open");
				exit(EXIT_FAILURE);
			}
			resultcode = unpack(argv[2], archive);
		} else {
			printf("Error");
			exit(EXIT_FAILURE);
		}
	}
	exit(resultcode);
}
