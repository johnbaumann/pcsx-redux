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
    SIO1_STAT |= SR_IRQ;
    psxHu32ref(0x1070) |= SWAP_LEu32(IRQ8_SIO);
    if (fifo_rx.BytesAvailable() > 1) scheduleInterrupt(SIO1_CYCLES);
}

uint8_t PCSX::SIO1::readData8() {
    uint8_t ret = 0;

    UpdateStat();
    psxHu8(0x1050) = ret;
    if (SIO1_STAT & SR_RXRDY) {
        ret = fifo_rx.Pull();
        SIO1_DATA = ret;
    }
    UpdateStat();

    return ret;
}

uint8_t PCSX::SIO1::readStat8() {
    UpdateStat();
    return SIO1_STAT & 0xFF;
}

uint16_t PCSX::SIO1::readStat16() {
    UpdateStat();
    return SIO1_STAT & 0xFFFF;
}

uint32_t PCSX::SIO1::readStat32() {
    UpdateStat();
    return SIO1_STAT;
}

void PCSX::SIO1::receiveCallback() {
    bool do_interrupt = false;

    UpdateStat();

    if (SIO1_CTRL & CR_RXIRQEN) {
        if (!(SIO1_STAT & SR_IRQ)) {
            switch ((SIO1_CTRL & 0x300) >> 8) {
                case 0:
                    if (fifo_rx.BytesAvailable() >= 1) do_interrupt = true;
                    break;

                case 1:
                    if (fifo_rx.BytesAvailable() >= 2) do_interrupt = true;
                    break;

                case 2:
                    if (fifo_rx.BytesAvailable() >= 4) do_interrupt = true;
                    break;

                case 3:
                    if (fifo_rx.BytesAvailable() >= 8) do_interrupt = true;
                    break;
            }

            if (do_interrupt) {
                scheduleInterrupt(SIO1_CYCLES);
                SIO1_STAT |= SR_IRQ;
            }
        }
    }
}

void PCSX::SIO1::TransmitData() {
    PCSX::g_emulator->m_sio1Server->write(SIO1_DATA);
    if (SIO1_CTRL & CR_TXIRQEN) {
        if (SIO1_STAT & SR_TXRDY || SIO1_STAT & SR_TXRDY2) {
            if (!(SIO1_STAT & SR_IRQ)) {
                scheduleInterrupt(SIO1_CYCLES);
                SIO1_STAT |= SWAP_LEu32(SR_IRQ);
            }
        }
    }
}

bool PCSX::SIO1::TransmitReady() { return (SIO1_CTRL & CR_TXEN) && (SIO1_STAT & SR_CTS) && (SIO1_STAT & SR_TXRDY2); }

void PCSX::SIO1::UpdateFIFO() {
    // Grab incoming bytes and stash them in fifo
    // 
    // dirty hack, nops sends more than 8 bytes at a time so make sure not to overflow fifo
    // this prevents implementing STAT.4 RX FIFO Overrun
    while (!m_slices.m_sliceQueueRX.empty() && fifo_rx.BytesAvailable() < 8) {
        fifo_rx.Push(m_slices.getByte());
    }
}

void PCSX::SIO1::UpdateStat() {
    UpdateFIFO(); // dirty hack. slices can be > fifo size, so need to check for more data

    if (fifo_rx.BytesAvailable() > 0) {
        SIO1_STAT |= SR_RXRDY;
    } else {
        SIO1_STAT &= ~SR_RXRDY;
    }

    psxHu32ref(0x1054) = SWAP_LEu32(SIO1_STAT);
}
void PCSX::SIO1::writeBaud16(uint16_t v) { SIO1_BAUD = v; }

void PCSX::SIO1::writeCtrl16(uint16_t v) {
    uint16_t old_ctrl = SIO1_CTRL;
    SIO1_CTRL = v;
    if (!(old_ctrl & CR_TXEN) && (SIO1_CTRL & CR_TXEN)) {
        if (TransmitReady()) {
            TransmitData();
        }
    }

    if (SIO1_CTRL & CR_ACK) {
        SIO1_CTRL &= ~CR_ACK;
        SIO1_STAT &= ~(SR_PARITYERR | SR_RXOVERRUN | SR_FRAMINGERR | SR_IRQ);
    }

    if (SIO1_CTRL & CR_RESET) {
        SIO1_STAT &= ~SR_IRQ;
        SIO1_STAT |= (SR_TXRDY | SR_TXRDY2);
        SIO1_MODE = 0;
        SIO1_CTRL = 0;
        SIO1_BAUD = 0;

        PCSX::g_emulator->m_psxCpu->m_psxRegs.interrupt &= ~(1 << PCSX::PSXINT_SIO1);
    }
}

void PCSX::SIO1::writeData8(uint8_t v) {
    SIO1_DATA = v;

    if (TransmitReady()) {
        TransmitData();
    }
}

void PCSX::SIO1::writeMode8(uint8_t v) { SIO1_MODE = v; }

void PCSX::SIO1::writeMode16(uint16_t v) { SIO1_MODE = v; }

void PCSX::SIO1::writeStat8(uint8_t v) {
    SIO1_STAT = v;
    if (TransmitReady()) {
        TransmitData();
    }
}

void PCSX::SIO1::writeStat16(uint16_t v) {
    SIO1_STAT = v;
    if (TransmitReady()) {
        TransmitData();
    }
}

void PCSX::SIO1::writeStat32(uint32_t v) {
    SIO1_STAT = v;
    if (TransmitReady()) {
        TransmitData();
    }
}
