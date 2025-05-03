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

//print debugging information to console (nxlink)
#define DEBUG_PRINTF 1

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

    // debug es service
    Debugger debugger(0x0100000000000033);
    if(R_FAILED(debugger.attachToProcess()))
    {
        printf("Failed to attach to es\n");
        return -1;
    }
    else
    {
        printf("Attached to es\n");
    }

    MemoryInfo mainMemInfo = query_find_main(debugger, 0, 1000);

    u8 buffer[0x10] = { 0 };
    // printf("%08lx %08lx %08lx %08lx\n%08lx %08lx %08lx %08lx\n",
    //     mainMemInfo.addr, mainMemInfo.size, mainMemInfo.type, mainMemInfo.attr, mainMemInfo.perm, mainMemInfo.ipc_refcount, mainMemInfo.device_refcount, mainMemInfo.padding);
    debugger.readMemory(buffer, sizeof(buffer), mainMemInfo.addr + 0x11CE60);
    print_u8_buffer(buffer, sizeof(buffer));

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
