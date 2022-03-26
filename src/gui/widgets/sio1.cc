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

static struct registerDisplay {
    const char* bits;
    uint8_t shift;
    uint32_t mask;
    const char* description;
    bool editable;
    const char* notes;
};

static const int kStatusEntries = 13;
static const int kModeEntries = 6;
static const int kControlEntries = 13;

static registerDisplay statusDisplay[kStatusEntries] = {
    {"0", 0, 1, "TX Ready Flag 1", TRUE, "(1=Ready/Started)  (depends on CTS) (TX requires CTS)"},  //
    {"1", 1, 1, "RX FIFO Not Empty", TRUE, "(0=Empty, 1=Not Empty)"},                               //
    {"2", 2, 1, "TX Ready Flag 2", TRUE, "(1=Ready/Finished) (depends on TXEN and on CTS)"},        //
    {"3", 3, 1, "RX Parity Error", TRUE, "(0=No, 1=Error; Wrong Parity, when enabled) (sticky)"},   //
    {"4", 4, 1, "RX FIFO Overrun", TRUE, "(0=No, 1=Error; Received more than 8 bytes) (sticky)"},   //
    {"5", 5, 1, "RX Bad Stop Bit", TRUE, "(0=No, 1=Error; Bad Stop Bit) (when RXEN) (sticky)"},     //
    {"6", 6, 1, "RX Input Level", TRUE, "(0=Normal, 1=Inverted) ;only AFTER receiving Stop Bit"},   //
    {"7", 7, 1, "DSR Input Level", TRUE, "(0=Off, 1=On) (remote DTR) ;DSR not required to be on"},  //
    {"8", 8, 1, "CTS Input Level", TRUE, "(0=Off, 1=On) (remote RTS) ;CTS required for TX"},        //
    {"9", 9, 1, "Interrupt Request", TRUE, "(0=None, 1=IRQ) (sticky)"},                             //
    {"10", 10, 1, "Unknown", TRUE, "(always zero)"},                                                //
    {"11-25", 11, 14, "Baudrate Timer", TRUE, "(15bit timer, decrementing at 33MHz)"},              //
    {"26-31", 26, 5, "Unknown", TRUE, "(usually zero, sometimes all bits set)"}                     //
};

static registerDisplay modeDisplay[kModeEntries] = {
    {"0-1", 0, 3, "Baudrate Reload Factor", TRUE, "(1=MUL1, 2=MUL16, 3=MUL64) (or 0=STOP)"},  //
    {"2-3", 2, 3, "Character Length", TRUE, "(0=5bits, 1=6bits, 2=7bits, 3=8bits)"},          //
    {"4", 4, 1, "Parity Enable", TRUE, "(0=No, 1=Enable)"},                                   //
    {"5", 5, 1, "Parity Type", TRUE, "(0=Even, 1=Odd) (seems to be vice-versa...?)"},         //
    {"6-7", 6, 3, "Stop bit length", TRUE, "(0=Reserved/1bit, 1=1bit, 2=1.5bits, 3=2bits)"},  //
    {"8-15", 8, 7, "Not used", TRUE, "(always zero)"},                                        //
};

static registerDisplay controlDisplay[kControlEntries] = {
    {"0", 0, 1, "TX Enable (TXEN)", TRUE, "(0=Disable, 1=Enable, when CTS=On)"},                       //
    {"1", 1, 1, "DTR Output Level", TRUE, "(0=Off, 1=On)"},                                            //
    {"2", 2, 1, "RX Enable (RXEN)", TRUE, "(0=Disable, 1=Enable)  ;Disable also clears RXFIFO"},       //
    {"3", 3, 1, "TX Output Level", TRUE, "(0=Normal, 1=Inverted, during Inactivity & Stop bits)"},     //
    {"4", 4, 1, "Acknowledge", TRUE, "(0=No change, 1=Reset SIO_STAT.Bits 3,4,5,9) (W)"},              //
    {"5", 5, 1, "RTS Output Level", TRUE, "(0=Off, 1=On)"},                                            //
    {"6", 6, 1, "Reset", TRUE, "(0=No change, 1=Reset most SIO_registers to zero) (W)"},               //
    {"7", 7, 1, "Unknown?", TRUE, "(read/write-able when FACTOR non-zero) (otherwise always zero)"},   //
    {"8-9", 8, 3, "RX Interrupt Mode", TRUE, "(0..3 = IRQ when RX FIFO contains 1,2,4,8 bytes)"},      //
    {"10", 10, 1, "TX Interrupt Enable", TRUE, "(0=Disable, 1=Enable) ;when SIO_STAT.0-or-2 ;Ready"},  //
    {"11", 11, 1, "RX Interrupt Enable", TRUE, "(0=Disable, 1=Enable) ;when N bytes in RX FIFO"},      //
    {"12", 12, 1, "DSR Interrupt Enable", TRUE, "(0=Disable, 1=Enable) ;when SIO_STAT.7  ;DSR=On"},    //
    {"13-15", 13, 3, "Not used", TRUE, "(always zero)"},                                               //
};

static ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
                                    ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

void ShowHelpMarker(const char* desc) {
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static void DrawControlEditor(PCSX::sio1Registers* regs) {
    bool changed = false;
    bool register_set = false;

    ImGui::Text("Control: 0x%04x", regs->control);
    ImGui::SameLine();
    ImGui::Button("Edit");

    if (ImGui::BeginTable("controlTable", 4, tableFlags)) {
        ImGui::TableSetupColumn("Bit(s)", ImGuiTableColumnFlags_WidthFixed);       // Column 0
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthFixed);  // 1
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed);        // 2
        ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthStretch);       // 3
        ImGui::TableHeadersRow();

        for (int row = 0; row < kControlEntries; row++) {
            ImGui::TableNextRow();
            for (int column = 0; column < 4; column++) {
                ImGui::TableSetColumnIndex(column);
                switch (column) {
                    case 0:
                        ImGui::Text(controlDisplay[row].bits);
                        break;

                    case 1:
                        ImGui::Text(controlDisplay[row].description);
                        ImGui::SameLine();
                        ShowHelpMarker(controlDisplay[row].notes);
                        break;

                    case 2:
                        ImGui::Text("%i", regs->control >> controlDisplay[row].shift & controlDisplay[row].mask);
                        break;

                    case 3:
                        if (controlDisplay[row].editable) {
                            if (controlDisplay[row].mask > 1) {
                                ImGui::Button("Edit");
                            } else {
                                register_set = (regs->control >> controlDisplay[row].shift) & 1;
                                changed = ImGui::Checkbox(controlDisplay[row].description, &register_set);

                                if (changed) {
                                    if (register_set) {
                                        regs->control |= (1 << controlDisplay[row].shift);
                                    } else {
                                        regs->control &= ~(1 << controlDisplay[row].shift);
                                    }
                                }
                            }
                        }
                        break;
                }
            }
        }
        ImGui::EndTable();
    }
}

static void DrawModeEditor(PCSX::sio1Registers* regs) {
    bool changed = false;
    bool register_set = false;

    ImGui::Text("Mode: 0x%04x", regs->mode);
    ImGui::SameLine();
    ImGui::Button("Edit");

    if (ImGui::BeginTable("modeTable", 4, tableFlags)) {
        ImGui::TableSetupColumn("Bit(s)", ImGuiTableColumnFlags_WidthFixed);       // Column 0
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthFixed);  // 1
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed);        // 2
        ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthStretch);       // 3
        ImGui::TableHeadersRow();

        for (int row = 0; row < kModeEntries; row++) {
            ImGui::TableNextRow();
            for (int column = 0; column < 4; column++) {
                ImGui::TableSetColumnIndex(column);
                switch (column) {
                    case 0:
                        ImGui::Text(modeDisplay[row].bits);
                        break;

                    case 1:
                        ImGui::Text(modeDisplay[row].description);
                        ImGui::SameLine();
                        ShowHelpMarker(modeDisplay[row].notes);
                        break;

                    case 2:
                        ImGui::Text("%i", regs->mode >> modeDisplay[row].shift & modeDisplay[row].mask);
                        break;

                    case 3:
                        if (modeDisplay[row].editable) {
                            if (modeDisplay[row].mask > 1) {
                                ImGui::Button("Edit");
                            } else {
                                register_set = (regs->mode >> modeDisplay[row].shift) & 1;
                                changed = ImGui::Checkbox(modeDisplay[row].description, &register_set);

                                if (changed) {
                                    if (register_set) {
                                        regs->mode |= (1 << modeDisplay[row].shift);
                                    } else {
                                        regs->mode &= ~(1 << modeDisplay[row].shift);
                                    }
                                }
                            }
                        }
                        break;
                }
            }
        }
        ImGui::EndTable();
    }
}

static void DrawStatusEditor(PCSX::sio1Registers* regs) {
    bool changed = false;
    bool register_set = false;

    ImGui::Text("Status: 0x%04x", regs->status);
    ImGui::SameLine();
    ImGui::Button("Edit");

    if (ImGui::BeginTable("statusTable", 4, tableFlags)) {
        ImGui::TableSetupColumn("Bit(s)", ImGuiTableColumnFlags_WidthFixed);       // Column 0
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthFixed);  // 1
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed);        // 2
        ImGui::TableSetupColumn("Edit", ImGuiTableColumnFlags_WidthStretch);       // 3
        ImGui::TableHeadersRow();

        for (int row = 0; row < kStatusEntries; row++) {
            ImGui::TableNextRow();
            for (int column = 0; column < 4; column++) {
                ImGui::TableSetColumnIndex(column);
                switch (column) {
                    case 0:
                        ImGui::Text(statusDisplay[row].bits);
                        break;

                    case 1:
                        ImGui::Text(statusDisplay[row].description);
                        ImGui::SameLine();
                        ShowHelpMarker(statusDisplay[row].notes);
                        break;

                    case 2:
                        ImGui::Text("%i", regs->status >> statusDisplay[row].shift & statusDisplay[row].mask);
                        break;

                    case 3:
                        if (statusDisplay[row].editable) {
                            if (statusDisplay[row].mask > 1) {
                                ImGui::Button("Edit");
                            } else {
                                register_set = (regs->status >> statusDisplay[row].shift) & 1;
                                changed = ImGui::Checkbox(statusDisplay[row].description, &register_set);

                                if (changed) {
                                    if (register_set) {
                                        regs->status |= (1 << statusDisplay[row].shift);
                                    } else {
                                        regs->status &= ~(1 << statusDisplay[row].shift);
                                    }
                                }
                            }
                        }
                        break;
                }
            }
        }
        ImGui::EndTable();
    }
}

void PCSX::Widgets::SIO1::draw(GUI* gui, sio1Registers* regs, const char* title) {
    bool changed = false;
    bool register_set = false;

    ImGui::SetNextWindowPos(ImVec2(1040, 20), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(210, 512), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(title, &m_show)) {
        ImGui::End();
        return;
    }

    static float w = 400.0f;
    static float h = 410.0f;

    static float width2 = 400.0f;
    static float height2 = 410.0f;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    ImGui::BeginChild("ChildTop", ImVec2(ImGui::GetContentRegionAvail().x, h));
    {
        // Status
        {
            ImGui::BeginChild("ChildLStatus",
                              ImVec2(w, ImGui::GetContentRegionAvail().y), true);

            DrawStatusEditor(regs);

            ImGui::EndChild();
        }

        ImGui::SameLine();

        // Vertical splitter - resizable
        {
            ImGui::InvisibleButton("vsplitter", ImVec2(8.0f, ImGui::GetContentRegionAvail().y));
            if (ImGui::IsItemActive()) w += ImGui::GetIO().MouseDelta.x;
        }

        ImGui::SameLine();

        // Control
        {
            ImGui::BeginChild("ChildRControl", ImVec2(0, ImGui::GetContentRegionAvail().y), true);

            DrawControlEditor(regs);

            ImGui::EndChild();
        }
    }
    ImGui::EndChild();

    ImGui::InvisibleButton("hsplitter", ImVec2(-1, 8.0f));
    if (ImGui::IsItemActive()) h += ImGui::GetIO().MouseDelta.y;

    ImGui::BeginChild("ChildBottom", ImVec2(ImGui::GetContentRegionAvail().x, height2));
    {
        // Mode
        {
            ImGui::BeginChild("ChildLMode", ImVec2(width2, ImGui::GetContentRegionAvail().y), true);

            DrawModeEditor(regs);

            ImGui::EndChild();
        }

        ImGui::SameLine();

        // Vertical splitter - resizable
        {
            ImGui::InvisibleButton("vsplitter", ImVec2(8.0f, ImGui::GetContentRegionAvail().y));
            if (ImGui::IsItemActive()) width2 += ImGui::GetIO().MouseDelta.x;
        }

        ImGui::SameLine();

        {
            ImGui::BeginChild("ChildBlank", ImVec2(0, ImGui::GetContentRegionAvail().y), true);

            ImGui::Text("This area intentionally left blank.");

            ImGui::EndChild();
        }
    }
    ImGui::EndChild();

    ImGui::PopStyleVar();

    ImGui::End();
}
