#include "FSTWindow.h"
#include <algorithm>
#include <list>
#include <iostream>
#include "imgui.h"
#include <bitset>
#include <fstream>
#include <sstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

//-------------------------------------------------------

//Shows the plots' names on the right to click on them and display matching plots
void FSTWindow::showPlotMenu() {
    if (g_Reader != nullptr) {
        ImGui::InputText("  Filter", filterBuffer, sizeof(filterBuffer));
        //For every scope we have one TreeNode
        for (const auto &scope : g_Reader->scopes) {
            //count for hidden signals
            int hiddenCount = 0;
            if (ImGui::TreeNode(scope->name.c_str())) {
                int count = 0;
                for (const auto &signal : scope->signals) {
                    std::string name = signal.second.name;
                    if (name.find(filterBuffer) != std::string::npos) {
                        if (hoverHighLight == signal.second.id) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1, 0.35, 0.10, 1));
                        }
                        ImGui::PushID(count);
                        if (ImGui::MenuItem((name.size() > 25 ? (name.substr(0, 25) + "...").c_str() : name.c_str()),
                                            "",
                                            FSTWindow::isDisplayed(signal.second.id))) {
                            if (!FSTWindow::isDisplayed(signal.second.id)) {
                                this->addPlot(signal.second.id);
                            } else {
                                this->removePlot(signal.second.id);
                            }
                        }
                        if (ImGui::IsItemHovered()) {
                            hoverRightClickMenu = signal.second.id;
                            ImGui::BeginTooltip();
                            ImGui::Text("%s", name.c_str());
                            ImGui::EndTooltip();
                        }
                        ImGui::PopID();
                        if (hoverHighLight == signal.second.id) {
                            ImGui::PopStyleColor();
                        }
                        count++;
                    } else {
                        hiddenCount++;
                    }
                }
                if (hiddenCount != 0) {
                    ImGui::MenuItem(("Hidden items " + std::to_string(hiddenCount)).c_str(), NULL, false, false);
                }
                ImGui::TreePop();
            }
        }
        showRightClickPlotSettings(hoverRightClickMenu);
    }
}

//-------------------------------------------------------

void FSTWindow::showRightClickPlotSettings(fstHandle signal) {
    Plot *plot = nullptr;
    for (Plot &item : g_Plots) {
        if (item.signalId == signal) {
            plot = &item;
        }
    }
    if (plot) {
        if (ImGui::BeginPopupContextWindow()) {
            ImGui::Text("   Value Format");
            ImGui::Separator();
            if (ImGui::MenuItem("Binary")) {
                plot->type = BINARY;
            }
            if (ImGui::MenuItem("Decimal")) {
                plot->type = DECIMALS;
            }
            if (ImGui::MenuItem("Hexadecimal")) {
                plot->type = HEXADECIMAL;
            }
            if (ImGui::InputText("  Custom", customFilterBuffer, sizeof(customFilterBuffer))) {
                plot->customtype_string = customFilterBuffer;
                plot->type = CUSTOM;
            }
            (this->bit_left_custom==-1)
            ? ImGui::Text("Bad custom expression")
            : ImGui::Text("Bit left for custom printing : %d", this->bit_left_custom);
            ImGui::Separator();
            ImGui::Text("   Plot Color Picker : ");
            ImGui::Separator();
            ImGui::ColorPicker4("Color Picker", (float *) &plot->color);
            ImGui::EndPopup();
        }
    }
}

//-------------------------------------------------------

//Adds a plot to the list
void FSTWindow::addPlot(fstHandle signal) {
    if (!g_Reader) {
        return;
    }
    Plot plot;
    plot.signalId = signal;
    std::string signalName = g_Reader->getSignal(signal)->name;
    plot.name = signalName;
    plot.type = DECIMALS;
    plot.fold = false;
    plot.color = ImVec4(1, 1, 1, 1);
    valuesList values = g_Reader->getValues(signal);
    for (const auto &item : values) {
        plot.x_data.push_back(item.first);
        plot.y_data.push_back(item.second);
    }

    g_Plots.push_back(plot);
}

//-------------------------------------------------------

//Removes a plot from the list
void FSTWindow::removePlot(fstHandle signal) {
    g_Plots.erase(remove_if(
            g_Plots.begin(),
            g_Plots.end(),
            [signal](Plot plot) {
                return plot.signalId == signal;
            }), g_Plots.end());
}

//-------------------------------------------------------

bool FSTWindow::isDisplayed(fstHandle signal) {
    auto res = std::find_if(g_Plots.begin(), g_Plots.end(), [signal](Plot plot) {
        return plot.signalId == signal;
    });
    return res != g_Plots.end();
}

//-------------------------------------------------------

// Function to convert binary to decimal
int FSTWindow::binaryToDecimal(std::string n) {
    std::string num = n;
    int dec_value = 0;

    int base = 1;

    int len = num.length();
    for (int i = len - 1; i >= 0; i--) {
        if (num[i] == '1')
            dec_value += base;
        base = base * 2;
    }

    return dec_value;
}

//-------------------------------------------------------

std::pair<std::string, int> FSTWindow::parseCustomExp(const std::string& expression, int value) {
    std::pair<std::string, int> res = std::pair("", 16);

    // Converting value to Binary
    std::string binaryVal;
    binaryVal = std::bitset<16>(value).to_string();

    char current = '0';
    std::string number;
    int numberInt = 0;
    std::stringstream stream;
    std::string buffer;

    //ToDo : regex

    // Parsing the expression and generating res
    for (char c: expression) {
        switch (c) {
            case 'b': // binary
            case 'd': // signed decimal
            case 'u': // unsigned decimal
            case 'x': // hex
                if (current != '0') return std::pair("", -1);
                current = c;
                break;
            case ';':
                if (current == '0' || number.empty()) return std::pair("", -1);
                numberInt = stoi(number);
                if (numberInt > binaryVal.length()) return std::pair("", -1);

                res.second -= numberInt;
                if (res.second < 0) // no bit left to print (max 16)
                {
                    return std::pair("", -1);
                }

                for (int i = 0; i < numberInt; i++) {
                    buffer += binaryVal[0]; // putting the first bit in the buffer
                    binaryVal.erase(binaryVal.begin()); // removing the first bit
                }

                switch (current) {
                    case 'b':
                        if (buffer.empty()) buffer = "0";
                        res.first += "b(" + buffer + ")";
                        break;
                    case 'u':
                        res.first += "u(" + std::to_string(this->binaryToDecimal(buffer)) + ")";
                        break;
                    case 'd':
                        if (buffer[0] == '1') {
                            buffer.erase(buffer.begin()); // removing the first bit
                            res.first += "d(-" + std::to_string(this->binaryToDecimal(buffer)) + ")";
                            break;
                        }
                        res.first += "d(" + std::to_string(this->binaryToDecimal(buffer)) + ")";
                        break;
                    case 'x':
                        stream << std::hex << this->binaryToDecimal(buffer);
                        res.first += "x(" + stream.str() + ")";
                        break;
                    default:
                        break;
                }
                current = '0';
                number = "";
                break;
            default:  // number
                if (current == '0') return std::pair("", -1);
                if (isdigit(c)) //checking if the char is a digit value
                {
                    number += c;
                }
                else
                {
                    return std::pair("", -1);
                }
                break;
        }
    }
    return res;
}

//-------------------------------------------------------

//Shows plots on the right of the window
void FSTWindow::showPlots() {
    //reset hoverHighLight to not change the color of a name if no plot is hovered
    hoverHighLight = 0;
    for (int i = 0; i < g_Plots.size(); i++) {
        Plot *item = &g_Plots[i];
        ImGui::PushID(i);
        ImGui::BeginGroup();

        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(cursor.x, cursor.y),
                                                  ImVec2(cursor.x + 1000, cursor.y + 25),
                                                  IM_COL32(51, 64, 74, 255));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);
        ImGui::Button(item->name.c_str());
        ImGui::SameLine();
        ImGui::SetCursorScreenPos(
                ImVec2(cursor.x + ImGui::GetWindowSize().x - ImGui::CalcTextSize("Folding").x - 23, cursor.y));
        if (ImGui::Button("Folding")) {
            item->fold = !item->fold;
        }
        ImGui::PopStyleVar(2);


        if (!item->fold) {
            ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(10, 0));
            //Coloring the line
            ImPlot::PushStyleColor(ImPlotCol_Line, item->color);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0);

            //set the plots Y limits to just below the lowest value to just upper the highest
            double max = *std::max_element(item->y_data.begin(), item->y_data.end());
            ImPlot::SetNextPlotLimitsY(0.0 - max / 3, max + max / 3);

            //Cloning in other values to prevent LinkNextPlot from modifying values (SetNextPlotLimitsX not working idk why)
            double xMin = plotXLimits->Min;
            double xMax = plotXLimits->Max;
            ImPlot::LinkNextPlotLimits(&xMin, &xMax, nullptr, nullptr);

            if (ImPlot::BeginPlot(item->name.c_str(), NULL, NULL, ImVec2(-1, 100),
                                  ImPlotFlags_NoLegend | ImPlotFlags_NoChild | ImPlotFlags_NoMousePos |
                                  ImPlotFlags_NoMenus | ImPlotFlags_NoTitle,
                                  NULL,
                                  ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoTickLabels)) {
                //Marker
                ImPlot::DragLineX("Marker", &markerX, true, ImVec4(1, 0.5, 0.5, 1), 1);

                ImPlot::PlotStairs(item->name.c_str(), (int *) &item->x_data[0], (int *) &item->y_data[0],
                                   item->x_data.size());


                std::vector<int> errors = g_Reader->getErrors(item->signalId);
                int maxY = -1;
                for (const auto &y : item->y_data) {
                    if (y > maxY) maxY = y;
                }
                for (const auto &error : errors) {
                    int next = -1;
                    for (const auto &x : item->x_data) {
                        if (error < x) {
                            next = x;
                            break;
                        }
                    }
                    ImPlot::PushPlotClipRect();
                    ImVec2 min = ImPlot::PlotToPixels(ImPlotPoint(error, 0 - maxY));
                    ImVec2 max = ImPlot::PlotToPixels(ImPlotPoint(next, 2 * maxY));
                    ImPlot::GetPlotDrawList()->AddRectFilled(min, max, IM_COL32(255, 0, 0, 100));
                    ImPlot::PopPlotClipRect();
                }
                if (ImPlot::IsPlotHovered()) {
                    //If the mouse is hover the plot, we take it's id to change the color of the right name on the left
                    hoverHighLight = item->signalId;
                    hoverRightClickMenu = item->signalId;

                    //modifies the range for all plots to be sync
                    ImPlotLimits limits = ImPlot::GetPlotLimits();
                    plotXLimits->Min = limits.X.Min;
                    plotXLimits->Max = limits.X.Max;

                    //Arrows to move to values change
                    ImGui::SetKeyboardFocusHere();
                    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow), true)) {
                        for (int x : item->x_data) {
                            if (markerX < x) {
                                markerX = x;
                                if (markerX > plotXLimits->Max || markerX < plotXLimits->Min) {
                                    double distance = plotXLimits->Max - plotXLimits->Min;
                                    plotXLimits->Min = markerX - (distance / 2);
                                    plotXLimits->Max = markerX + (distance / 2);
                                }
                                break;
                            }
                        }
                    }
                    if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow), true)) {
                        for (int ii = item->x_data.size() - 1; ii >= 0; ii--) {
                            int x = item->x_data[ii];
                            if (markerX > x) {
                                markerX = x;
                                if (markerX > plotXLimits->Max || markerX < plotXLimits->Min) {
                                    double distance = plotXLimits->Max - plotXLimits->Min;
                                    plotXLimits->Min = markerX - (distance / 2);
                                    plotXLimits->Max = markerX + (distance / 2);
                                }
                                break;
                            }
                        }
                    }

                    //Double click to move marker
                    if (ImGui::IsMouseDoubleClicked(0)) {
                        markerX = ImPlot::GetPlotMousePos().x;
                    }
                }

                //displaying values on the plot
                for (int i = 0; i < item->x_data.size(); i++) {
                    std::basic_string<char> value;
                    std::stringstream stream;
                    ImPlot::PushStyleColor(ImPlotCol_InlayText, ImVec4(1, 1, 1, 1));
                    //Converting to binary,decimal, hexa or custom user input
                    switch (item->type) {
                        case BINARY:
                            value = std::bitset<16>(item->y_data[i]).to_string();
                            break;
                        case DECIMALS:
                            value = std::to_string(item->y_data[i]);
                            break;
                        case HEXADECIMAL:
                            stream << std::hex << item->y_data[i];
                            value = stream.str();
                            break;
                        case CUSTOM:
                            std::pair<std::string, int> pair = parseCustomExp(item->customtype_string, item->y_data[i]);
                            value = pair.first;
                            this->bit_left_custom = pair.second;
                            if (value.empty())
                            {
                                value = std::to_string(item->y_data[i]); // Using Decimal if the expression is bad
                            }
                            break;
                    }
                    //Offset to place the value correctly
                    ImVec2 offset = ImVec2(0, 0);
                    if (i > 0) {
                        if (item->y_data[i] > item->y_data[i - 1]) {
                            offset.y = -6;
                        } else {
                            offset.y = 6;
                        }
                    }
                    ImPlot::PlotText(value.c_str(), item->x_data[i], item->y_data[i], false, offset);

                }

                ImPlot::PopStyleColor(2);
                ImPlot::PopStyleVar();
                ImPlot::EndPlot();
            }
        }

        ImGui::EndGroup();

/*
       //Drag&drop source
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
            ImGui::SetDragDropPayload("PlotPayload", &i, sizeof(fstHandle));
            ImGui::Text("%s", g_Reader->getSignalName(g_Plots[i].signalId).c_str());
            ImGui::EndDragDropSource();
        }
        ImGui::PopID();
        //Drag&drop target
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload *pload = ImGui::AcceptDragDropPayload("PlotPayload")) {
                int data = reinterpret_cast<int>(pload->Data);
                if (data > i) {
                    for (int k = payload; k > i; k--) {
                        std::swap(g_Plots[k], g_Plots[k - 1]);
                    }
                } else if (i > payload) {
                    for (int k = payload; k < i; k++) {
                        std::swap(g_Plots[k], g_Plots[k + 1]);
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }*/
    }
    this->showRightClickPlotSettings(hoverRightClickMenu);
}

//-------------------------------------------------------

void FSTWindow::render() {
    int treeWidth = 250;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(treeWidth + 500, 500), ImGuiCond_FirstUseEver);
    ImGui::Begin("PlotWindow", nullptr,
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        ImVec2 wPos = ImGui::GetCursorScreenPos();
        ImVec2 wSize = ImGui::GetWindowSize();
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y));
        ImGui::BeginChild("Tree", ImVec2(treeWidth, wSize.y), true,
                          ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysVerticalScrollbar);
        {
            this->showPlotMenu();
        }
        ImGui::EndChild();

        ImGui::SetNextWindowPos(ImVec2(wPos.x + treeWidth, wPos.y));
        ImGui::BeginChild("Graph", ImVec2(wSize.x - treeWidth, wSize.y), true,
                          ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysVerticalScrollbar);
        {
            this->showPlots();
        }
        ImGui::End();
    }
    ImGui::End();
    ImGui::PopStyleVar(); // Padding

    int index = -1;
    int tmp = 0;

    for (int ii = 1; ii < qindexValues.size(); ii++) {
        if (markerX < qindexValues[ii].first && markerX >= qindexValues[tmp].first) {
            index = tmp;
            std::cout << index << std::endl;
            break;
        }
        tmp = ii;
    }

    if (!editor) return;

    (index != -1) ? editor->FSMframeAtIndex(index)
        : editor->FSMunframe();
}

//-------------------------------------------------------

json FSTWindow::save() {
    json j;
    j["filePath"] = this->fstFilePath;
    j["rangeMin"] = range.Min;
    j["rangeMax"] = range.Max;
    j["markerX"] = markerX;
    std::vector<fstHandle> displayedPlots;
    std::vector<ConvertType> displayedTypes;
    for (const auto &item : g_Plots) {
        displayedPlots.push_back(item.signalId);
        displayedTypes.push_back(item.type);
    }
    j["displayedSignals"] = displayedPlots;
    j["displayedTypes"] = displayedTypes;
    return j;
}

//-------------------------------------------------------

void FSTWindow::clean() {
    g_Plots.clear();
    qindexValues.clear();
    g_Reader = nullptr;
}

//-------------------------------------------------------

void FSTWindow::load(std::string file, TextEditor &editors) {
    this->clean();
    this->fstFilePath = file;
    g_Reader = new FSTReader(file.c_str());
    if (plotXLimits == nullptr) {
        plotXLimits = &range;
        double maxTime = g_Reader->getMaxTime();
        plotXLimits->Min = 0 - (maxTime / 20);
        plotXLimits->Max = maxTime + (maxTime / 20);
    }
    this->editor = &editors;
    for (const auto &item : g_Reader->scopes) {
        for (const auto &signal : item->signals) {
            if (signal.second.name.find("_q_index") != std::string::npos) {
                qindex = signal.second.id;
            }
        }
    }
    valuesList valuesList = g_Reader->getValues(qindex);
    for (const auto &item : valuesList) {
        qindexValues.emplace_back(item.first, item.second);
    }
    editor->setIndexPairs(g_Reader->get_q_index_values());
}

void FSTWindow::load(json data, TextEditor &editors) {
    this->clean();
    this->fstFilePath = data["filePath"];
    g_Reader = new FSTReader(this->fstFilePath.c_str());
    range = ImPlotRange(data["rangeMin"], data["rangeMax"]);
    plotXLimits = &range;
    this->editor = &editors;
    markerX = data["markerX"];

    for (const auto &item : g_Reader->scopes) {
        for (const auto &signal : item->signals) {
            if (signal.second.name.find("_q_index") != std::string::npos) {
                qindex = signal.second.id;
            }
        }
    }
    valuesList valuesList = g_Reader->getValues(qindex);
    for (const auto &item : valuesList) {
        qindexValues.emplace_back(item.first, item.second);
    }

    editor->setIndexPairs(g_Reader->get_q_index_values());
}