// Read-only "file" access over the embedded WL1 blobs.
#pragma once

// Returns a handle >= 0, or -1 if the name is not embedded.
int  tcm_fopen(const char *name);
int  tcm_fread(int h, void *buf, unsigned len);
long tcm_flseek(int h, long pos, int whence);   // whence: 0 SET / 1 CUR / 2 END
void tcm_fclose(int h);
long tcm_fsize(int h);

#define TCM_SEEK_SET 0
#define TCM_SEEK_CUR 1
#define TCM_SEEK_END 2
