#include "imgui_vulkan_init.h"
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <random>

// Helpers ---------------------------------------------------------------------
struct Holding {
    char  symbol[16];
    int   qty = 0;
    float avg_cost = 0.0f;
};

struct OrderFill {
    int   id;
    char  symbol[16];
    bool  is_buy;
    int   qty;
    float price;
};

static int find_holding(const std::vector<Holding>& v, const char* sym) {
    for (int i = 0; i < (int)v.size(); ++i)
        if (std::strncmp(v[i].symbol, sym, sizeof(v[i].symbol)) == 0) return i;
    return -1;
}

// Main ------------------------------------------------------------------------
int main(int, char**)
{
    InitOutput init = initilize();
    auto [window, err, wd] = init;

    // --- App/login state & colors ---
    bool  logged_in = false;
    char  username[32] = "";
    char  password[32] = "";
    char  greet_name[32] = "bob";

    const ImVec4 kLoginColor = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    const ImVec4 kHomeColor  = ImVec4(0.12f, 0.18f, 0.12f, 1.00f);
    ImVec4 clear_color = kLoginColor;

    // --- Fixed center window size (no auto-resize) ---
    const ImVec2 kCenterSize(900.0f, 560.0f); // Change as you like

    // --- Toy trading state ---
    static bool                       trading_state_init = false;
    static float                      cash = 100000.0f;
    static float                      realized_pnl = 0.0f;
    static std::vector<Holding>       holdings;
    static std::vector<OrderFill>     fills;
    static int                        next_order_id = 1;

    static const char* kSymbols[] = { "AAPL", "MSFT", "TSLA", "SPY", "BTC" };
    static const int   kSymbolCount = (int)(sizeof(kSymbols)/sizeof(kSymbols[0]));
    static std::unordered_map<std::string, float> quote;

    static int   ticket_symbol_idx = 0;
    static int   ticket_qty        = 10;
    static float ticket_price      = 100.0f;
    static bool  ticket_market     = true;

    // Simple status line shown inside the center window
    static char  status_line[128] = "";

    // RNG for small quote nudges
    static std::mt19937 rng{12345};
    static std::normal_distribution<float> n01(0.0f, 1.0f);

    auto init_trading_state = [&]() {
        if (trading_state_init) return;
        trading_state_init = true;
        // Seed some starting quotes
        quote["AAPL"] = 195.0f;
        quote["MSFT"] = 420.0f;
        quote["TSLA"] = 250.0f;
        quote["SPY"]  = 540.0f;
        quote["BTC"]  = 65000.0f;

        ticket_price = quote[kSymbols[ticket_symbol_idx]];
    };

    auto portfolio_market_value = [&]() -> float {
        float mv = 0.0f;
        for (const auto& h : holdings) {
            auto it = quote.find(h.symbol);
            if (it != quote.end()) mv += h.qty * it->second;
        }
        return mv;
    };

    auto portfolio_unrealized = [&]() -> float {
        float upnl = 0.0f;
        for (const auto& h : holdings) {
            auto it = quote.find(h.symbol);
            if (it != quote.end()) upnl += h.qty * (it->second - h.avg_cost);
        }
        return upnl;
    };

    auto do_fill = [&](const char* sym, bool is_buy, int qty, float price) {
        // Update holdings + cash + realized pnl
        int idx = find_holding(holdings, sym);

        if (is_buy) {
            cash -= qty * price;
            if (idx < 0) {
                Holding h{};
                std::strncpy(h.symbol, sym, sizeof(h.symbol)-1);
                h.qty = qty;
                h.avg_cost = price;
                holdings.push_back(h);
            } else {
                Holding& h = holdings[idx];
                float new_qty = (float)h.qty + qty;
                if (new_qty <= 0.0f) new_qty = 1.0f; // safety
                h.avg_cost = (h.avg_cost * (float)h.qty + price * (float)qty) / new_qty;
                h.qty += qty;
            }
        } else {
            int available = (idx >= 0) ? holdings[idx].qty : 0;
            int actual_qty = std::min(qty, available);
            if (actual_qty <= 0) {
                std::snprintf(status_line, sizeof(status_line), "No shares to sell.");
                return;
            }
            Holding& h = holdings[idx];
            cash += actual_qty * price;
            realized_pnl += (price - h.avg_cost) * (float)actual_qty;
            h.qty -= actual_qty;
            if (h.qty == 0) holdings.erase(holdings.begin() + idx);
        }

        // Record fill
        OrderFill f{};
        f.id = next_order_id++;
        std::strncpy(f.symbol, sym, sizeof(f.symbol)-1);
        f.is_buy = is_buy;
        f.qty = qty;
        f.price = price;
        fills.push_back(f);

        std::snprintf(status_line, sizeof(status_line), "%s %d %s @ %.2f",
                      is_buy ? "Bought" : "Sold", qty, sym, price);
    };

    // Main loop ----------------------------------------------------------------
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Resize swap chain?
        int fb_width, fb_height;
        glfwGetFramebufferSize(window, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0 &&
            (g_SwapChainRebuild || g_MainWindowData.Width != fb_width || g_MainWindowData.Height != fb_height))
        {
            ImGui_ImplVulkan_SetMinImageCount(g_MinImageCount);
            ImGui_ImplVulkanH_CreateOrResizeWindow(
                g_Instance, g_PhysicalDevice, g_Device, &g_MainWindowData, g_QueueFamily,
                g_Allocator, fb_width, fb_height, g_MinImageCount);
            g_MainWindowData.FrameIndex = 0;
            g_SwapChainRebuild = false;
        }
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0) {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Menu bar (Logout only now)
        {
            ImGui::BeginMainMenuBar();
            if (logged_in) {
                float x = ImGui::GetWindowWidth() - ImGui::CalcTextSize("Logout").x - 20.0f;
                ImGui::SetCursorPosX(x);
                if (ImGui::Button("Logout")) {
                    logged_in = false;
                    clear_color = kLoginColor;
                    username[0] = password[0] = 0;
                    status_line[0] = 0;
                    std::strncpy(greet_name, "bob", sizeof(greet_name)-1);
                    greet_name[sizeof(greet_name)-1] = '\0';
                }
            }
            ImGui::EndMainMenuBar();
        }

        // Center window: fixed size & centered every frame
        {
            ImGuiViewport* vp = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(vp->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(kCenterSize, ImGuiCond_Always);

            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
                                   | ImGuiWindowFlags_NoSavedSettings
                                   | ImGuiWindowFlags_NoMove
                                   | ImGuiWindowFlags_NoResize
                                   | ImGuiWindowFlags_NoCollapse;

            ImGui::Begin("CenterWindowFixed", nullptr, flags);

            if (!logged_in)
            {
                ImGui::TextUnformatted("Login");
                ImGui::Separator();
                ImGui::SetNextItemWidth(300.0f);
                bool submit = ImGui::InputText("Username", username, IM_ARRAYSIZE(username),
                                               ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::SetNextItemWidth(300.0f);
                submit |= ImGui::InputText("Password", password, IM_ARRAYSIZE(password),
                                           ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_Password);

                if (ImGui::Button("Login") || submit) {
                    if (username[0] == '\0')
                        std::strncpy(greet_name, "bob", sizeof(greet_name)-1);
                    else
                        std::strncpy(greet_name, username, sizeof(greet_name)-1);
                    greet_name[sizeof(greet_name)-1] = '\0';

                    logged_in = true;
                    clear_color = kHomeColor;
                    std::memset(password, 0, sizeof(password));
                    init_trading_state();
                    std::snprintf(status_line, sizeof(status_line), "Welcome, %s!", greet_name);
                }

                ImGui::TextDisabled("or sign up...");
            }
            else
            {
                // ---- Trading UI (fixed window; contents can scroll if needed) ---
                ImGui::Text("Wef Trader", greet_name);
                if (status_line[0]) {
                    ImGui::SameLine();
                    ImGui::TextDisabled(" | %s", status_line);
                }
                ImGui::Separator();

                if (ImGui::BeginTabBar("TradingTabs"))
                {
                    // Overview Tab ------------------------------------------------
                    if (ImGui::BeginTabItem("Overview"))
                    {
                        float mv   = portfolio_market_value();
                        float upnl = portfolio_unrealized();
                        float eq   = cash + mv;

                        ImGui::Text("Cash:         $%.2f", cash);
                        ImGui::Text("Portfolio MV: $%.2f", mv);
                        ImGui::Text("Equity:       $%.2f", eq);
                        ImGui::Text("Unrealized P&L: %s$%.2f", (upnl>=0?"+":""), upnl);
                        ImGui::Text("Realized P&L:  %s$%.2f", (realized_pnl>=0?"+":""), realized_pnl);

                        ImGui::Spacing();
                        ImGui::TextUnformatted("Portfolio");
                        if (ImGui::BeginTable("PortfolioTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                        {
                            ImGui::TableSetupColumn("Symbol");
                            ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                            ImGui::TableSetupColumn("Avg Cost");
                            ImGui::TableSetupColumn("Price");
                            ImGui::TableSetupColumn("Market Value");
                            ImGui::TableSetupColumn("Unreal. P&L");
                            ImGui::TableHeadersRow();

                            for (const auto& h : holdings)
                            {
                                float px = quote[h.symbol];
                                float mv_pos = h.qty * px;
                                float upnl_pos = h.qty * (px - h.avg_cost);

                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(h.symbol);
                                ImGui::TableSetColumnIndex(1); ImGui::Text("%d", h.qty);
                                ImGui::TableSetColumnIndex(2); ImGui::Text("$%.2f", h.avg_cost);
                                ImGui::TableSetColumnIndex(3); ImGui::Text("$%.2f", px);
                                ImGui::TableSetColumnIndex(4); ImGui::Text("$%.2f", mv_pos);
                                ImGui::TableSetColumnIndex(5); ImGui::Text("%s$%.2f", (upnl_pos>=0?"+":""), upnl_pos);
                            }
                            ImGui::EndTable();
                        }

                        ImGui::EndTabItem();
                    }

                    // Trade Tab ---------------------------------------------------
                    if (ImGui::BeginTabItem("Trade"))
                    {
                        ImGui::TextUnformatted("Order Ticket");
                        ImGui::Separator();

                        ImGui::TextUnformatted("Symbol");
                        if (ImGui::BeginCombo("##ticket_symbol", kSymbols[ticket_symbol_idx])) {
                            for (int i = 0; i < kSymbolCount; ++i) {
                                bool selected = (ticket_symbol_idx == i);
                                if (ImGui::Selectable(kSymbols[i], selected)) {
                                    ticket_symbol_idx = i;
                                    ticket_price = quote[kSymbols[i]];
                                }
                                if (selected) ImGui::SetItemDefaultFocus();
                            }
                            ImGui::EndCombo();
                        }

                        ImGui::SetNextItemWidth(120.0f);
                        ImGui::InputInt("Quantity", &ticket_qty);
                        ticket_qty = std::max(1, ticket_qty);

                        ImGui::Checkbox("Market", &ticket_market);
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(120.0f);
                        bool price_changed = ImGui::InputFloat("Limit Price", &ticket_price, 0.0f, 0.0f, "%.2f");
                        if (!price_changed && ticket_market) {
                            ticket_price = quote[kSymbols[ticket_symbol_idx]];
                        }

                        const char* sym = kSymbols[ticket_symbol_idx];
                        float mkt_px = quote[sym];
                        ImGui::Text("Current %s: $%.2f", sym, mkt_px);

                        if (ImGui::Button("Buy")) {
                            float px = ticket_market ? mkt_px : ticket_price;
                            do_fill(sym, true, ticket_qty, px);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Sell")) {
                            float px = ticket_market ? mkt_px : ticket_price;
                            do_fill(sym, false, ticket_qty, px);
                        }

                        ImGui::Spacing();
                        ImGui::TextDisabled("Note: Everything fills immediately. This is a toy.");

                        ImGui::EndTabItem();
                    }

                    // Quotes Tab --------------------------------------------------
                    if (ImGui::BeginTabItem("Quotes"))
                    {
                        ImGui::TextUnformatted("Adjust toy quotes (manual or nudge):");
                        if (ImGui::BeginTable("QuoteTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                        {
                            ImGui::TableSetupColumn("Symbol");
                            ImGui::TableSetupColumn("Price");
                            ImGui::TableHeadersRow();

                            for (int i = 0; i < kSymbolCount; ++i)
                            {
                                const char* s = kSymbols[i];
                                float& px = quote[s];
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(s);
                                ImGui::TableSetColumnIndex(1);
                                ImGui::SetNextItemWidth(120.0f);
                                ImGui::InputFloat(("##px_" + std::string(s)).c_str(), &px, 0.0f, 0.0f, "%.2f");
                            }
                            ImGui::EndTable();
                        }

                        if (ImGui::Button("Nudge prices")) {
                            for (int i = 0; i < kSymbolCount; ++i) {
                                float& px = quote[kSymbols[i]];
                                float step = n01(rng) * (px * 0.0025f); // ~0.25% std dev
                                px = std::max(0.01f, px + step);
                            }
                        }

                        ImGui::EndTabItem();
                    }

                    // Orders Tab --------------------------------------------------
                    if (ImGui::BeginTabItem("Orders"))
                    {
                        if (ImGui::BeginTable("FillsTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                        {
                            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 40.0f);
                            ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                            ImGui::TableSetupColumn("Symbol");
                            ImGui::TableSetupColumn("Qty", ImGuiTableColumnFlags_WidthFixed, 60.0f);
                            ImGui::TableSetupColumn("Price");
                            ImGui::TableHeadersRow();

                            for (const auto& f : fills) {
                                ImGui::TableNextRow();
                                ImGui::TableSetColumnIndex(0); ImGui::Text("%d", f.id);
                                ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(f.is_buy ? "BUY" : "SELL");
                                ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(f.symbol);
                                ImGui::TableSetColumnIndex(3); ImGui::Text("%d", f.qty);
                                ImGui::TableSetColumnIndex(4); ImGui::Text("$%.2f", f.price);
                            }
                            ImGui::EndTable();
                        }
                        if (ImGui::Button("Clear orders")) fills.clear();

                        ImGui::EndTabItem();
                    }

                    ImGui::EndTabBar();
                }
            }

            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        ImDrawData* draw_data = ImGui::GetDrawData();
        const bool is_minimized = (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f);
        if (!is_minimized)
        {
            wd->ClearValue.color.float32[0] = clear_color.x * clear_color.w;
            wd->ClearValue.color.float32[1] = clear_color.y * clear_color.w;
            wd->ClearValue.color.float32[2] = clear_color.z * clear_color.w;
            wd->ClearValue.color.float32[3] = clear_color.w;
            FrameRender(wd, draw_data);
            FramePresent(wd);
        }
    }

    // Cleanup
    err = vkDeviceWaitIdle(g_Device);
    check_vk_result(err);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    CleanupVulkanWindow();
    CleanupVulkan();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
