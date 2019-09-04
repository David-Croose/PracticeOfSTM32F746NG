#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "ff.h"
#include "test.h"

#define ERROR_PRINT()       goto error;

static int32_t filesystem_init(void) {
    static FATFS FatFs;   /* Work area (filesystem object) for logical drive */
    /// static BYTE work[FF_MAX_SS];
    FRESULT res;

    /// if ((res = f_mkfs("0:/", FM_ANY, 0, work, sizeof(work))) != FR_OK) {
    ///     /// printf("mkfs error! res=%d\n", res);
    ///     return 1;
    /// }
    /// printf("mkfs successfully\n");

    if ((res = f_mount(&FatFs, "0:", 1)) != FR_OK) {
        /// printf("mount error! res=%d\n", res);
        return 1;
    }
    /// printf("mount successfully\n");
    return 0;
}

static int32_t create_file(const char *name) {
    FIL fil;

    if (f_open(&fil, name, FA_OPEN_ALWAYS)) {
        return 1;
    }
    f_close(&fil); 
    return 0;
}

static int32_t create_folder(const char *name) {
    if (f_mkdir(name)) {
        return 1;
    }
    return 0;
}

static int32_t write_file(const char *name, const void *p, uint32_t len) {
    FIL fil;
    uint32_t bw;

    if (f_open(&fil, name, FA_WRITE)) {
        return 1;
    }
    if (f_write(&fil, p, len, &bw)) {
        f_close(&fil);
        return 1;
    }
    f_close(&fil);
    return 0;
}

static int32_t read_file(const char *name, void *p, uint32_t len) {
    FIL fil;
    uint32_t br;

    if (f_open(&fil, name, FA_READ)) {
        return 1;
    }
    if (f_read(&fil, p, len, &br)) {
        f_close(&fil);
        return 1;
    }
    f_close(&fil);
    return 0; 
}

//static int32_t delete_folder(const char *name) {
//    DIR dir;
//    FILINFO fno;
//    uint32_t i;

//    if (f_opendir(&dir, name)) {
//        return 1;
//    }

//    for (i = 0; i < 50; i++) {
//        if (f_readdir(&dir, &fno) || fno.fname[0] == 0) {
//            continue;
//        }

//        if (f_unlink(fno.fname)) {
//            goto error;
//        }
//    }

//    return 0;

//error:
//    f_closedir(&dir);
//    return 1;
//}

//static int32_t delete_file(const char *name) {
//    if (f_unlink(name)) {
//        return 1;
//    } 
//    return 0; 
//}

// return: 1 --- exist
//         0 --- not exist
static int32_t is_file_exist(const char *name) {
    return f_stat(name, NULL) == 0;
}

static int32_t verify_buf(const char *p1, const char *p2) {
    return strcmp(p1, p2); 
}

int test_main(void) {
    uint32_t i;
    uint32_t max = 90;
    char name[100];
    char *rbuf = NULL;
    int32_t rbuf_len = sizeof(testbuf);
 
    if (filesystem_init()) {
        ERROR_PRINT();
    }

    /// printf("start...\n");
    if ((rbuf = malloc(rbuf_len)) == NULL) {
        ERROR_PRINT();
    }

    if (!is_file_exist("/data")) {
        if (create_folder("/data")) {
            ERROR_PRINT();
        }
    }
 
    for (i = 1; i <= max; i++) {
        memset(name, 0, sizeof(name));
        snprintf(name, sizeof(name), "0:/data/%014d", i);
        if (create_folder(name)) {
            ERROR_PRINT();
        }

        memset(name, 0, sizeof(name));
        snprintf(name, sizeof(name), "0:/data/%014d/CIM_ECG_PARM_WW.txt", i);
        if (create_file(name)) {
            ERROR_PRINT();
        }   

        if (write_file(name, testbuf, sizeof(testbuf))) {
            ERROR_PRINT();
        }

        if (read_file(name, rbuf, rbuf_len)) {
            ERROR_PRINT();
        }
        rbuf[rbuf_len - 1] = 0;

        if (verify_buf(testbuf, rbuf)) {
            ERROR_PRINT();
        }
    }

    free(rbuf);
    /// printf("OK\n");
    return 0;

error:
    if (rbuf) {
        free(rbuf);
    }
    /// printf("failure\n");
    return 1; 
}

