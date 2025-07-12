#ifndef _XDIR_H_
#define _XDIR_H_

/* General use directory functions & structures.
 * Required due to various differences between directory handling
 * code on different OS'es.
 * Dynamic Boards v2.4 - PjD (dughi@imaxx.net)
 */

#ifdef CIRCLE_UNIX
#include <dirent.h>

struct xap_dir {
  int total, current;
  struct dirent **namelist;
};

#elif defined(CIRCLE_WINDOWS)

struct xap_dir {
  int total, current;
  char **namelist;
};

#endif

int xdir_scan(const char *dir_name, struct xap_dir *xapdirp);
int xdir_get_total(struct xap_dir *xd);
char *xdir_get_name(struct xap_dir *xd, int num);
char *xdir_get_next(struct xap_dir *xd);
void xdir_close(struct xap_dir *xd);
int insure_directory(char *path, int isfile);

#endif /* _XDIR_H_ */
