/**
 * VisionPipe Documentation Generator
 * 
 * Generates HTML, Markdown, and JSON documentation for VisionPipe interpreter items.
 */

#include "doc_gen.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

#include "interpreter/runtime.h"

namespace visionpipe {

// ============================================================================
// Helper functions
// ============================================================================

static std::string escapeHtml(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            default: result += c;
        }
    }
    return result;
}

static void createDirectory(const std::string& path) {
#ifdef _WIN32
    system(("mkdir \"" + path + "\" 2>nul").c_str());
#else
    system(("mkdir -p \"" + path + "\"").c_str());
#endif
}

// ============================================================================
// HTML CSS Generation (split to avoid MSVC string literal limits)
// ============================================================================

static void writeHtmlHead(std::ofstream& html) {
    html << "<!DOCTYPE html>\n";
    html << "<html lang=\"en\">\n";
    html << "<head>\n";
    html << "    <meta charset=\"UTF-8\">\n";
    html << "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n";
    html << "    <title>VisionPipe Documentation</title>\n";
    html << "    <link rel=\"preconnect\" href=\"https://fonts.googleapis.com\">\n";
    html << "    <link rel=\"preconnect\" href=\"https://fonts.gstatic.com\" crossorigin>\n";
    html << "    <link href=\"https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&family=JetBrains+Mono:wght@400;500&display=swap\" rel=\"stylesheet\">\n";
    html << "    <style>\n";
}

static void writeHtmlCssVariables(std::ofstream& html) {
    html << "        :root {\n";
    html << "            --bg-body: #1a1a1f;\n";
    html << "            --bg-sidebar: #141417;\n";
    html << "            --bg-content: #1e1e24;\n";
    html << "            --bg-card: #252530;\n";
    html << "            --bg-card-hover: #2d2d3a;\n";
    html << "            --bg-code: #0d0d10;\n";
    html << "            --bg-input: #252530;\n";
    html << "            --border-subtle: rgba(255,255,255,0.06);\n";
    html << "            --border-medium: rgba(255,255,255,0.1);\n";
    html << "            --text-primary: #f5f5f7;\n";
    html << "            --text-secondary: #a1a1aa;\n";
    html << "            --text-muted: #71717a;\n";
    html << "            --accent-red: #ef4444;\n";
    html << "            --accent-red-hover: #f87171;\n";
    html << "            --accent-orange: #f97316;\n";
    html << "            --accent-orange-hover: #fb923c;\n";
    html << "            --accent-amber: #f59e0b;\n";
    html << "            --accent-gradient: linear-gradient(135deg, #ef4444, #f97316);\n";
    html << "            --success: #22c55e;\n";
    html << "            --warning: #eab308;\n";
    html << "            --radius-sm: 6px;\n";
    html << "            --radius-md: 10px;\n";
    html << "            --radius-lg: 16px;\n";
    html << "            --shadow-sm: 0 1px 2px rgba(0,0,0,0.3);\n";
    html << "            --shadow-md: 0 4px 12px rgba(0,0,0,0.4);\n";
    html << "            --shadow-lg: 0 8px 30px rgba(0,0,0,0.5);\n";
    html << "            --transition-fast: 150ms ease;\n";
    html << "            --transition-normal: 250ms ease;\n";
    html << "        }\n";
}

static void writeHtmlCssBase(std::ofstream& html) {
    html << "        * { box-sizing: border-box; margin: 0; padding: 0; }\n";
    html << "        html { scroll-behavior: smooth; }\n";
    html << "        body {\n";
    html << "            font-family: 'Inter', -apple-system, BlinkMacSystemFont, sans-serif;\n";
    html << "            background: var(--bg-body);\n";
    html << "            color: var(--text-primary);\n";
    html << "            line-height: 1.65;\n";
    html << "            font-size: 15px;\n";
    html << "            -webkit-font-smoothing: antialiased;\n";
    html << "        }\n";
    html << "        .app-container { display: flex; min-height: 100vh; }\n";
}

static void writeHtmlCssSidebar(std::ofstream& html) {
    html << "        .sidebar {\n";
    html << "            width: 280px;\n";
    html << "            background: var(--bg-sidebar);\n";
    html << "            border-right: 1px solid var(--border-subtle);\n";
    html << "            position: fixed;\n";
    html << "            top: 0;\n";
    html << "            left: 0;\n";
    html << "            height: 100vh;\n";
    html << "            overflow-y: auto;\n";
    html << "            z-index: 1000;\n";
    html << "            display: flex;\n";
    html << "            flex-direction: column;\n";
    html << "        }\n";
    html << "        .sidebar::-webkit-scrollbar { width: 6px; }\n";
    html << "        .sidebar::-webkit-scrollbar-track { background: transparent; }\n";
    html << "        .sidebar::-webkit-scrollbar-thumb { background: var(--border-medium); border-radius: 3px; }\n";
    html << "        .sidebar-header {\n";
    html << "            padding: 24px 20px;\n";
    html << "            border-bottom: 1px solid var(--border-subtle);\n";
    html << "            background: var(--bg-sidebar);\n";
    html << "            position: sticky;\n";
    html << "            top: 0;\n";
    html << "            z-index: 10;\n";
    html << "        }\n";
    html << "        .logo { display: flex; align-items: center; gap: 12px; text-decoration: none; }\n";
    html << "        .logo-icon {\n";
    html << "            width: 36px;\n";
    html << "            height: 36px;\n";
    html << "            background: var(--accent-gradient);\n";
    html << "            border-radius: var(--radius-md);\n";
    html << "            display: flex;\n";
    html << "            align-items: center;\n";
    html << "            justify-content: center;\n";
    html << "            font-weight: 700;\n";
    html << "            font-size: 18px;\n";
    html << "            color: white;\n";
    html << "            box-shadow: var(--shadow-md);\n";
    html << "        }\n";
    html << "        .logo-text { font-size: 20px; font-weight: 700; color: var(--text-primary); }\n";
    html << "        .version-badge {\n";
    html << "            display: inline-block;\n";
    html << "            background: var(--bg-card);\n";
    html << "            color: var(--accent-orange);\n";
    html << "            padding: 3px 10px;\n";
    html << "            border-radius: 20px;\n";
    html << "            font-size: 11px;\n";
    html << "            font-weight: 600;\n";
    html << "            margin-top: 12px;\n";
    html << "            border: 1px solid var(--border-subtle);\n";
    html << "        }\n";
}

static void writeHtmlCssSearch(std::ofstream& html) {
    html << "        .sidebar-search { padding: 16px 20px; border-bottom: 1px solid var(--border-subtle); }\n";
    html << "        .search-input-wrapper { position: relative; }\n";
    html << "        .search-icon { position: absolute; left: 12px; top: 50%; transform: translateY(-50%); color: var(--text-muted); pointer-events: none; }\n";
    html << "        .search-input {\n";
    html << "            width: 100%;\n";
    html << "            padding: 10px 12px 10px 40px;\n";
    html << "            background: var(--bg-input);\n";
    html << "            border: 1px solid var(--border-subtle);\n";
    html << "            border-radius: var(--radius-md);\n";
    html << "            color: var(--text-primary);\n";
    html << "            font-size: 14px;\n";
    html << "            outline: none;\n";
    html << "            transition: var(--transition-fast);\n";
    html << "        }\n";
    html << "        .search-input:focus { border-color: var(--accent-orange); box-shadow: 0 0 0 3px rgba(249, 115, 22, 0.15); }\n";
    html << "        .search-input::placeholder { color: var(--text-muted); }\n";
    html << "        .search-shortcut {\n";
    html << "            position: absolute;\n";
    html << "            right: 10px;\n";
    html << "            top: 50%;\n";
    html << "            transform: translateY(-50%);\n";
    html << "            background: var(--bg-body);\n";
    html << "            padding: 3px 8px;\n";
    html << "            border-radius: 4px;\n";
    html << "            font-size: 11px;\n";
    html << "            color: var(--text-muted);\n";
    html << "            font-family: 'JetBrains Mono', monospace;\n";
    html << "            border: 1px solid var(--border-subtle);\n";
    html << "        }\n";
}

static void writeHtmlCssNav(std::ofstream& html) {
    html << "        .sidebar-nav { flex: 1; padding: 16px 12px; }\n";
    html << "        .nav-section { margin-bottom: 24px; }\n";
    html << "        .nav-section-title {\n";
    html << "            font-size: 11px;\n";
    html << "            font-weight: 600;\n";
    html << "            color: var(--text-muted);\n";
    html << "            text-transform: uppercase;\n";
    html << "            letter-spacing: 0.5px;\n";
    html << "            padding: 8px;\n";
    html << "            margin-bottom: 4px;\n";
    html << "        }\n";
    html << "        .nav-item {\n";
    html << "            display: flex;\n";
    html << "            align-items: center;\n";
    html << "            justify-content: space-between;\n";
    html << "            padding: 10px 12px;\n";
    html << "            color: var(--text-secondary);\n";
    html << "            text-decoration: none;\n";
    html << "            border-radius: var(--radius-sm);\n";
    html << "            font-size: 14px;\n";
    html << "            font-weight: 500;\n";
    html << "            transition: var(--transition-fast);\n";
    html << "            cursor: pointer;\n";
    html << "        }\n";
    html << "        .nav-item:hover { background: var(--bg-card); color: var(--text-primary); }\n";
    html << "        .nav-item.active { background: rgba(249, 115, 22, 0.1); color: var(--accent-orange); }\n";
    html << "        .nav-item-count { background: var(--bg-card); padding: 2px 8px; border-radius: 10px; font-size: 11px; color: var(--text-muted); }\n";
    html << "        .nav-item.active .nav-item-count { background: rgba(249, 115, 22, 0.2); color: var(--accent-orange); }\n";
    html << "        .sidebar-footer { padding: 16px 20px; border-top: 1px solid var(--border-subtle); background: var(--bg-sidebar); }\n";
    html << "        .sidebar-stats { display: flex; gap: 16px; }\n";
    html << "        .stat-item { flex: 1; }\n";
    html << "        .stat-value { font-size: 20px; font-weight: 700; color: var(--accent-orange); }\n";
    html << "        .stat-label { font-size: 11px; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.3px; }\n";
}

static void writeHtmlCssMainContent(std::ofstream& html) {
    html << "        .main-content { flex: 1; margin-left: 280px; min-height: 100vh; }\n";
    html << "        .content-header {\n";
    html << "            background: var(--bg-content);\n";
    html << "            border-bottom: 1px solid var(--border-subtle);\n";
    html << "            padding: 48px 60px;\n";
    html << "            position: sticky;\n";
    html << "            top: 0;\n";
    html << "            z-index: 100;\n";
    html << "        }\n";
    html << "        .content-header h1 {\n";
    html << "            font-size: 32px;\n";
    html << "            font-weight: 700;\n";
    html << "            margin-bottom: 8px;\n";
    html << "            background: var(--accent-gradient);\n";
    html << "            -webkit-background-clip: text;\n";
    html << "            -webkit-text-fill-color: transparent;\n";
    html << "            background-clip: text;\n";
    html << "        }\n";
    html << "        .content-header p { color: var(--text-secondary); font-size: 16px; max-width: 600px; }\n";
    html << "        .content-body { padding: 40px 60px; }\n";
}

static void writeHtmlCssFilterBar(std::ofstream& html) {
    html << "        .filter-bar { display: flex; gap: 12px; margin-bottom: 32px; flex-wrap: wrap; align-items: center; }\n";
    html << "        .filter-chip {\n";
    html << "            padding: 8px 16px;\n";
    html << "            background: var(--bg-card);\n";
    html << "            border: 1px solid var(--border-subtle);\n";
    html << "            border-radius: 20px;\n";
    html << "            color: var(--text-secondary);\n";
    html << "            font-size: 13px;\n";
    html << "            font-weight: 500;\n";
    html << "            cursor: pointer;\n";
    html << "            transition: var(--transition-fast);\n";
    html << "        }\n";
    html << "        .filter-chip:hover { border-color: var(--accent-orange); color: var(--accent-orange); }\n";
    html << "        .filter-chip.active { background: var(--accent-gradient); border-color: transparent; color: white; }\n";
    html << "        .results-count { margin-left: auto; color: var(--text-muted); font-size: 13px; }\n";
}

static void writeHtmlCssCategory(std::ofstream& html) {
    html << "        .category-section { margin-bottom: 48px; scroll-margin-top: 140px; }\n";
    html << "        .category-header { display: flex; align-items: center; gap: 16px; margin-bottom: 24px; padding-bottom: 16px; border-bottom: 1px solid var(--border-subtle); }\n";
    html << "        .category-icon {\n";
    html << "            width: 40px;\n";
    html << "            height: 40px;\n";
    html << "            background: var(--accent-gradient);\n";
    html << "            border-radius: var(--radius-md);\n";
    html << "            display: flex;\n";
    html << "            align-items: center;\n";
    html << "            justify-content: center;\n";
    html << "            font-size: 18px;\n";
    html << "        }\n";
    html << "        .category-title { font-size: 24px; font-weight: 700; color: var(--text-primary); }\n";
    html << "        .category-count { background: var(--bg-card); padding: 4px 12px; border-radius: 20px; font-size: 12px; color: var(--text-muted); font-weight: 500; }\n";
    html << "        .items-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(420px, 1fr)); gap: 20px; }\n";
}

static void writeHtmlCssItemCard(std::ofstream& html) {
    html << "        .item-card {\n";
    html << "            background: var(--bg-card);\n";
    html << "            border: 1px solid var(--border-subtle);\n";
    html << "            border-radius: var(--radius-lg);\n";
    html << "            padding: 24px;\n";
    html << "            transition: var(--transition-normal);\n";
    html << "            cursor: pointer;\n";
    html << "        }\n";
    html << "        .item-card:hover {\n";
    html << "            background: var(--bg-card-hover);\n";
    html << "            border-color: var(--accent-orange);\n";
    html << "            transform: translateY(-2px);\n";
    html << "            box-shadow: var(--shadow-lg);\n";
    html << "        }\n";
    html << "        .item-header { display: flex; align-items: flex-start; justify-content: space-between; margin-bottom: 12px; }\n";
    html << "        .item-name { font-family: 'JetBrains Mono', monospace; font-size: 16px; font-weight: 600; color: var(--accent-orange); }\n";
    html << "        .item-name::after { content: '()'; color: var(--text-muted); }\n";
    html << "        .item-desc { color: var(--text-secondary); font-size: 14px; line-height: 1.6; margin-bottom: 16px; }\n";
}

static void writeHtmlCssParams(std::ofstream& html) {
    html << "        .item-params { background: var(--bg-body); border-radius: var(--radius-md); padding: 16px; margin-bottom: 16px; }\n";
    html << "        .params-title { font-size: 11px; font-weight: 600; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.5px; margin-bottom: 12px; }\n";
    html << "        .param-row { display: flex; align-items: flex-start; gap: 12px; padding: 8px 0; border-bottom: 1px solid var(--border-subtle); font-size: 13px; }\n";
    html << "        .param-row:last-child { border-bottom: none; }\n";
    html << "        .param-name { font-family: 'JetBrains Mono', monospace; color: var(--accent-red); min-width: 120px; font-weight: 500; }\n";
    html << "        .param-type { font-family: 'JetBrains Mono', monospace; color: var(--text-muted); font-size: 12px; min-width: 70px; }\n";
    html << "        .param-desc { flex: 1; color: var(--text-secondary); }\n";
    html << "        .param-required { color: var(--accent-red); font-weight: 600; font-size: 11px; }\n";
    html << "        .param-optional { background: rgba(234, 179, 8, 0.15); color: var(--warning); padding: 2px 8px; border-radius: 4px; font-size: 10px; font-weight: 600; }\n";
}

static void writeHtmlCssExample(std::ofstream& html) {
    html << "        .item-example {\n";
    html << "            background: var(--bg-code);\n";
    html << "            border-radius: var(--radius-md);\n";
    html << "            padding: 16px;\n";
    html << "            font-family: 'JetBrains Mono', monospace;\n";
    html << "            font-size: 13px;\n";
    html << "            color: var(--text-secondary);\n";
    html << "            overflow-x: auto;\n";
    html << "            border-left: 3px solid var(--accent-orange);\n";
    html << "        }\n";
    html << "        .item-tags { display: flex; gap: 8px; flex-wrap: wrap; margin-top: 16px; }\n";
    html << "        .tag { padding: 4px 12px; background: var(--bg-body); border-radius: 20px; font-size: 11px; color: var(--text-muted); font-weight: 500; border: 1px solid var(--border-subtle); }\n";
}

static void writeHtmlCssFooter(std::ofstream& html) {
    html << "        .footer { background: var(--bg-sidebar); border-top: 1px solid var(--border-subtle); padding: 32px 60px; margin-top: 60px; }\n";
    html << "        .footer-content { display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 16px; }\n";
    html << "        .footer-text { color: var(--text-muted); font-size: 13px; }\n";
    html << "        .footer-link { color: var(--accent-orange); text-decoration: none; transition: var(--transition-fast); }\n";
    html << "        .footer-link:hover { color: var(--accent-orange-hover); }\n";
}

static void writeHtmlCssMobile(std::ofstream& html) {
    html << "        .mobile-menu-btn {\n";
    html << "            display: none;\n";
    html << "            position: fixed;\n";
    html << "            bottom: 24px;\n";
    html << "            right: 24px;\n";
    html << "            width: 56px;\n";
    html << "            height: 56px;\n";
    html << "            background: var(--accent-gradient);\n";
    html << "            border: none;\n";
    html << "            border-radius: 50%;\n";
    html << "            color: white;\n";
    html << "            font-size: 24px;\n";
    html << "            cursor: pointer;\n";
    html << "            z-index: 1001;\n";
    html << "            box-shadow: var(--shadow-lg);\n";
    html << "        }\n";
    html << "        .mobile-overlay { display: none; position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.6); z-index: 999; }\n";
    html << "        .empty-state { text-align: center; padding: 80px 40px; color: var(--text-muted); }\n";
    html << "        .empty-state-icon { font-size: 48px; margin-bottom: 16px; opacity: 0.5; }\n";
    html << "        .empty-state-text { font-size: 16px; }\n";
}

static void writeHtmlCssResponsive(std::ofstream& html) {
    html << "        @media (max-width: 1024px) {\n";
    html << "            .items-grid { grid-template-columns: 1fr; }\n";
    html << "            .content-header, .content-body, .footer { padding-left: 40px; padding-right: 40px; }\n";
    html << "        }\n";
    html << "        @media (max-width: 768px) {\n";
    html << "            .sidebar { transform: translateX(-100%); transition: transform var(--transition-normal); }\n";
    html << "            .sidebar.open { transform: translateX(0); }\n";
    html << "            .main-content { margin-left: 0; }\n";
    html << "            .mobile-menu-btn { display: flex; align-items: center; justify-content: center; }\n";
    html << "            .mobile-overlay.active { display: block; }\n";
    html << "            .content-header, .content-body, .footer { padding-left: 20px; padding-right: 20px; }\n";
    html << "            .content-header { padding-top: 32px; padding-bottom: 32px; }\n";
    html << "            .content-header h1 { font-size: 24px; }\n";
    html << "            .items-grid { grid-template-columns: 1fr; }\n";
    html << "            .param-row { flex-wrap: wrap; gap: 6px; }\n";
    html << "            .param-name { min-width: unset; }\n";
    html << "            .param-type { min-width: unset; }\n";
    html << "            .filter-bar { flex-direction: column; align-items: stretch; }\n";
    html << "            .results-count { margin-left: 0; text-align: center; }\n";
    html << "        }\n";
}

static void writeHtmlCssScrollbarAndAnimations(std::ofstream& html) {
    html << "        ::-webkit-scrollbar { width: 8px; height: 8px; }\n";
    html << "        ::-webkit-scrollbar-track { background: transparent; }\n";
    html << "        ::-webkit-scrollbar-thumb { background: var(--border-medium); border-radius: 4px; }\n";
    html << "        ::-webkit-scrollbar-thumb:hover { background: var(--text-muted); }\n";
    html << "        @keyframes fadeIn { from { opacity: 0; transform: translateY(10px); } to { opacity: 1; transform: translateY(0); } }\n";
    html << "        .category-section { animation: fadeIn 0.4s ease forwards; }\n";
    html << "        .highlight { background: rgba(249, 115, 22, 0.3); padding: 1px 4px; border-radius: 3px; }\n";
    html << "    </style>\n";
    html << "</head>\n";
}

static void writeHtmlBodyStart(std::ofstream& html) {
    html << "<body>\n";
    html << "    <div class=\"app-container\">\n";
    html << "        <div class=\"mobile-overlay\" id=\"mobileOverlay\" onclick=\"toggleSidebar()\"></div>\n";
    html << "        <aside class=\"sidebar\" id=\"sidebar\">\n";
    html << "            <div class=\"sidebar-header\">\n";
    html << "                <a href=\"#\" class=\"logo\">\n";
    html << "                    <div class=\"logo-icon\">VP</div>\n";
    html << "                    <span class=\"logo-text\">VisionPipe</span>\n";
    html << "                </a>\n";
    html << "                <div class=\"version-badge\">v1.0.0</div>\n";
    html << "            </div>\n";
    html << "            <div class=\"sidebar-search\">\n";
    html << "                <div class=\"search-input-wrapper\">\n";
    html << "                    <svg class=\"search-icon\" width=\"16\" height=\"16\" fill=\"none\" stroke=\"currentColor\" viewBox=\"0 0 24 24\">\n";
    html << "                        <circle cx=\"11\" cy=\"11\" r=\"8\"></circle>\n";
    html << "                        <path d=\"m21 21-4.35-4.35\"></path>\n";
    html << "                    </svg>\n";
    html << "                    <input type=\"text\" class=\"search-input\" id=\"searchInput\" placeholder=\"Search functions...\" onkeyup=\"filterItems()\" />\n";
    html << "                    <span class=\"search-shortcut\">Ctrl K</span>\n";
    html << "                </div>\n";
    html << "            </div>\n";
    html << "            <nav class=\"sidebar-nav\">\n";
    html << "                <div class=\"nav-section\">\n";
    html << "                    <div class=\"nav-section-title\">Categories</div>\n";
}

static void writeHtmlSidebarNav(std::ofstream& html, const std::map<std::string, std::vector<InterpreterItem*>>& categories) {
    for (const auto& [category, items] : categories) {
        html << "                    <a class=\"nav-item\" href=\"#" << category 
             << "\" onclick=\"filterByCategory('" << category << "')\">\n";
        html << "                        <span>" << category << "</span>\n";
        html << "                        <span class=\"nav-item-count\">" << items.size() << "</span>\n";
        html << "                    </a>\n";
    }
}

static void writeHtmlSidebarFooter(std::ofstream& html, size_t totalItems, size_t categoryCount) {
    html << "                </div>\n";
    html << "            </nav>\n";
    html << "            <div class=\"sidebar-footer\">\n";
    html << "                <div class=\"sidebar-stats\">\n";
    html << "                    <div class=\"stat-item\">\n";
    html << "                        <div class=\"stat-value\">" << totalItems << "</div>\n";
    html << "                        <div class=\"stat-label\">Functions</div>\n";
    html << "                    </div>\n";
    html << "                    <div class=\"stat-item\">\n";
    html << "                        <div class=\"stat-value\">" << categoryCount << "</div>\n";
    html << "                        <div class=\"stat-label\">Categories</div>\n";
    html << "                    </div>\n";
    html << "                </div>\n";
    html << "            </div>\n";
    html << "        </aside>\n";
}

static void writeHtmlMainContentHeader(std::ofstream& html) {
    html << "        <main class=\"main-content\">\n";
    html << "            <header class=\"content-header\">\n";
    html << "                <h1>API Reference</h1>\n";
    html << "                <p>Complete documentation for all VisionPipe functions, operators, and built-in items.</p>\n";
    html << "            </header>\n";
    html << "            <div class=\"content-body\">\n";
    html << "                <div class=\"filter-bar\">\n";
    html << "                    <button class=\"filter-chip active\" onclick=\"filterByCategory('all')\" data-category=\"all\">All</button>\n";
}

static void writeHtmlFilterChips(std::ofstream& html, const std::map<std::string, std::vector<InterpreterItem*>>& categories) {
    for (const auto& [category, items] : categories) {
        (void)items;
        html << "                    <button class=\"filter-chip\" onclick=\"filterByCategory('" 
             << category << "')\" data-category=\"" << category << "\">" << category << "</button>\n";
    }
    html << "                    <span class=\"results-count\" id=\"resultsCount\"></span>\n";
    html << "                </div>\n";
    html << "                <div id=\"contentSections\">\n";
}

static void writeHtmlItems(std::ofstream& html, const std::map<std::string, std::vector<InterpreterItem*>>& categories) {
    for (const auto& [category, items] : categories) {
        html << "                    <section class=\"category-section\" id=\"" << category << "\" data-category=\"" << category << "\">\n";
        html << "                        <div class=\"category-header\">\n";
        html << "                            <div class=\"category-icon\">&#128736;</div>\n";
        html << "                            <h2 class=\"category-title\">" << category << "</h2>\n";
        html << "                            <span class=\"category-count\">" << items.size() << " items</span>\n";
        html << "                        </div>\n";
        html << "                        <div class=\"items-grid\">\n";
        
        for (auto* item : items) {
            html << "                            <div class=\"item-card\" data-name=\"" << item->functionName() 
                 << "\" data-category=\"" << category << "\">\n";
            html << "                                <div class=\"item-header\">\n";
            html << "                                    <span class=\"item-name\">" << item->functionName() << "</span>\n";
            html << "                                </div>\n";
            html << "                                <p class=\"item-desc\">" << escapeHtml(item->description()) << "</p>\n";
            
            auto params = item->params();
            if (!params.empty()) {
                html << "                                <div class=\"item-params\">\n";
                html << "                                    <div class=\"params-title\">Parameters</div>\n";
                for (const auto& p : params) {
                    html << "                                    <div class=\"param-row\">\n";
                    html << "                                        <span class=\"param-name\">" << p.name << "</span>\n";
                    html << "                                        <span class=\"param-type\">" << baseTypeToString(p.type) << "</span>\n";
                    html << "                                        <span class=\"param-desc\">" << escapeHtml(p.description);
                    if (p.defaultValue.has_value()) {
                        html << " <em style=\"color:var(--text-muted)\">(default: " << p.defaultValue->debugString() << ")</em>";
                    }
                    html << "</span>\n";
                    if (!p.isOptional) {
                        html << "                                        <span class=\"param-required\">*</span>\n";
                    } else {
                        html << "                                        <span class=\"param-optional\">optional</span>\n";
                    }
                    html << "                                    </div>\n";
                }
                html << "                                </div>\n";
            }
            
            html << "                                <div class=\"item-example\">" << escapeHtml(item->example()) << "</div>\n";
            
            auto tags = item->tags();
            if (!tags.empty()) {
                html << "                                <div class=\"item-tags\">\n";
                for (const auto& tag : tags) {
                    html << "                                    <span class=\"tag\">" << tag << "</span>\n";
                }
                html << "                                </div>\n";
            }
            
            html << "                            </div>\n";
        }
        
        html << "                        </div>\n";
        html << "                    </section>\n";
    }
}

static void writeHtmlContentFooter(std::ofstream& html) {
    html << "                </div>\n";
    html << "                <div class=\"empty-state\" id=\"emptyState\" style=\"display: none;\">\n";
    html << "                    <div class=\"empty-state-icon\">&#128269;</div>\n";
    html << "                    <p class=\"empty-state-text\">No functions found matching your search.</p>\n";
    html << "                </div>\n";
    html << "            </div>\n";
    html << "            <footer class=\"footer\">\n";
    html << "                <div class=\"footer-content\">\n";
    html << "                    <span class=\"footer-text\">Generated by VisionPipe Documentation</span>\n";
    html << "                    <span class=\"footer-text\">\n";
    html << "                        <a href=\"https://github.com/visionpipe\" class=\"footer-link\">GitHub</a>\n";
    html << "                    </span>\n";
    html << "                </div>\n";
    html << "            </footer>\n";
    html << "        </main>\n";
    html << "        <button class=\"mobile-menu-btn\" onclick=\"toggleSidebar()\">\n";
    html << "            <svg width=\"24\" height=\"24\" fill=\"none\" stroke=\"currentColor\" viewBox=\"0 0 24 24\">\n";
    html << "                <path stroke-linecap=\"round\" stroke-linejoin=\"round\" stroke-width=\"2\" d=\"M4 6h16M4 12h16M4 18h16\"></path>\n";
    html << "            </svg>\n";
    html << "        </button>\n";
    html << "    </div>\n";
}

// Helper to escape strings for JSON
static std::string escapeJson(const std::string& str) {
    std::string result;
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

// Generate search index as inline JavaScript
static void writeSearchIndex(std::ofstream& html, const std::map<std::string, std::vector<InterpreterItem*>>& categories) {
    html << "    <script>\n";
    html << "    // Search index for BM25\n";
    html << "    const searchIndex = [\n";
    
    bool first = true;
    for (const auto& [category, items] : categories) {
        for (auto* item : items) {
            if (!first) html << ",\n";
            first = false;
            
            // Build searchable text from all fields
            std::string paramText;
            for (const auto& p : item->params()) {
                paramText += " " + p.name + " " + p.description;
            }
            std::string tagText;
            for (const auto& t : item->tags()) {
                tagText += " " + t;
            }
            
            html << "        {\n";
            html << "            id: \"" << escapeJson(item->functionName()) << "\",\n";
            html << "            name: \"" << escapeJson(item->functionName()) << "\",\n";
            html << "            category: \"" << escapeJson(category) << "\",\n";
            html << "            description: \"" << escapeJson(item->description()) << "\",\n";
            html << "            example: \"" << escapeJson(item->example()) << "\",\n";
            html << "            params: \"" << escapeJson(paramText) << "\",\n";
            html << "            tags: \"" << escapeJson(tagText) << "\"\n";
            html << "        }";
        }
    }
    
    html << "\n    ];\n";
    html << "    </script>\n";
}

static void writeHtmlScript(std::ofstream& html, const std::map<std::string, std::vector<InterpreterItem*>>& categories) {
    // First write the search index
    writeSearchIndex(html, categories);
    
    // BM25 Implementation
    html << "    <script>\n";
    html << "    // ================================================================\n";
    html << "    // BM25 Search Implementation\n";
    html << "    // ================================================================\n";
    html << "    class BM25 {\n";
    html << "        constructor(documents, fields, options = {}) {\n";
    html << "            this.k1 = options.k1 || 1.5;\n";
    html << "            this.b = options.b || 0.75;\n";
    html << "            this.fields = fields;\n";
    html << "            this.fieldWeights = options.fieldWeights || {};\n";
    html << "            this.documents = documents;\n";
    html << "            this.docCount = documents.length;\n";
    html << "            this.avgDocLength = 0;\n";
    html << "            this.docLengths = [];\n";
    html << "            this.termFreqs = [];\n";
    html << "            this.docFreqs = {};\n";
    html << "            this.idf = {};\n";
    html << "            this._buildIndex();\n";
    html << "        }\n";
    html << "        \n";
    html << "        _tokenize(text) {\n";
    html << "            if (!text) return [];\n";
    html << "            return text.toLowerCase()\n";
    html << "                .replace(/[^a-z0-9_]/g, ' ')\n";
    html << "                .split(/\\s+/)\n";
    html << "                .filter(t => t.length > 1);\n";
    html << "        }\n";
    html << "        \n";
    html << "        _buildIndex() {\n";
    html << "            let totalLength = 0;\n";
    html << "            this.documents.forEach((doc, idx) => {\n";
    html << "                const termFreq = {};\n";
    html << "                let docLength = 0;\n";
    html << "                this.fields.forEach(field => {\n";
    html << "                    const tokens = this._tokenize(doc[field]);\n";
    html << "                    const weight = this.fieldWeights[field] || 1;\n";
    html << "                    tokens.forEach(token => {\n";
    html << "                        termFreq[token] = (termFreq[token] || 0) + weight;\n";
    html << "                        docLength += weight;\n";
    html << "                    });\n";
    html << "                });\n";
    html << "                this.termFreqs.push(termFreq);\n";
    html << "                this.docLengths.push(docLength);\n";
    html << "                totalLength += docLength;\n";
    html << "                Object.keys(termFreq).forEach(term => {\n";
    html << "                    this.docFreqs[term] = (this.docFreqs[term] || 0) + 1;\n";
    html << "                });\n";
    html << "            });\n";
    html << "            this.avgDocLength = totalLength / this.docCount || 1;\n";
    html << "            Object.keys(this.docFreqs).forEach(term => {\n";
    html << "                const df = this.docFreqs[term];\n";
    html << "                this.idf[term] = Math.log((this.docCount - df + 0.5) / (df + 0.5) + 1);\n";
    html << "            });\n";
    html << "        }\n";
    html << "        \n";
    html << "        search(query) {\n";
    html << "            const queryTerms = this._tokenize(query);\n";
    html << "            if (queryTerms.length === 0) return [];\n";
    html << "            const scores = [];\n";
    html << "            this.documents.forEach((doc, idx) => {\n";
    html << "                let score = 0;\n";
    html << "                const docLength = this.docLengths[idx];\n";
    html << "                const termFreq = this.termFreqs[idx];\n";
    html << "                queryTerms.forEach(term => {\n";
    html << "                    const tf = termFreq[term] || 0;\n";
    html << "                    if (tf === 0) return;\n";
    html << "                    const idf = this.idf[term] || 0;\n";
    html << "                    const numerator = tf * (this.k1 + 1);\n";
    html << "                    const denominator = tf + this.k1 * (1 - this.b + this.b * (docLength / this.avgDocLength));\n";
    html << "                    score += idf * (numerator / denominator);\n";
    html << "                });\n";
    html << "                // Boost exact name matches\n";
    html << "                if (doc.name.toLowerCase().includes(query.toLowerCase())) {\n";
    html << "                    score += 5;\n";
    html << "                }\n";
    html << "                if (doc.name.toLowerCase() === query.toLowerCase()) {\n";
    html << "                    score += 10;\n";
    html << "                }\n";
    html << "                if (score > 0) {\n";
    html << "                    scores.push({ doc, score, idx });\n";
    html << "                }\n";
    html << "            });\n";
    html << "            scores.sort((a, b) => b.score - a.score);\n";
    html << "            return scores;\n";
    html << "        }\n";
    html << "    }\n";
    html << "    \n";
    html << "    // Initialize BM25 search engine\n";
    html << "    const bm25 = new BM25(searchIndex, ['name', 'description', 'params', 'tags', 'example'], {\n";
    html << "        k1: 1.5,\n";
    html << "        b: 0.75,\n";
    html << "        fieldWeights: { name: 3, description: 2, params: 1.5, tags: 1.5, example: 1 }\n";
    html << "    });\n";
    html << "    \n";
    html << "    // ================================================================\n";
    html << "    // UI State and Functions\n";
    html << "    // ================================================================\n";
    html << "    let currentCategory = 'all';\n";
    html << "    let searchQuery = '';\n";
    html << "    let searchResults = null;\n";
    html << "    \n";
    html << "    document.addEventListener('DOMContentLoaded', () => {\n";
    html << "        updateResultsCount();\n";
    html << "        document.addEventListener('keydown', (e) => {\n";
    html << "            if ((e.ctrlKey || e.metaKey) && e.key === 'k') {\n";
    html << "                e.preventDefault();\n";
    html << "                document.getElementById('searchInput').focus();\n";
    html << "            }\n";
    html << "            if (e.key === 'Escape') {\n";
    html << "                document.getElementById('searchInput').blur();\n";
    html << "                closeSidebar();\n";
    html << "            }\n";
    html << "        });\n";
    html << "    });\n";
    html << "    \n";
    html << "    function filterItems() {\n";
    html << "        searchQuery = document.getElementById('searchInput').value.trim();\n";
    html << "        if (searchQuery.length > 0) {\n";
    html << "            searchResults = bm25.search(searchQuery);\n";
    html << "        } else {\n";
    html << "            searchResults = null;\n";
    html << "        }\n";
    html << "        applyFilters();\n";
    html << "    }\n";
    html << "    \n";
    html << "    function filterByCategory(category) {\n";
    html << "        currentCategory = category;\n";
    html << "        document.querySelectorAll('.filter-chip').forEach(chip => {\n";
    html << "            chip.classList.toggle('active', chip.dataset.category === category);\n";
    html << "        });\n";
    html << "        document.querySelectorAll('.nav-item').forEach(item => {\n";
    html << "            const href = item.getAttribute('href');\n";
    html << "            if (href) {\n";
    html << "                const cat = href.replace('#', '');\n";
    html << "                item.classList.toggle('active', cat === category || category === 'all');\n";
    html << "            }\n";
    html << "        });\n";
    html << "        applyFilters();\n";
    html << "        closeSidebar();\n";
    html << "    }\n";
    html << "    \n";
    html << "    function applyFilters() {\n";
    html << "        let visibleCount = 0;\n";
    html << "        let visibleSections = new Set();\n";
    html << "        const matchedIds = searchResults ? new Set(searchResults.map(r => r.doc.id)) : null;\n";
    html << "        const scoreMap = searchResults ? new Map(searchResults.map(r => [r.doc.id, r.score])) : null;\n";
    html << "        \n";
    html << "        // Collect all cards for potential reordering\n";
    html << "        const cards = Array.from(document.querySelectorAll('.item-card'));\n";
    html << "        \n";
    html << "        cards.forEach(card => {\n";
    html << "            const name = card.dataset.name;\n";
    html << "            const category = card.dataset.category;\n";
    html << "            const matchesSearch = !matchedIds || matchedIds.has(name);\n";
    html << "            const matchesCategory = currentCategory === 'all' || category === currentCategory;\n";
    html << "            const isVisible = matchesSearch && matchesCategory;\n";
    html << "            card.style.display = isVisible ? '' : 'none';\n";
    html << "            if (isVisible) {\n";
    html << "                visibleCount++;\n";
    html << "                visibleSections.add(category);\n";
    html << "                // Add score indicator for search results\n";
    html << "                if (scoreMap && scoreMap.has(name)) {\n";
    html << "                    card.style.order = Math.round(-scoreMap.get(name) * 100);\n";
    html << "                } else {\n";
    html << "                    card.style.order = '';\n";
    html << "                }\n";
    html << "            }\n";
    html << "        });\n";
    html << "        \n";
    html << "        document.querySelectorAll('.category-section').forEach(section => {\n";
    html << "            const category = section.dataset.category;\n";
    html << "            const hasVisibleItems = visibleSections.has(category);\n";
    html << "            const matchesCategoryFilter = currentCategory === 'all' || category === currentCategory;\n";
    html << "            section.style.display = (hasVisibleItems && matchesCategoryFilter) ? '' : 'none';\n";
    html << "        });\n";
    html << "        \n";
    html << "        document.getElementById('emptyState').style.display = visibleCount === 0 ? '' : 'none';\n";
    html << "        updateResultsCount(visibleCount);\n";
    html << "    }\n";
    html << "    \n";
    html << "    function updateResultsCount(count) {\n";
    html << "        const total = document.querySelectorAll('.item-card').length;\n";
    html << "        count = count !== undefined ? count : total;\n";
    html << "        const el = document.getElementById('resultsCount');\n";
    html << "        if (searchResults && searchQuery) {\n";
    html << "            el.textContent = count + ' results for \"' + searchQuery + '\"';\n";
    html << "        } else if (count === total) {\n";
    html << "            el.textContent = total + ' functions';\n";
    html << "        } else {\n";
    html << "            el.textContent = 'Showing ' + count + ' of ' + total + ' functions';\n";
    html << "        }\n";
    html << "    }\n";
    html << "    \n";
    html << "    function toggleSidebar() {\n";
    html << "        document.getElementById('sidebar').classList.toggle('open');\n";
    html << "        document.getElementById('mobileOverlay').classList.toggle('active');\n";
    html << "    }\n";
    html << "    \n";
    html << "    function closeSidebar() {\n";
    html << "        document.getElementById('sidebar').classList.remove('open');\n";
    html << "        document.getElementById('mobileOverlay').classList.remove('active');\n";
    html << "    }\n";
    html << "    \n";
    html << "    document.querySelectorAll('.nav-item[href^=\"#\"]').forEach(link => {\n";
    html << "        link.addEventListener('click', (e) => {\n";
    html << "            const category = link.getAttribute('href').replace('#', '');\n";
    html << "            filterByCategory(category);\n";
    html << "            const section = document.getElementById(category);\n";
    html << "            if (section) {\n";
    html << "                e.preventDefault();\n";
    html << "                section.scrollIntoView({ behavior: 'smooth', block: 'start' });\n";
    html << "            }\n";
    html << "        });\n";
    html << "    });\n";
    html << "    </script>\n";
    html << "</body>\n";
    html << "</html>\n";
}

// ============================================================================
// HTML Generation
// ============================================================================

static int generateHtmlDocs(const DocGenOptions& opts, 
                            const std::map<std::string, std::vector<InterpreterItem*>>& categories,
                            size_t totalItems) {
    std::string htmlPath = opts.outputDir + "/visionpipe-docs.html";
    std::ofstream html(htmlPath);
    
    if (!html.is_open()) {
        std::cerr << "Error: Cannot create " << htmlPath << std::endl;
        return 1;
    }
    
    // Write HTML document in chunks to avoid MSVC string literal limits
    writeHtmlHead(html);
    writeHtmlCssVariables(html);
    writeHtmlCssBase(html);
    writeHtmlCssSidebar(html);
    writeHtmlCssSearch(html);
    writeHtmlCssNav(html);
    writeHtmlCssMainContent(html);
    writeHtmlCssFilterBar(html);
    writeHtmlCssCategory(html);
    writeHtmlCssItemCard(html);
    writeHtmlCssParams(html);
    writeHtmlCssExample(html);
    writeHtmlCssFooter(html);
    writeHtmlCssMobile(html);
    writeHtmlCssResponsive(html);
    writeHtmlCssScrollbarAndAnimations(html);
    writeHtmlBodyStart(html);
    writeHtmlSidebarNav(html, categories);
    writeHtmlSidebarFooter(html, totalItems, categories.size());
    writeHtmlMainContentHeader(html);
    writeHtmlFilterChips(html, categories);
    writeHtmlItems(html, categories);
    writeHtmlContentFooter(html);
    writeHtmlScript(html, categories);
    
    html.close();
    std::cout << "[Docs] Generated: " << htmlPath << std::endl;
    return 0;
}

// ============================================================================
// Markdown Generation
// ============================================================================

static int generateMdDocs(const DocGenOptions& opts,
                          const std::map<std::string, std::vector<InterpreterItem*>>& categories) {
    std::string mdPath = opts.outputDir + "/visionpipe-docs.md";
    std::ofstream md(mdPath);
    
    if (!md.is_open()) {
        std::cerr << "Error: Cannot create " << mdPath << std::endl;
        return 1;
    }
    
    md << "# VisionPipe Documentation\n\n";
    md << "Auto-generated documentation for VisionPipe interpreter items.\n\n";
    md << "---\n\n";
    md << "## Table of Contents\n\n";
    
    for (const auto& [category, items] : categories) {
        md << "- [" << category << "](#" << category << ") (" << items.size() << " items)\n";
    }
    md << "\n---\n\n";
    
    for (const auto& [category, items] : categories) {
        md << "## " << category << "\n\n";
        
        for (auto* item : items) {
            md << "### `" << item->functionName() << "()`\n\n";
            md << item->description() << "\n\n";
            
            auto params = item->params();
            if (!params.empty()) {
                md << "**Parameters:**\n\n";
                md << "| Name | Type | Required | Description |\n";
                md << "|------|------|----------|-------------|\n";
                for (const auto& p : params) {
                    md << "| `" << p.name << "` | " 
                       << baseTypeToString(p.type) << " | "
                       << (!p.isOptional ? "Yes" : "No") << " | "
                       << p.description;
                    if (p.defaultValue.has_value()) {
                        md << " (default: `" << p.defaultValue->debugString() << "`)";
                    }
                    md << " |\n";
                }
                md << "\n";
            }
            
            md << "**Example:**\n\n```vsp\n" << item->example() << "\n```\n\n";
            
            auto tags = item->tags();
            if (!tags.empty()) {
                md << "**Tags:** ";
                for (size_t i = 0; i < tags.size(); i++) {
                    md << "`" << tags[i] << "`";
                    if (i < tags.size() - 1) md << ", ";
                }
                md << "\n\n";
            }
            
            md << "---\n\n";
        }
    }
    
    md.close();
    std::cout << "[Docs] Generated: " << mdPath << std::endl;
    return 0;
}

// ============================================================================
// JSON Generation
// ============================================================================

static int generateJsonDocs(const DocGenOptions& opts,
                            const std::map<std::string, std::vector<InterpreterItem*>>& categories) {
    std::string jsonPath = opts.outputDir + "/visionpipe-docs.json";
    std::ofstream json(jsonPath);
    
    if (!json.is_open()) {
        std::cerr << "Error: Cannot create " << jsonPath << std::endl;
        return 1;
    }
    
    json << "{\n";
    json << "  \"version\": \"1.0.0\",\n";
    json << "  \"categories\": {\n";
    
    bool firstCat = true;
    for (const auto& [category, items] : categories) {
        if (!firstCat) json << ",\n";
        json << "    \"" << category << "\": [\n";
        
        bool firstItem = true;
        for (auto* item : items) {
            if (!firstItem) json << ",\n";
            json << "      {\n";
            json << "        \"name\": \"" << item->functionName() << "\",\n";
            json << "        \"description\": \"" << item->description() << "\",\n";
            json << "        \"example\": \"" << item->example() << "\",\n";
            json << "        \"params\": [\n";
            
            auto params = item->params();
            for (size_t i = 0; i < params.size(); i++) {
                const auto& p = params[i];
                json << "          {\"name\": \"" << p.name << "\", "
                     << "\"type\": \"" << baseTypeToString(p.type) << "\", "
                     << "\"required\": " << (!p.isOptional ? "true" : "false") << ", "
                     << "\"description\": \"" << p.description << "\"}";
                if (i < params.size() - 1) json << ",";
                json << "\n";
            }
            json << "        ]\n";
            json << "      }";
            firstItem = false;
        }
        json << "\n    ]";
        firstCat = false;
    }
    
    json << "\n  }\n}\n";
    json.close();
    std::cout << "[Docs] Generated: " << jsonPath << std::endl;
    return 0;
}

// ============================================================================
// Main Documentation Generator
// ============================================================================

int generateDocs(const DocGenOptions& opts) {
    std::cout << "[VisionPipe] Generating documentation..." << std::endl;
    
    // Create runtime to get registered items
    Runtime runtime;
    runtime.loadBuiltins();
    
    // Get item registry from interpreter
    auto& registry = runtime.interpreter().registry();
    
    // Collect items by category
    std::map<std::string, std::vector<InterpreterItem*>> categories;
    auto itemNames = registry.getItemNames();
    
    for (const auto& name : itemNames) {
        auto item = registry.getItem(name);
        if (item) {
            categories[item->category()].push_back(item.get());
        }
    }
    
    createDirectory(opts.outputDir);
    
    int result = 0;
    
    if (opts.outputFormat == "html") {
        result = generateHtmlDocs(opts, categories, itemNames.size());
    } else if (opts.outputFormat == "md") {
        result = generateMdDocs(opts, categories);
    } else if (opts.outputFormat == "json") {
        result = generateJsonDocs(opts, categories);
    } else {
        std::cerr << "Error: Unknown output format: " << opts.outputFormat << std::endl;
        return 1;
    }
    
    if (result == 0) {
        std::cout << "[Docs] Total items documented: " << itemNames.size() << std::endl;
    }
    
    return result;
}

}  // namespace visionpipe
