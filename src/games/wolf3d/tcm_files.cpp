#include "tcm_files.h"
#include <string.h>
#include "wolfdata.h"

namespace
{
    struct Blob { const char *name; const uint8_t *data; long size; };
    const Blob blobs[] = {
        { "vgahead.",  wolf_vgahead,  (long)sizeof(wolf_vgahead)  },
        { "vgadict.",  wolf_vgadict,  (long)sizeof(wolf_vgadict)  },
        { "vgagraph.", wolf_vgagraph, (long)sizeof(wolf_vgagraph) },
        { "maphead.",  wolf_maphead,  (long)sizeof(wolf_maphead)  },
        { "gamemaps.", wolf_gamemaps, (long)sizeof(wolf_gamemaps) },
        { "audiohed.", wolf_vgahead,  (long)sizeof(wolf_vgahead)  }, // never really used
        { "audiot.",   wolf_vgadict,  (long)sizeof(wolf_vgadict)  }, // never really used
    };

    struct File { const uint8_t *data; long size; long pos; bool used; };
    File files[8];

    int alloc_slot(const Blob &b)
    {
        for (int i = 0; i < 8; ++i)
        {
            if (!files[i].used)
            {
                files[i] = { b.data, b.size, 0, true };
                return i;
            }
        }
        return -1;
    }
}

int tcm_fopen(const char *name)
{
    // name arrives as DATADIR-prefixed with trailing extension appended by the
    // engine ("vgahead.wl1" etc.). Match on the leading part before '.'.
    const char *base = name;
    const char *slash = strrchr(name, '/');
    if (slash) base = slash + 1;
    const char *dot = strchr(base, '.');
    int blen = dot ? (int)(dot - base) : (int)strlen(base);
    for (const Blob &b : blobs)
    {
        if ((int)strlen(b.name) == blen + 1 && !strncmp(base, b.name, blen))
            return alloc_slot(b);
    }
    return -1;
}

int tcm_fread(int h, void *buf, unsigned len)
{
    if (h < 0 || h >= 8 || !files[h].used) return -1;
    File &f = files[h];
    long avail = f.size - f.pos;
    if (avail <= 0) return 0;
    if ((long)len > avail) len = (unsigned)avail;
    memcpy(buf, f.data + f.pos, len);
    f.pos += len;
    return (int)len;
}

long tcm_flseek(int h, long pos, int whence)
{
    if (h < 0 || h >= 8 || !files[h].used) return -1;
    File &f = files[h];
    switch (whence)
    {
        case TCM_SEEK_SET: break;
        case TCM_SEEK_CUR: pos += f.pos; break;
        case TCM_SEEK_END: pos += f.size; break;
        default: return -1;
    }
    if (pos < 0) pos = 0;
    if (pos > f.size) pos = f.size;
    f.pos = pos;
    return f.pos;
}

void tcm_fclose(int h)
{
    if (h >= 0 && h < 8) files[h].used = false;
}

long tcm_fsize(int h)
{
    if (h < 0 || h >= 8 || !files[h].used) return -1;
    return files[h].size;
}
