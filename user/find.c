#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char*
fmtname(char *path)
{
  static char buf[DIRSIZ + 1];
  char *p;

  // Find first character after last slash.
  for (p = path + strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  if (strlen(p) >= DIRSIZ)
    return p;

  memmove(buf, p, strlen(p));
  memset(buf + strlen(p), ' ', DIRSIZ - strlen(p));
  return buf;
}

void
find(char *path, char *target)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  if ((fd = open(path, 0)) < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  // 起点本身是文件的话，也要匹配
  if (st.type == T_FILE) {
    if (strcmp(fmtname(path), target) == 0) {
      printf("%s\n", path);
    }
    close(fd);
    return;
  }

  if (st.type == T_DIR) {
    if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) {
      printf("find: path too long\n");
      close(fd);
      return;
    }

    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/';

    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0)
        continue;

      // 取出项目名字（de.name)
      char name[DIRSIZ + 1];
      memmove(name, de.name, DIRSIZ);
      name[DIRSIZ] = 0;

      if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        continue;
      }

      memmove(p, de.name, DIRSIZ);
      p[DIRSIZ] = 0;

      if (stat(buf, &st) < 0) {
        printf("find: cannot stat %s\n", buf);
        continue;
      }

      if (strcmp(name, target) == 0) {
        printf("%s\n", buf);
      }

      if (st.type == T_DIR) {
        find(buf, target);
      }
    }
  }

  close(fd);
}

int
main(int argc, char *argv[])
{
  if (argc ==2) {
    find(".", argv[1]);
    exit(0);
  }

  else if (argc == 3) {
    find(argv[1], argv[2]);
    exit(0);
  }
  else {
    printf("incorrect command\n");
    exit(1);
  }
}
