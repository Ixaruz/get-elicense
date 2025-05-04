#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <dirent.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/errno.h>
#include <unistd.h>

#include <switch.h>

#include <debugger.hpp>

void print_u8_buffer(u8 *buffer, int size)
{
    for (int i = 0; i < size; i++)
    {
        if (i % 32 == 0 && i != 0)
            printf("\n");
        printf("%02x", buffer[i]);
    }
    printf("\n");
}

MemoryInfo *query_multi(Debugger &debugger, u64 start, u32 max_count, u32 *out_count)
{
    u32 i = 0;
    u64 offset = 0;
    MemoryInfo *output = (MemoryInfo *)malloc(sizeof(MemoryInfo) * max_count);
    for (i = 0; i < max_count; i++)
    {
        MemoryInfo meminfo = debugger.queryMemory(start + offset);
        output[i] = meminfo;
        if(MemoryType::MemType_Reserved == meminfo.type){
            break;
        }
        offset += meminfo.size;
    }
    *out_count = i;
    return output;
}

MemoryInfo query_find_main(Debugger &debugger, u64 start, u32 max_count)
{
    u32 i = 0;
    u64 offset = 0;
    MemoryInfo meminfo;
    for (i = 0; i < max_count; i++)
    {
        meminfo = debugger.queryMemory(start + offset);
        // break on first CodeStatic (main) region
        if(MemoryType::MemType_CodeStatic == meminfo.type){
            break;
        }
        if(MemoryType::MemType_Reserved == meminfo.type){
            break;
        }
        offset += meminfo.size;
    }
    if (meminfo.type != MemoryType::MemType_CodeStatic)
    {
        printf("Failed to find main region\n");
        meminfo = MemoryInfo{0};
    }
    return meminfo;
}

struct Elicense {
    u64 elicense_chunk1;
    u64 elicense_chunk2;
    u64 title_id; /* title id of the content */
    u64 unk1; /* looks like another title id, but not sure which */
    u64 license_holder_id;
    u64 license_holder_id_again;
    u64 unk2; /* always 0x7fffffffffffffff */
    u64 unk3; /* always 0x7fffffffffffffff */
    u64 unk4; /* always 0x0000000000000000 */
    u64 unk5; /* some version string or timestamp(but not unix) or flags about the elicense? */
    u64 unk6; /* always 0x0000000000000000 */
    u64 title_id_base; /* title id of the main game */
};

Elicense getLicenseFromTitleId(u64 titleId)
{
    Elicense elicense;
    // debug es service
    Debugger debugger(0x0100000000000033);
    if(R_FAILED(debugger.attachToProcess()))
    {
        printf("Failed to attach to es\n");
        return elicense;
    }
    else
    {
        printf("Attached to es\n");
    }

    MemoryInfo mainMemInfo = query_find_main(debugger, 0, 1000);
    u64 offsetElicenseCount = 0x11CE50; // subject to change with system updates
    u64 offsetElicenses = 0x11CE60; // subject to change with system updates
    u32 ElicenseCount = 0;
    debugger.readMemory(&ElicenseCount, sizeof(ElicenseCount), mainMemInfo.addr + offsetElicenseCount);
    for (u32 i = 0; i < ElicenseCount; i++)
    {
        debugger.readMemory(&elicense, sizeof(Elicense), mainMemInfo.addr + offsetElicenses + (sizeof(Elicense) * i));
        if (elicense.title_id == titleId)
        {
            break;
        }
    }

    return elicense;
}

int main(int argc, char **argv)
{
    consoleInit(NULL);
    socketInitializeDefault();
    nxlinkStdio();
    // redirect stdout & stderr over network to nxlink

    // Configure our supported input layout: a single player with standard controller styles
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    // Initialize the default gamepad (which reads handheld mode inputs as well as the first connected controller)
    PadState pad;
    padInitializeDefault(&pad);

    // get elicense of Animal Crossing New Horizons
    Elicense elicense = getLicenseFromTitleId(0x01006F8002326000);

    printf("elicense: %08lx%08lx\n", elicense.elicense_chunk1, elicense.elicense_chunk2);
    printf("licensee: %08lx (verify %08lx)\n", elicense.license_holder_id, elicense.license_holder_id_again);

    // Main loop
    while(appletMainLoop())
    {
        // Scan the gamepad. This should be done once for each frame
        padUpdate(&pad);

        // padGetButtonsDown returns the set of buttons that have been newly pressed in this frame compared to the previous one
        u64 kDown = padGetButtonsDown(&pad);

        if (kDown & HidNpadButton_Plus) break; // break in order to return to hbmenu

        consoleUpdate(NULL);
    }

    socketExit();
    consoleExit(NULL);
    return 0;
}
