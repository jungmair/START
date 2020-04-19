#ifndef REWIRING_COMMUNICATION_H
#define REWIRING_COMMUNICATION_H

#include <linux/ioctl.h>

//https://github.com/jungmair/rewiring-lkm/blob/master/module/inc/communication.h

// header file that defines shared types for both, C++ libraries and kernel module
//commands
enum cmd_types{ GET_PAGE_IDS,SET_PAGE_IDS,CREATE_PAGE_IDS};

//two special page ids
#define PAGEID_UNASSIGNED 0xffffffffu
#define PAGEID_OFFSET_INVALID 0xfffffffeu

//struct for specifying rewiring commands
struct cmd {
    enum cmd_types type;
    unsigned long start;
    unsigned long len;
    void* mapping_start;
    void* payload;
};
//assemble a 'valid' ioctl command for communication
#define REW_CMD _IOC(IOC_INOUT,'k',0u,1u)

#endif //REWIRING_COMMUNICATION_H
