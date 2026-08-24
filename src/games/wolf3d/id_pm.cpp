#include "wl_def.h"
#include "wolfdata.h"

int ChunksInFile;
int PMSpriteStart;
int PMSoundStart;

bool PMSoundInfoPagePadded = false;

// holds the whole VSWAP
uint32_t *PMPageData;
size_t PMPageDataSize;

// ChunksInFile+1 pointers to page starts.
// The last pointer points one byte after the last page.
uint8_t **PMPages;

void PM_Startup()
{
    // VSWAP is embedded as a const blob; pages are stored uncompressed and
    // can be used in place. Only the pointer table needs RAM.
    const uint8_t *file = wolf_vswap;

    ChunksInFile  = file[0] | (file[1] << 8);
    PMSpriteStart = file[2] | (file[3] << 8);
    PMSoundStart  = file[4] | (file[5] << 8);

    uint32_t *pageOffsets = (uint32_t *) malloc((ChunksInFile + 1) * sizeof(int32_t));
    CHECKMALLOCRESULT(pageOffsets);
    memcpy(pageOffsets, file + 6, ChunksInFile * sizeof(uint32_t));

    word *pageLengths = (word *) malloc(ChunksInFile * sizeof(word));
    CHECKMALLOCRESULT(pageLengths);
    memcpy(pageLengths, file + 6 + ChunksInFile * sizeof(uint32_t), ChunksInFile * sizeof(word));

    long fileSize = (long)sizeof(wolf_vswap);
    pageOffsets[ChunksInFile] = fileSize;

    uint32_t dataStart = pageOffsets[0];
    int i;

    for(i = 0; i < ChunksInFile; i++)
    {
        if(!pageOffsets[i]) continue;   // sparse page
        if(pageOffsets[i] < dataStart || pageOffsets[i] >= (size_t) fileSize)
            Quit("Illegal page offset for page %i: %u (filesize: %u)",
                    i, pageOffsets[i], fileSize);
    }

    PMPages = (uint8_t **) malloc((ChunksInFile + 1) * sizeof(uint8_t *));
    CHECKMALLOCRESULT(PMPages);

    // Point pages directly into the embedded image. Sprites need 2-byte
    // alignment; if an odd page exists we fall back to a padded RAM copy.
    int alignPadding = 0;
    for(i = PMSpriteStart; i < PMSoundStart; i++)
    {
        if(!pageOffsets[i]) continue;
        uint32_t offs = pageOffsets[i] - dataStart;
        if(offs & 1) alignPadding++;
    }
    if((pageOffsets[ChunksInFile - 1] - dataStart) & 1)
        alignPadding++;

    // Always copy into RAM. (Direct pointers into the embedded const image
    // proved unreliable on real hardware: pages read back as zeroes.)
    size_t totalSize = 0;
    word  *sizes = (word *) malloc(ChunksInFile * sizeof(word));
    CHECKMALLOCRESULT(sizes);
    for(i = 0; i < ChunksInFile; i++)
    {
        uint32_t size;
        if(!pageOffsets[i]) { sizes[i] = 0; continue; }
        if(!pageOffsets[i + 1]) size = pageLengths[i];
        else size = pageOffsets[i + 1] - pageOffsets[i];
        sizes[i] = (word)size;
    }
    for(i = 0; i < ChunksInFile; i++)
        totalSize += sizes[i];

    PMPageData = (uint32_t *) malloc(totalSize + 16);
    CHECKMALLOCRESULT(PMPageData);
    PMPageDataSize = totalSize;

    uint8_t *ptr = (uint8_t *) PMPageData;
    for(i = 0; i < ChunksInFile; i++)
    {
        if(i >= PMSpriteStart && i < PMSoundStart || i == ChunksInFile - 1)
        {
            size_t offs = ptr - (uint8_t *) PMPageData;
            if(offs & 1) *ptr++ = 0;
        }
        PMPages[i] = ptr;
        if(sizes[i])
        {
            memcpy(ptr, file + pageOffsets[i], sizes[i]);
            ptr += sizes[i];
        }
    }
    PMPages[ChunksInFile] = ptr;

    free(sizes);
    free(pageLengths);
    free(pageOffsets);
}


void PM_Shutdown()
{
    free(PMPages);
    PMPages = NULL;
}
