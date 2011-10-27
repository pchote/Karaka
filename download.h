//***************************************************************************
//
//    File........: download.h
//    Description.: Commands the camera to download a frame by pulling the
//                  PA0 pin LOW for 512us. Uses Timer0
//    Copyright...: 2009-2011 Johnny McClymont, Paul Chote
//
//    This file is part of Karaka, which is free software. It is made available
//    to you under the terms of version 3 of the GNU General Public License, as
//    published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#ifndef KARAKA_DOWNLOAD_H
#define KARAKA_DOWNLOAD_H

void download_init(void);
void trigger_download(void);

#endif