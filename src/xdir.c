/* Dynamic Boards v2.4 - Directory utilities (cross-platform) */

#include "conf.h"
#include "sysdep.h"
#include "xdir.h"

#include <sys/types.h>
#include <sys/stat.h>

#ifdef CIRCLE_WINDOWS
#include <direct.h>
#include <windows.h>

int xdir_scan(const char *dir_name, struct xap_dir *xapdirp) {
  HANDLE dirhandle;
  WIN32_FIND_DATA wtfd;
  int i;

  xapdirp->current = xapdirp->total = -1;

  dirhandle = FindFirstFile(dir_name, &wtfd);
  if (dirhandle == INVALID_HANDLE_VALUE)
    return -1;

  xapdirp->total = 1;
  while (FindNextFile(dirhandle, &wtfd))
    xapdirp->total++;

  if (GetLastError() != ERROR_NO_MORE_FILES) {
    xapdirp->total = -1;
    return -1;
  }

  FindClose(dirhandle);
  dirhandle = FindFirstFile(dir_name, &wtfd);

  xapdirp->namelist = (char **)malloc(sizeof(char *) * xapdirp->total);

  for (i = 0; i < xapdirp->total && FindNextFile(dirhandle, &wtfd); i++)
    xapdirp->namelist[i] = strdup(wtfd.cFileName);

  FindClose(dirhandle);

  xapdirp->current = 0;
  return xapdirp->total;
}

char *xdir_get_name(struct xap_dir *xd, int i) {
  return xd->namelist[i];
}

char *xdir_get_next(struct xap_dir *xd) {
  if (++(xd->current) >= xd->total)
    return NULL;
  return xd->namelist[xd->current - 1];
}

#else /* Unix / Linux */

#include <dirent.h>
#include <unistd.h>

int xdir_scan(const char *dir_name, struct xap_dir *xapdirp) {
  xapdirp->total = scandir(dir_name, &(xapdirp->namelist), 0, alphasort);
  xapdirp->current = 0;
  return xapdirp->total;
}

char *xdir_get_name(struct xap_dir *xd, int i) {
  return xd->namelist[i]->d_name;
}

char *xdir_get_next(struct xap_dir *xd) {
  if (++(xd->current) >= xd->total)
    return NULL;
  return xd->namelist[xd->current - 1]->d_name;
}

#endif

void xdir_close(struct xap_dir *xd) {
  int i;
  for (i = 0; i < xd->total; i++) {
    free(xd->namelist[i]);
  }
  free(xd->namelist);
  xd->namelist = NULL;
  xd->current = xd->total = -1;
}

int xdir_get_total(struct xap_dir *xd) {
  return xd->total;
}

int insure_directory(char *path, int isfile) {
  char *chopsuey = strdup(path);
  char *p;
#ifdef CIRCLE_WINDOWS
  struct _stat st;
#else
  struct stat st;
#endif
  extern int errno;

  /* If it's a file, remove the filename */
  if (isfile) {
    if (!(p = strrchr(chopsuey, '/'))) {
      free(chopsuey);
      return 1;
    }
    *p = '\0';
  }

  /* Remove trailing slashes */
  while (chopsuey[strlen(chopsuey) - 1] == '/')
    chopsuey[strlen(chopsuey) - 1] = '\0';

#ifdef CIRCLE_WINDOWS
  if (!_stat(chopsuey, &st) && S_ISDIR(st.st_mode)) {
#else
  if (!stat(chopsuey, &st) && S_ISDIR(st.st_mode)) {
#endif
    free(chopsuey);
    return 1;
  }

  /* Recurse to ensure parent directories exist */
  char *temp = strdup(chopsuey);
  if ((p = strrchr(temp, '/')) != NULL) {
    *p = '\0';
    if (!insure_directory(temp, 0)) {
      free(temp);
      free(chopsuey);
      return 0;
    }
  }
  free(temp);

  /* Attempt to create this directory */
#ifdef CIRCLE_WINDOWS
  if (!_mkdir(chopsuey)) {
#else
  if (!mkdir(chopsuey, S_IRWXU | S_IRWXG | S_IRWXO)) {
#endif
    free(chopsuey);
    return 1;
  }

  if (errno == EEXIST
#ifdef CIRCLE_WINDOWS
      && !_stat(chopsuey, &st)
#else
      && !stat(chopsuey, &st)
#endif
      && S_ISDIR(st.st_mode)) {
    free(chopsuey);
    return 1;
  } else {
    free(chopsuey);
    return 0;
  }
}

