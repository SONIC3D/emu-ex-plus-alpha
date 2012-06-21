/* FCE Ultra - NES/Famicom Emulator
 *
 * Copyright notice for this file:
 *  Copyright (C) 2006 CaH4e3
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "mapinc.h"

namespace Board15
{

static uint16 latchea;
static uint8 latched;
static uint8 *WRAM=NULL;
static uint32 WRAMSIZE;
static SFORMAT StateRegs[]=
{
  {&latchea, 2, "LATCHEA"},
  {&latched, 1, "LATCHED"},
  {0}
};

static void Sync(void)
{
  int i;
  setmirror(((latched>>6)&1)^1);
  switch(latchea)
  {
    case 0x8000:
      for(i=0;i<4;i++)
        setprg8(0x8000+(i<<13),(((latched&0x7F)<<1)+i)^(latched>>7));
      break;
    case 0x8002:
      for(i=0;i<4;i++)
        setprg8(0x8000+(i<<13),((latched&0x7F)<<1)+(latched>>7));
      break;
    case 0x8001:
    case 0x8003:
      for(i=0;i<4;i++)
      {
        unsigned int b;
        b=latched&0x7F;
        if(i>=2 && !(latchea&0x2))
          i=0x7F;
        setprg8(0x8000+(i<<13),(i&1)+((b<<1)^(latched>>7)));
      }
      break;
  }
}

static DECLFW(M15Write)
{
  latchea=A;
  latched=V;
  Sync();
}

static void StateRestore(int version)
{
  Sync();
}

static void M15Power(void)
{
  latchea=0x8000;
  latched=0;
  setchr8(0);
  setprg8r(0x10,0x6000,0);
  SetReadHandler(0x6000,0x7FFF,CartBR);
  SetWriteHandler(0x6000,0x7FFF,CartBW);
  SetWriteHandler(0x8000,0xFFFF,M15Write);
  SetReadHandler(0x8000,0xFFFF,CartBR);
  Sync();
}

static void M15Reset(void)
{
  latchea=0x8000;
  latched=0;
  Sync();
}

static void M15Close(void)
{
  if(WRAM)
    FCEU_gfree(WRAM);
  WRAM=NULL;
}

}

void Mapper15_Init(CartInfo *info)
{
	using namespace Board15;
  info->Power=M15Power;
  info->Reset=M15Reset;
  info->Close=M15Close;
  GameStateRestore=Board15::StateRestore;
  Board15::WRAMSIZE=8192;
  Board15::WRAM=(uint8*)FCEU_gmalloc(Board15::WRAMSIZE);
  SetupCartPRGMapping(0x10,Board15::WRAM,Board15::WRAMSIZE,1);
  if(info->battery)
  {
    info->SaveGame[0]=Board15::WRAM;
    info->SaveGameLen[0]=Board15::WRAMSIZE;
  }
  AddExState(Board15::WRAM, Board15::WRAMSIZE, 0, "WRAM");
  AddExState(&Board15::StateRegs, ~0, 0, 0);
}
