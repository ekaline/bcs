#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

/* The fbcapture driver implements a /proc interface for read/write the pci pre/post cursor for V7 cards.
   and keeps a copy of values written.
   This function writes to the card specific file to reset the copy to default.
 */
int fbcapture_cursor_reset()
{
    int err = 0;
    int fd = 0;
    int len = 0;
    char card_cursor_file[100];
    char device[100];
    char my_device[20];
    memset(my_device, 0, 20);

    //if no device given use default capture device name
    //otherwise strip /dev
    len = strlen(device);
    if (len  == 0 || len > 20) {
        strcpy(my_device, "fbcard0");
    } else if (strncmp("/dev/", device, 5) == 0) {
        strncpy(my_device, &device[5], len - 5);
    } else {
        strncpy(my_device, device, len);
    }

    //file created by fbcapture driver for V7 cards
    sprintf(card_cursor_file, "/proc/driver/fbcapture/%s/pci/cursor_auto", my_device);
    err = access(card_cursor_file, W_OK);
    if (err == 0) {
        int numberOfBytesWritten;
        //write something to file to reset cursor to default
        fd = open(card_cursor_file, O_WRONLY);
        numberOfBytesWritten = write(fd, "1", 1);
        if (numberOfBytesWritten != 1)
        {
        }
        close(fd);
    } else {
        //printf("err %d, errno %d\n", err, errno);
        if (errno == ENOENT) {
            printf("The fbcapture cursor setting file: %s does not exist, so copy of cursor setting in driver is not reset.\n",
                   card_cursor_file);
        } else if (errno == EACCES) {
            printf("Root access is required to reset fbcapture driver copy of cursor settings\n");
        }
    }
    return 0;
}

