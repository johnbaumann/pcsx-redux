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

#include "gui/widgets/sio1.h"

#include "core/sio1.h"
#include "gui/gui.h"
#include "imgui.h"
#include "imgui_stdlib.h"

void PCSX::Widgets::SIO1::draw(GUI* gui, sio1Registers* registers, const char* title) {
    ImGui::SetNextWindowPos(ImVec2(1040, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(210, 512), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(title, &m_show)) {
        ImGui::End();
        return;
    }

    {
        {
            ImGui::Text("Status Register 0x%08x", registers->status);
            ImGui::Columns(2);
            {
                ImGui::Text(
                    "TX Ready Flag 1:\nRX FIFO Not Empty:\nTX Ready Flag 2:\nRX Parity Error:\nRX FIFO Overrun:\nRX "
                    "Bad Stop Bit:\n");
                ImGui::SameLine();
                ImGui::Text("%i\n%i\n%i\n%i\n%i\n%i\n", registers->status >> 0 & 1, registers->status >> 1 & 1,
                            registers->status >> 2 & 1, registers->status >> 3 & 1, registers->status >> 4 & 1,
                            registers->status >> 5 & 1);
            }
            ImGui::NextColumn();
            {
                ImGui::Text(
                    "RX Input Level:\nDSR Input Level:\nCTS Input Level:\nInterrupt Request:\nUnknown\nBaudrate "
                    "Timer:\n");
                ImGui::SameLine();
                ImGui::Text("%i\n%i\n%i\n%i\n%i\n%i\n", registers->status >> 6 & 1, registers->status >> 7 & 1,
                            registers->status >> 8 & 1, registers->status >> 9 & 1, registers->status >> 10 & 1,
                            registers->status >> 11 & 14);
            }
        }

        ImGui::Columns(1);
        ImGui::Separator();

        {
            ImGui::Text("Mode Register 0x%04x", registers->mode);
            ImGui::Columns(2);
            {
                ImGui::Text("Baudrate Reload Factor\nCharacter Length\nParity Enable\n");
                ImGui::SameLine();
                ImGui::Text("%i\n%i\n%i\n", registers->mode >> 0 & 1, registers->mode >> 1 & 3,
                            registers->mode >> 4 & 1);
            }
            ImGui::NextColumn();
            {
                ImGui::Text("Parity Type\nStop bit length\n");
                ImGui::SameLine();
                ImGui::Text("%i\n%i\n", registers->mode >> 5 & 1, registers->mode >> 6 & 3);
            }
        }

        ImGui::Columns(1);
        ImGui::Separator();

        {
            ImGui::Text("Control Register 0x%04x", registers->control);
            ImGui::Columns(2);
            {
                ImGui::Text("TX Enable:\nDTR:\nRXEN\nTX Out\nACK:\nRTS Out:\n");
                ImGui::SameLine();
                ImGui::Text("%i\n%i\n%i\n%i\n%i\n%i\n", registers->control >> 0 & 1, registers->control >> 1 & 1,
                            registers->control >> 2 & 1, registers->control >> 3 & 1, registers->control >> 4 & 1,
                            registers->control >> 5 & 1);
            }
            ImGui::NextColumn();
            {
                ImGui::Text("Reset:\n?:\nIRQ Mode:\nTX IRQ EN:\nRX IRQ EN:\nDSR IRQ EN:");
                ImGui::SameLine();
                ImGui::Text("%i\n%i\n%i\n%i\n%i\n%i\n", registers->control >> 6 & 1, registers->control >> 7 & 1,
                            registers->control >> 8 & 3, registers->control >> 10 & 1, registers->control >> 11 & 1,
                            registers->control >> 12 & 1);
            }
        }
    }
}
