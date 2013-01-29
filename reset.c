//***************************************************************************
//
//  File        : reset.c
//  Copyright   : 2013 Paul Chote
//  Description : Toggles the DTR output on the USB chip to reset into the bootloader
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#include <stdio.h>
#ifdef _WIN32
#   include <windows.h>
#else
#   include <fcntl.h>
#   include <termios.h>
#   include <unistd.h>
#   include <sys/ioctl.h>
#endif

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("Usage: reset <path to port>\n");
        return 1;
    }

#ifdef _WIN32
    HANDLE port = CreateFile(argv[1], GENERIC_READ | GENERIC_WRITE, 0, 0, 
                             OPEN_EXISTING, 0, 0);
    if (port == INVALID_HANDLE_VALUE)
    {
        printf("Failed to open port: %s\n", argv[1]);
        return 1;
    }

    EscapeCommFunction(port, SETDTR);
    Sleep(1000);
    EscapeCommFunction(port, CLRDTR);
    CloseHandle(port);
#else
    int port = open(argv[1], O_RDWR | O_NOCTTY | O_NDELAY);
    if (port == -1)
    {
        printf("Failed to open port: %s\n", argv[1]);
        return 1;
    }

    ioctl(port, TIOCMSET, &(int){TIOCM_DTR});
    sleep(1);
    ioctl(port, TIOCMSET, &(int){0});
    close(port);
#endif
    return 0;
}