/***************************************************************************
 *   Copyright (C) 2022 PCSX-Redux authors                                 *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.           *
 ***************************************************************************/

#include "core/sio1.h"

void PCSX::SIO1::interrupt() {
    SIO1_LOG("SIO1 Interrupt (CP0.Status = %x)\n", PCSX::g_emulator->m_psxCpu->m_psxRegs.CP0.n.Status);
    SIO1_STAT |= SWAP_LEu32(SR_IRQ);
    I_STAT |= SWAP_LEu16(IRQ8_SIO);
}

uint8_t PCSX::SIO1::readData8() {
    uint8_t ret = 0;

    if (SIO1_STAT & SWAP_LEu32(SR_RXRDY)) {
        ret = m_slices.getByte();
        readStat8();
        SIO1_DATA = ret;
    }

    return ret;
}

uint8_t PCSX::SIO1::readStat8() {
    updateStat();
    return SIO1_STAT & 0xFF;
}

uint16_t PCSX::SIO1::readStat16() {
    updateStat();
    return SIO1_STAT & 0xFFFF;
}

uint32_t PCSX::SIO1::readStat32() {
    updateStat();
    return SIO1_STAT;
}

void PCSX::SIO1::receiveCallback() {
    if (SIO1_CTRL & SWAP_LEu16(CR_RXIRQEN)) {
        if (!(SIO1_STAT & SWAP_LEu32(SR_IRQ))) {
            scheduleInterrupt(SIO1_CYCLES);
            SIO1_STAT |= SWAP_LEu32(SR_IRQ);
        }
    }
}

void PCSX::SIO1::updateStat() {
    if (m_slices.m_sliceQueue.empty()) {
        SIO1_STAT &= SWAP_LEu32(~SR_RXRDY);
    } else {
        SIO1_STAT |= SWAP_LEu32(SR_RXRDY);
    }
}

void PCSX::SIO1::writeBaud16(uint16_t v) { SIO1_BAUD = SWAP_LEu16(v); }

void PCSX::SIO1::writeCtrl16(uint16_t v) {
    SIO1_CTRL = v;

    if (SIO1_CTRL & SWAP_LEu16(CR_ACK)) {
        SIO1_CTRL &= SWAP_LEu16(~CR_ACK);
        SIO1_STAT &= SWAP_LEu32(~(SR_PARITYERR | SR_RXOVERRUN | SR_FRAMINGERR | SR_IRQ));
    }

    if (SIO1_CTRL & SWAP_LEu16(CR_RESET)) {
        SIO1_STAT &= SWAP_LEu32(~SR_IRQ);
        SIO1_STAT |= SWAP_LEu32(SR_TXRDY | SR_TXRDY2);
        SIO1_MODE = 0;
        SIO1_CTRL = 0;
        SIO1_BAUD = 0;

        PCSX::g_emulator->m_psxCpu->m_psxRegs.interrupt &= ~(1 << PCSX::PSXINT_SIO1);
    }
}

void PCSX::SIO1::writeData8(uint8_t v) {
    SIO1_DATA = v;
    PCSX::g_emulator->m_sio1Server->write(v);

    if ((SIO1_CTRL & SWAP_LEu16(CR_TXIRQEN)) && (SIO1_STAT & SWAP_LEu32(SR_CTS)) && (SIO1_STAT & SWAP_LEu32(SR_TXRDY2))) {
        if (!(SIO1_STAT & SWAP_LEu32(SR_IRQ))) {
            scheduleInterrupt(SIO1_CYCLES);
            SIO1_STAT |= SWAP_LEu32(SR_IRQ);
        }
    }

    SIO1_STAT |= SWAP_LEu32(SR_TXRDY | SR_TXRDY2);
}

void PCSX::SIO1::writeMode8(uint8_t v) { SIO1_MODE = v; }

void PCSX::SIO1::writeMode16(uint16_t v) { SIO1_MODE = SWAP_LEu16(v); }

void PCSX::SIO1::writeStat8(uint8_t v) { SIO1_STAT = v; }

void PCSX::SIO1::writeStat16(uint16_t v) { SIO1_STAT = SWAP_LEu16(v); }

void PCSX::SIO1::writeStat32(uint32_t v) { SIO1_STAT = SWAP_LEu32(v); }
