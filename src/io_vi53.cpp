/*
 *  Emu80/SDL 3.x
 *  Copyright (C) 1997-2013 Viktor Pykhonin <pyk@mail.ru>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// io_vi53.cpp

// ���������� ������������� ������� ��580��53

#include <stdint.h>
#include <stdlib.h>
//#include "sdllayer.h"
#include "emu80.h"
#include "asm.h"
#include "emuinit.h"
#include "io_vi53.h"

const uint8_t bOneChLevel=0x1F;

uint16_t wPartnerCnt=0;
const uint8_t wPartnerInitCnt=28;
uint8_t bPartnerOutput=0;

TTimer8253::TTimer8253(TPITType ptPITType)
{
    InitDevice();
    ptType=ptPITType;
}

void TTimer8253::SetPITType(TPITType ptPITType)
{
    ptType=ptPITType;
}

void TTimer8253::StartCount(int nCnt)
{
    wPITCounter[nCnt]=wPITInitCnt[nCnt];
    switch (bPITMode[nCnt])
    {
    case 3:
        bPITOutput[nCnt]=1;
        break;
    case 0:
        bPITOutput[nCnt]=0;
        break;
    }
    return;
}

void TTimer8253::InitDevice()
{
    for(int i=0;i<3;i++)
    {
        wPITCounter[i]=0;
        wPITInitCnt[i]=0;
        wPITLatch[i]=0;
        bPITMode[i]=0;//3;
        bPITState[i]=stStopped;
        bPITLoadMode[i]=rlLatch;
        bPITRState[i]=rwstWaitLow;
        bPITWState[i]=rwstWaitLow;
        bPITCntLatched[i]=0;
        bPITCountBCD[i]=0;
        bPITOutput[i]=1;
    }
    bPITCSW[0]=0x10;//0x36;
    bPITCSW[1]=0x50;//0x76;
    bPITCSW[2]=0x90;//0xB6;
/*    if (cModel==MODEL_R) //!!!
    {
        bPITCSW[2]=0xB0;
        bPITMode[2]=0;
    }*/
}

void TTimer8253::WriteReg(uint16_t wReg, uint8_t bValue)
{
    wReg&=0x3;
    if (wReg==0x03) // CSW
    {
        int nCnt=(bValue&0xc0)>>6;
        if (nCnt==3) return;
        uint8_t bLoadMode=(bValue&0x30)>>4;
        uint8_t bMode=(bValue&0x0E)>>1;
        if (bMode==7) bMode=3;
        if (bLoadMode==rlLatch) // ������� ������������
        {
            wPITLatch[nCnt]=wPITCounter[nCnt];
            bPITCntLatched[nCnt]=1;
            bPITRState[nCnt]=rwstWaitLow;
        }
        else // ��������� ������ ��������
        {
            bPITCSW[nCnt]=bValue;
            bPITMode[nCnt]=bMode;
            bPITLoadMode[nCnt]=bLoadMode;
            bPITCountBCD[nCnt]=bValue&0x01;
            bPITCntLatched[nCnt]=0;
            bPITState[nCnt]=stStopped;
            bPITWState[nCnt]=(bLoadMode==rlHighByte)?rwstWaitHigh:rwstWaitLow;
            bPITRState[nCnt]=rwstWaitLow;
            wPITCounter[nCnt]=0;
            wPITInitCnt[nCnt]=0;
            if (bMode==0)
                bPITOutput[nCnt]=0;
        }
    }
    else // ������� ��������
    {
        if (bPITWState[wReg]==rwstWaitLow)
        {
            wPITInitCnt[wReg]=(wPITInitCnt[wReg]&0xFF00)|bValue;
            if (bPITLoadMode[wReg]==rlWord) // �������� �����, ��������� ������� ����
                {
                    bPITWState[wReg]=rwstWaitHigh;
                    if (bPITMode[wReg]==0)
                    {
                        bPITState[wReg]=stStopped;
                        bPITOutput[wReg]=1;
                    }
                }
            else // �������� ������ �������� �����, ��� � ���������
            {
                if (bPITState[wReg]==stStopped || bPITMode[wReg]==0)
                {
                    bPITState[wReg]=stWorking;
                    StartCount(wReg);
                }
            }
        }
        else // rwstWaitLow - �������� �������� �����
        {
            wPITInitCnt[wReg]=(wPITInitCnt[wReg]&0xFF)|(bValue<<8);
            if (bPITLoadMode[wReg]==rlWord) // �������� �����, ��������� ��������� - ������� ����
                bPITWState[wReg]=rwstWaitLow;
            if (bPITState[wReg]==stStopped || bPITMode[wReg]==0)
            {
                bPITState[wReg]=stWorking;
                StartCount(wReg);
            }
        }
    }
}

uint8_t TTimer8253::ReadReg(uint16_t wReg)
{
    wReg&=0x03;
    if (wReg==3)
//        return bPITWState[2];
        return 0xFF;
    if (bPITRState[wReg]==rwstWaitLow)
    {
        bPITRState[wReg]=rwstWaitHigh;
        if (bPITCntLatched[wReg])
            return bPITCntLatched[wReg]&0xFF;
        else
            return wPITCounter[wReg]&0xFF;
    }
    else // PICCNT_WAITHI
    {
        bPITRState[wReg]=rwstWaitLow;
        if (bPITCntLatched[wReg])
        {
            bPITCntLatched[wReg]=0;
            return (bPITCntLatched[wReg]&0xFF00)>>8;
        }
        else
            return (wPITCounter[wReg]&0xFF00)>>8;
    }
}

void TTimer8253::ProcessTime(int nTicks)
{
    int nPulses[3]; // ���������� ��������� ����� 0
    int nTicksCnt;

    for(int i=0;i<3;i++)
    {
        if (bPITState[i]==stWorking)
        {
            if (ptType==ptRK86 && i==2) //!!!
            {
                if (wPITCounter[2]<nPulses[1])
                {
                    //wPITCounter[i]=0;
                    wPITCounter[i]-=(uint16_t)nPulses[1];
                    bPITOutput[i]=1;
                    nPulses[i]=0;
                }
                else
                   wPITCounter[2]=wPITCounter[2]-nPulses[1];
            }
            else switch (bPITMode[i])
            {
            case 0:
                if (wPITCounter[i]<nTicks)
                {
                    //wPITCounter[i]=0;
                    wPITCounter[i]-=(uint16_t)nTicks;
                    bPITOutput[i]=1;
                    bPITState[i]=stStopped;
                    nPulses[i]=0;
                }
                else
                   wPITCounter[i]=wPITCounter[i]-nTicks;
                break;
            case 3:
                nPulses[i]=0;
                if (wPITInitCnt[i]!=0)
                    nPulses[i]=(wPITInitCnt[i]+nTicks-wPITCounter[i])/wPITInitCnt[i];
                if (wPITInitCnt[i]<3)  // ���������� �� ������������� ;)
                    break;
                nTicksCnt=nTicks % (long)wPITInitCnt[i];
                if ((int)wPITCounter[i]<nTicksCnt)
                    wPITCounter[i]=(uint16_t)((int)wPITCounter[i]+(int)wPITInitCnt[i]-nTicksCnt);
                else
                    wPITCounter[i]=(uint16_t)((int)wPITCounter[i]-nTicksCnt);
                bPITOutput[i]=wPITCounter[i]<wPITInitCnt[i]/2?0:1;
                break;
            default:
              wPITCounter[i]-=(uint16_t)nTicks;
            }
        }
        else
        {
            if (ptType==ptRK86 && i==2) //!!!
                wPITCounter[i]-=(uint16_t)nPulses[1];
            else
              wPITCounter[i]-=(uint16_t)nTicks;

        }
    }
    if (cModel==MODEL_P) //!!! ������� � TPlatform
    {
        nTicksCnt=nTicks%wPartnerInitCnt;
        if ((int)wPartnerCnt<nTicksCnt)
            wPartnerCnt=(uint16_t)((int)wPartnerCnt+(int)wPartnerInitCnt-nTicksCnt);
        else
            wPartnerCnt=(uint16_t)((int)wPartnerCnt-nTicksCnt);
        bPartnerOutput=wPartnerCnt<((long)wPartnerInitCnt*2/5)?0:1; //
    }
}

uint8_t TTimer8253::GetOutput(uint8_t bCnt)
{
    return bPITOutput[bCnt];
}

uint8_t TTimer8253::GetSample() //!!! ������� � TPlatform
{
    uint8_t bRes=0;
    switch (cModel) //!!!
    {
    case MODEL_A:
        for (int i=0;i<3;i++)
            bRes+=bPITOutput[i]?bOneChLevel:0;
        bRes+=port_c&0x1?bOneChLevel:0;
        break;
    case MODEL_M:
        bRes+=(bPITOutput[2]&&port_c&0x2/*&&port_c&0x4*/)?bOneChLevel:0; // ����������� � Gate2 !!!
        bRes+=port_c&0x1?bOneChLevel:0;
        break;
    case MODEL_R:
        bRes+=bPITOutput[0]&&(!bPITOutput[2])?bOneChLevel:0;
        bRes+=port_c&0x1?bOneChLevel:0;
        bRes+=int_flag?bOneChLevel:0;
        break;
    case MODEL_P:
        bRes+=bPartnerOutput&&!(port_c&0x2)?bOneChLevel:0;
        bRes+=port_c&0x1?bOneChLevel:0;
        break;
    case MODEL_O:
        bRes+=port_c&0x1?bOneChLevel:0;
        bRes+=int_flag?bOneChLevel:0;
        break;
    case MODEL_S:
        bRes+=port_c&0x80?bOneChLevel:0;
        bRes+=snd_state&0x20?bOneChLevel:0;
        break;
    case MODEL_U:
        bRes+=port_c&0x1?bOneChLevel:0;
        break;
    case MODEL_M80:
        bRes+=port_c&0x1?bOneChLevel:0;
        bRes+=int_flag?bOneChLevel:0;
        break;
    }
    return bRes;
}


void TTimer8253::SavePITState(RKSS_PIT_STATE *pitState)
{
    pitState->wPitK0=wPITInitCnt[0];
    pitState->wPitK1=wPITInitCnt[1];
    pitState->wPitK2=wPITInitCnt[2];
    pitState->wPitCnt0=wPITCounter[0];
    pitState->wPitCnt1=wPITCounter[1];
    pitState->wPitCnt2=wPITCounter[2];
    pitState->bPitMode0=bPITCSW[0];
    pitState->bPitMode1=bPITCSW[1];
    pitState->bPitMode2=bPITCSW[2];
    pitState->bPitLd0=bPITState[0]==stWorking;
    pitState->bPitLd1=bPITState[1]==stWorking;
    pitState->bPitLd2=bPITState[2]==stWorking;
}

void TTimer8253::LoadPITState(RKSS_PIT_STATE *pitState)
{
    bPITCSW[0]=pitState->bPitMode0;
    bPITCSW[1]=pitState->bPitMode1;
    bPITCSW[2]=pitState->bPitMode2;
    for (int i=0;i<3;i++)
        WritePITReg(3, bPITCSW[i]);
    wPITInitCnt[0]=pitState->wPitK0;
    wPITInitCnt[1]=pitState->wPitK1;
    wPITInitCnt[2]=pitState->wPitK2;
    wPITCounter[0]=pitState->wPitCnt0;
    wPITCounter[1]=pitState->wPitCnt1;
    wPITCounter[2]=pitState->wPitCnt2;
    if (pitState->bPitLd0)
    {
        bPITState[0]=stWorking;
        StartCount(0);
    }
    if (pitState->bPitLd1)
    {
        bPITState[1]=stWorking;
        StartCount(1);
    }
    if (pitState->bPitLd2)
    {
        bPITState[2]=stWorking;
        StartCount(2);
    }
}

// --- Temporary Stubs for ASM ---------------------------------------------

static class TTimer8253 *PTimer = NULL;

void InitPIT()
{
    if (!PTimer)
        PTimer = new TTimer8253(cModel!=MODEL_R?ptGeneral:ptRK86);
    else
    {
        PTimer->InitDevice();
        PTimer->SetPITType(cModel!=MODEL_R?ptGeneral:ptRK86);
    }
}

void StartCount(int nCnt)
{
    PTimer->StartCount(nCnt);
}

void WritePITReg(uint16_t wReg, uint8_t bValue)
{
    PTimer->WriteReg(wReg, bValue);
}

uint8_t ReadPITReg(uint16_t wReg)
{
    return PTimer->ReadReg(wReg);
}

void ProcessTime(long lTicks)
{
    PTimer->ProcessTime(lTicks);
}

//uint8_t TTimer8253.GetOutput(uint8_t bCnt)

uint8_t GetSample()
{
    return PTimer->GetSample();
}

void LoadPITState(RKSS_PIT_STATE *pitState)
{
    PTimer->LoadPITState(pitState);
}

void SavePITState(RKSS_PIT_STATE *pitState)
{
    PTimer->SavePITState(pitState);
}
