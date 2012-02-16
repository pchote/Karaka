//***************************************************************************
//
//  File        : download.h
//  Copyright   : 2009-2012 Johnny McClymont, Paul Chote
//  Description : Triggers camera frame downloads by changing the level of PA0
//
//  This file is part of Karaka, which is free software. It is made available
//  to you under the terms of version 3 of the GNU General Public License, as
//  published by the Free Software Foundation. For more information, see LICENSE.
//
//***************************************************************************

#ifndef KARAKA_DOWNLOAD_H
#define KARAKA_DOWNLOAD_H

void download_init_hardware();
void trigger_download();

#endif