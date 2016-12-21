#ifndef PATH_H
#define PATH_H

#include "Types.h"

void InitPath(const char *resDir, const char *saveDir);

void DosToLocalPath(char *localPath, const char *dosPath, bool homeRes);
void LocalToDosPath(char *dosPath, const char *localPath);

const char *GetSaveDosDir();

#endif // PATH_H
