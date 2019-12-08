#ifndef PERMISSIONS_H
#define PERMISSIONS_H

// (user and group must be both NULL or both non-NULL)
int set_file_perms(char *file, char *user, char *group, char *mode);

#endif // PERMISSIONS_H

