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
    SIO1_LOG("SIO1 Interrupt (CP0.Status = %x)\n", PCSX::g_emulator->m_cpu->m_regs.CP0.n.Status);
    m_statusReg |= SR_IRQ;
    psxHu32ref(0x1070) |= SWAP_LEu32(IRQ8_SIO);
    if (fifo_rx.bytesAvailable() > 1) scheduleInterrupt(SIO1_CYCLES);
}

uint8_t PCSX::SIO1::readData8() {
    updateStat();
    if (m_statusReg & SR_RXRDY) {
        m_dataReg = fifo_rx.pull();
        psxHu8(0x1050) = m_dataReg;
    }
    updateStat();

    return m_dataReg;
}

uint8_t PCSX::SIO1::readStat8() {
    updateStat();
    return m_statusReg & 0xFF;
}

uint16_t PCSX::SIO1::readStat16() {
    updateStat();
    return m_statusReg & 0xFFFF;
}

uint32_t PCSX::SIO1::readStat32() {
    updateStat();
    return m_statusReg;
}

void PCSX::SIO1::receiveCallback() {
    bool do_interrupt = false;

    updateStat();

    if (m_ctrlReg & CR_RXIRQEN) {
        if (!(m_statusReg & SR_IRQ)) {
            switch ((m_ctrlReg & 0x300) >> 8) {
                case 0:
                    if (fifo_rx.bytesAvailable() >= 1) do_interrupt = true;
                    break;

                case 1:
                    if (fifo_rx.bytesAvailable() >= 2) do_interrupt = true;
                    break;

                case 2:
                    if (fifo_rx.bytesAvailable() >= 4) do_interrupt = true;
                    break;

                case 3:
                    if (fifo_rx.bytesAvailable() >= 8) do_interrupt = true;
                    break;
            }

            if (do_interrupt) {
                scheduleInterrupt(SIO1_CYCLES);
                m_statusReg |= SR_IRQ;
            }
        }
    }
}

void PCSX::SIO1::transmitData() {
    PCSX::g_emulator->m_sio1Server->write(m_dataReg);
    if (m_ctrlReg & CR_TXIRQEN) {
        if (m_statusReg & SR_TXRDY || m_statusReg & SR_TXRDY2) {
            if (!(m_statusReg & SR_IRQ)) {
                scheduleInterrupt(SIO1_CYCLES);
                m_statusReg |= SWAP_LEu32(SR_IRQ);
            }
        }
    }
}

bool PCSX::SIO1::isTransmitReady() { return (m_ctrlReg & CR_TXEN) && (m_statusReg & SR_CTS) && (m_statusReg & SR_TXRDY2); }

void PCSX::SIO1::updateFIFO() {
    // Grab incoming bytes and stash them in fifo
    // 
    // dirty hack, nops sends more than 8 bytes at a time so make sure not to overflow fifo
    // this prevents implementing STAT.4 RX FIFO Overrun
    while (!m_slices.m_sliceQueueRX.empty() && fifo_rx.bytesAvailable() < 8) {
        fifo_rx.push(m_slices.getByte());
    }
}

void PCSX::SIO1::updateStat() {
    updateFIFO(); // dirty hack. slices can be > fifo size, so need to check for more data

    if (fifo_rx.bytesAvailable() > 0) {
        m_statusReg |= SR_RXRDY;
    } else {
        m_statusReg &= ~SR_RXRDY;
    }

    psxHu32ref(0x1054) = SWAP_LEu32(m_statusReg);
}
void PCSX::SIO1::writeBaud16(uint16_t v) {
    m_baudReg = v;
    psxHu8ref(0x105E) = m_baudReg;
}

void PCSX::SIO1::writeCtrl16(uint16_t v) {
    uint16_t old_ctrl = m_ctrlReg;
    m_ctrlReg = v;
    if (!(old_ctrl & CR_TXEN) && (m_ctrlReg & CR_TXEN)) {
        if (isTransmitReady()) {
            transmitData();
        }
    }

    if (m_ctrlReg & CR_ACK) {
        m_ctrlReg &= ~CR_ACK;
        m_statusReg &= ~(SR_PARITYERR | SR_RXOVERRUN | SR_FRAMINGERR | SR_IRQ);
    }

    if (m_ctrlReg & CR_RESET) {
        m_statusReg &= ~SR_IRQ;
        m_statusReg |= (SR_TXRDY | SR_TXRDY2);
        m_modeReg = 0;
        m_ctrlReg = 0;
        m_baudReg = 0;

        PCSX::g_emulator->m_cpu->m_regs.interrupt &= ~(1 << PCSX::PSXINT_SIO1);
    }

    psxHu16ref(0x105A) = SWAP_LE16(m_ctrlReg);
}

void PCSX::SIO1::writeData8(uint8_t v) {
    m_dataReg = v;

    if (isTransmitReady()) {
        transmitData();
    }

    psxHu8ref(0x1050) = m_dataReg;
}

void PCSX::SIO1::writeMode16(uint16_t v) { m_modeReg = v; }

void PCSX::SIO1::writeStat32(uint32_t v) {
    m_statusReg = v;
    if (isTransmitReady()) {
        transmitData();
    }
    psxHu32ref(0x1054) = SWAP_LE32(m_statusReg);

}
