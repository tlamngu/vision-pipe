/**
 * gen_vscode_ext – VisionPipe VS Code Extension Generator
 *
 * Reads the live ItemRegistry (all built-in VSP functions with their metadata)
 * and regenerates:
 *   - syntaxes/vsp.tmLanguage.json   (TextMate grammar – built-in highlights)
 *   - snippets/vsp.json              (IntelliSense snippets)
 *
 * The static sections (keywords, execution directives, param-refs, …) are
 * embedded here as a template.  Only the "builtins" patterns and snippet
 * bodies are derived from the registry.
 *
 * Usage:
 *   gen_vscode_ext [--out-dir <path/to/vscode-vsp>] [--dry-run]
 *   gen_vscode_ext --help
 */

#include "interpreter/item_registry.h"
#include "interpreter/items/all_items.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** JSON-escape a plain string (no outer quotes). */
std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

/** Escape a string for use inside a regex character class / alternation. */
std::string regexEscape(const std::string& s) {
    static const std::string meta = R"(\.^$|?*+()[]{}/)";
    std::string out;
    for (char c : s) {
        if (meta.find(c) != std::string::npos) out += '\\';
        out += c;
    }
    return out;
}

/** Convert snake_case or camelCase to "Title Case Words". */
std::string toTitleCase(const std::string& s) {
    std::string out;
    bool capitalize = true;
    for (char c : s) {
        if (c == '_') {
            out += ' ';
            capitalize = true;
        } else if (capitalize) {
            out += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            capitalize = false;
        } else {
            out += c;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Snippet body builder
// ---------------------------------------------------------------------------
/**
 * Convert an example call string like:
 *   find_chessboard_corners(9, 6, "adaptive_thresh+normalize_image", "corners")
 * into a VS Code snippet body string:
 *   "find_chessboard_corners(${1:9}, ${2:6}, ${3:adaptive_thresh+normalize_image}, ${4:corners})"
 *
 * Falls back to building from ParamDefs if no example is given.
 */
std::string buildSnippetBody(const visionpipe::InterpreterItem& item) {
    const std::string& ex = item.example();

    // If the example has a pipe separator, take the first call only.
    auto example = ex;
    auto pipePos = example.find(" | ");
    if (pipePos != std::string::npos) example = example.substr(0, pipePos);

    // Split on first '(' to get function name and args string.
    auto parenPos = example.find('(');
    if (parenPos == std::string::npos || example.find(')') == std::string::npos) {
        // Fall back: build from params
        std::string body = item.functionName() + "(";
        for (size_t i = 0; i < item.params().size(); ++i) {
            if (i > 0) body += ", ";
            body += "${" + std::to_string(i + 1) + ":" + item.params()[i].name + "}";
        }
        body += ")";
        return body;
    }

    // Extract arguments between outermost parens.
    auto closeParen = example.rfind(')');
    std::string argsStr = example.substr(parenPos + 1, closeParen - parenPos - 1);

    // Split args respecting nested parens, brackets, and quoted strings.
    std::vector<std::string> args;
    int depth = 0;
    bool inStr = false;
    char strChar = 0;
    std::string cur;
    for (size_t i = 0; i < argsStr.size(); ++i) {
        char c = argsStr[i];
        if (!inStr && (c == '"' || c == '\'')) {
            inStr = true; strChar = c; cur += c;
        } else if (inStr && c == strChar && (i == 0 || argsStr[i-1] != '\\')) {
            inStr = false; cur += c;
        } else if (!inStr && (c == '(' || c == '[')) {
            depth++; cur += c;
        } else if (!inStr && (c == ')' || c == ']')) {
            depth--; cur += c;
        } else if (!inStr && depth == 0 && c == ',') {
            // Trim whitespace
            while (!cur.empty() && cur.front() == ' ') cur.erase(cur.begin());
            while (!cur.empty() && cur.back() == ' ') cur.pop_back();
            args.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    while (!cur.empty() && cur.front() == ' ') cur.erase(cur.begin());
    while (!cur.empty() && cur.back() == ' ') cur.pop_back();
    if (!cur.empty()) args.push_back(cur);

    // Strip outer quotes from string args (will add back without quotes for readability in tabstop).
    auto stripOuter = [](std::string s) -> std::string {
        if (s.size() >= 2 &&
            ((s.front() == '"' && s.back() == '"') ||
             (s.front() == '\'' && s.back() == '\''))) {
            return s.substr(1, s.size() - 2);
        }
        return s;
    };

    // Build snippet body.
    std::string funcName = example.substr(0, parenPos);
    // Trim whitespace from funcName.
    while (!funcName.empty() && funcName.back() == ' ') funcName.pop_back();

    std::string body = funcName + "(";
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) body += ", ";
        std::string placeholder = jsonEscape(stripOuter(args[i]));
        body += "${" + std::to_string(i + 1) + ":" + placeholder + "}";
    }
    body += ")";
    return body;
}

// ---------------------------------------------------------------------------
// Grammar generator
// ---------------------------------------------------------------------------
std::string generateGrammar(const visionpipe::ItemRegistry& registry) {
    // Collect categories → sorted function names.
    std::map<std::string, std::vector<std::string>> byCat;
    for (const auto& name : registry.getItemNames()) {
        auto item = registry.getItem(name);
        if (!item) continue;
        byCat[item->category()].push_back(name);
    }
    for (auto& [cat, names] : byCat) {
        std::sort(names.begin(), names.end());
    }

    auto indent = [](int n) { return std::string(n * 2, ' '); };

    std::ostringstream o;
    o << "{\n";
    o << indent(1) << "\"$schema\": \"https://raw.githubusercontent.com/martinring/tmlanguage/master/tmlanguage.json\",\n";
    o << indent(1) << "\"name\": \"VisionPipe Script\",\n";
    o << indent(1) << "\"scopeName\": \"source.vsp\",\n";
    o << indent(1) << "\"patterns\": [\n";
    o << indent(2) << "{ \"include\": \"#comments\" },\n";
    o << indent(2) << "{ \"include\": \"#param-refs\" },\n";
    o << indent(2) << "{ \"include\": \"#booleans\" },\n";
    o << indent(2) << "{ \"include\": \"#keywords\" },\n";
    o << indent(2) << "{ \"include\": \"#execution\" },\n";
    o << indent(2) << "{ \"include\": \"#debug-blocks\" },\n";
    o << indent(2) << "{ \"include\": \"#cache-operators\" },\n";
    o << indent(2) << "{ \"include\": \"#builtins\" },\n";
    o << indent(2) << "{ \"include\": \"#strings\" },\n";
    o << indent(2) << "{ \"include\": \"#numbers\" },\n";
    o << indent(2) << "{ \"include\": \"#operators\" },\n";
    o << indent(2) << "{ \"include\": \"#punctuation\" },\n";
    o << indent(2) << "{ \"include\": \"#identifiers\" }\n";
    o << indent(1) << "],\n";
    o << indent(1) << "\"repository\": {\n";

    // --- Static sections ---
    o << indent(2) << "\"comments\": {\n";
    o << indent(3) << "\"patterns\": [\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"comment.line.pound.vsp\",\n";
    o << indent(5) << "\"match\": \"#.*$\"\n";
    o << indent(4) << "}\n";
    o << indent(3) << "]\n";
    o << indent(2) << "},\n";

    o << indent(2) << "\"param-refs\": {\n";
    o << indent(3) << "\"comment\": \"Runtime parameter references: @param_name\",\n";
    o << indent(3) << "\"patterns\": [\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"variable.language.param-ref.vsp\",\n";
    o << indent(5) << "\"match\": \"@[a-zA-Z_][a-zA-Z0-9_]*\"\n";
    o << indent(4) << "}\n";
    o << indent(3) << "]\n";
    o << indent(2) << "},\n";

    o << indent(2) << "\"booleans\": {\n";
    o << indent(3) << "\"patterns\": [\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"constant.language.boolean.vsp\",\n";
    o << indent(5) << "\"match\": \"\\\\b(true|false)\\\\b\"\n";
    o << indent(4) << "}\n";
    o << indent(3) << "]\n";
    o << indent(2) << "},\n";

    o << indent(2) << "\"keywords\": {\n";
    o << indent(3) << "\"patterns\": [\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"keyword.control.vsp\",\n";
    o << indent(5) << "\"match\": \"\\\\b(pipeline|end|if|else|while|for|break|continue|return|loop)\\\\b\"\n";
    o << indent(4) << "},\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"keyword.control.params.vsp\",\n";
    o << indent(5) << "\"match\": \"\\\\b(params|on_params)\\\\b\"\n";
    o << indent(4) << "},\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"keyword.other.vsp\",\n";
    o << indent(5) << "\"match\": \"\\\\b(import|config|global|let|use|cache)\\\\b\"\n";
    o << indent(4) << "},\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"keyword.operator.word.vsp\",\n";
    o << indent(5) << "\"match\": \"\\\\b(and|or|not)\\\\b\"\n";
    o << indent(4) << "}\n";
    o << indent(3) << "]\n";
    o << indent(2) << "},\n";

    o << indent(2) << "\"execution\": {\n";
    o << indent(3) << "\"comment\": \"All exec_* scheduling and execution directives\",\n";
    o << indent(3) << "\"patterns\": [\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"keyword.control.execution.vsp\",\n";
    o << indent(5) << "\"match\": \"\\\\b(exec_seq|exec_multi|exec_loop|exec_nasync|exec_interval|exec_interval_multi|exec_rt_seq|exec_rt_multi|exec_fork|no_interval)\\\\b\"\n";
    o << indent(4) << "}\n";
    o << indent(3) << "]\n";
    o << indent(2) << "},\n";

    o << indent(2) << "\"debug-blocks\": {\n";
    o << indent(3) << "\"comment\": \"debug_start / debug_end delimiter blocks\",\n";
    o << indent(3) << "\"patterns\": [\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"keyword.control.debug.vsp\",\n";
    o << indent(5) << "\"match\": \"\\\\b(debug_start|debug_end)\\\\b\"\n";
    o << indent(4) << "}\n";
    o << indent(3) << "]\n";
    o << indent(2) << "},\n";

    o << indent(2) << "\"cache-operators\": {\n";
    o << indent(3) << "\"patterns\": [\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"keyword.operator.arrow.vsp\",\n";
    o << indent(5) << "\"match\": \"->\"\n";
    o << indent(4) << "}\n";
    o << indent(3) << "]\n";
    o << indent(2) << "},\n";

    // --- Generated builtins section ---
    o << indent(2) << "\"builtins\": {\n";
    o << indent(3) << "\"comment\": \"Auto-generated from visionpipe::ItemRegistry — do not edit by hand\",\n";
    o << indent(3) << "\"patterns\": [\n";

    bool firstCat = true;
    for (const auto& [cat, names] : byCat) {
        if (!firstCat) o << ",\n";
        firstCat = false;

        // Build alternation regex.
        std::string alt;
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) alt += '|';
            alt += regexEscape(names[i]);
        }

        o << indent(4) << "{\n";
        o << indent(5) << "\"comment\": \"Category: " << jsonEscape(cat) << "\",\n";
        o << indent(5) << "\"name\": \"support.function." << jsonEscape(cat) << ".vsp\",\n";
        o << indent(5) << "\"match\": \"\\\\b(" << alt << ")\\\\b\"\n";
        o << indent(4) << "}";
    }
    o << "\n";

    o << indent(3) << "]\n";
    o << indent(2) << "},\n";

    // --- Static trailing sections ---
    o << indent(2) << "\"strings\": {\n";
    o << indent(3) << "\"patterns\": [\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"string.quoted.double.vsp\",\n";
    o << indent(5) << "\"begin\": \"\\\"\",\n";
    o << indent(5) << "\"end\": \"\\\"\",\n";
    o << indent(5) << "\"patterns\": [\n";
    o << indent(6) << "{\n";
    o << indent(7) << "\"name\": \"constant.character.escape.vsp\",\n";
    o << indent(7) << "\"match\": \"\\\\\\\\.\"\n";
    o << indent(6) << "}\n";
    o << indent(5) << "]\n";
    o << indent(4) << "},\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"string.quoted.single.vsp\",\n";
    o << indent(5) << "\"begin\": \"'\",\n";
    o << indent(5) << "\"end\": \"'\",\n";
    o << indent(5) << "\"patterns\": [\n";
    o << indent(6) << "{\n";
    o << indent(7) << "\"name\": \"constant.character.escape.vsp\",\n";
    o << indent(7) << "\"match\": \"\\\\\\\\.\"\n";
    o << indent(6) << "}\n";
    o << indent(5) << "]\n";
    o << indent(4) << "}\n";
    o << indent(3) << "]\n";
    o << indent(2) << "},\n";

    o << indent(2) << "\"numbers\": {\n";
    o << indent(3) << "\"patterns\": [\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"constant.numeric.hex.vsp\",\n";
    o << indent(5) << "\"match\": \"\\\\b0[xX][0-9a-fA-F]+\\\\b\"\n";
    o << indent(4) << "},\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"constant.numeric.float.vsp\",\n";
    o << indent(5) << "\"match\": \"\\\\b\\\\d+\\\\.\\\\d+([eE][+-]?\\\\d+)?\\\\b\"\n";
    o << indent(4) << "},\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"constant.numeric.integer.vsp\",\n";
    o << indent(5) << "\"match\": \"\\\\b\\\\d+\\\\b\"\n";
    o << indent(4) << "}\n";
    o << indent(3) << "]\n";
    o << indent(2) << "},\n";

    o << indent(2) << "\"operators\": {\n";
    o << indent(3) << "\"patterns\": [\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"keyword.operator.comparison.vsp\",\n";
    o << indent(5) << "\"match\": \"(==|!=|<=|>=|<|>)\"\n";
    o << indent(4) << "},\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"keyword.operator.logical.vsp\",\n";
    o << indent(5) << "\"match\": \"(&&|\\\\|\\\\||!)\"\n";
    o << indent(4) << "},\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"keyword.operator.arithmetic.vsp\",\n";
    o << indent(5) << "\"match\": \"(\\\\+|-|\\\\*|/|%)\"\n";
    o << indent(4) << "},\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"keyword.operator.pipe.vsp\",\n";
    o << indent(5) << "\"match\": \"\\\\|(?!\\\\|)\"\n";
    o << indent(4) << "},\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"keyword.operator.assignment.vsp\",\n";
    o << indent(5) << "\"match\": \"=\"\n";
    o << indent(4) << "}\n";
    o << indent(3) << "]\n";
    o << indent(2) << "},\n";

    o << indent(2) << "\"punctuation\": {\n";
    o << indent(3) << "\"patterns\": [\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"punctuation.separator.comma.vsp\",\n";
    o << indent(5) << "\"match\": \",\"\n";
    o << indent(4) << "},\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"punctuation.definition.parameters.begin.vsp\",\n";
    o << indent(5) << "\"match\": \"\\\\(\"\n";
    o << indent(4) << "},\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"punctuation.definition.parameters.end.vsp\",\n";
    o << indent(5) << "\"match\": \"\\\\)\"\n";
    o << indent(4) << "},\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"punctuation.definition.array.begin.vsp\",\n";
    o << indent(5) << "\"match\": \"\\\\[\"\n";
    o << indent(4) << "},\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"punctuation.definition.array.end.vsp\",\n";
    o << indent(5) << "\"match\": \"\\\\]\"\n";
    o << indent(4) << "}\n";
    o << indent(3) << "]\n";
    o << indent(2) << "},\n";

    o << indent(2) << "\"identifiers\": {\n";
    o << indent(3) << "\"patterns\": [\n";
    o << indent(4) << "{\n";
    o << indent(5) << "\"name\": \"variable.other.vsp\",\n";
    o << indent(5) << "\"match\": \"\\\\b[a-zA-Z_][a-zA-Z0-9_]*\\\\b\"\n";
    o << indent(4) << "}\n";
    o << indent(3) << "]\n";
    o << indent(2) << "}\n";

    o << indent(1) << "}\n";
    o << "}\n";

    return o.str();
}

// ---------------------------------------------------------------------------
// Snippet generator
// ---------------------------------------------------------------------------
std::string buildParamDoc(const visionpipe::InterpreterItem& item) {
    if (item.params().empty()) return item.description();
    std::string doc = item.description() + "\n\nParams:";
    for (const auto& p : item.params()) {
        doc += "\n  " + p.name + " (" + visionpipe::baseTypeToString(p.type) + ")";
        if (!p.description.empty()) doc += ": " + p.description;
        if (p.isOptional) doc += " [optional]";
    }
    if (!item.example().empty()) doc += "\n\nExample: " + item.example();
    return doc;
}

std::string generateSnippets(const visionpipe::ItemRegistry& registry) {
    // Collect items sorted by functionName for deterministic output.
    auto names = registry.getItemNames();
    std::sort(names.begin(), names.end());

    // Map snippet key → JSON body string
    // We also append static hand-crafted snippets for language constructs.

    auto indent = [](int n) { return std::string(n * 2, ' '); };

    std::ostringstream o;
    o << "{\n";

    // ----------------------------------------------------------------
    // Static language-construct snippets
    // ----------------------------------------------------------------
    auto staticSnippets = [&]() {
        // pipeline
        o << indent(1) << "\"Pipeline\": {\n";
        o << indent(2) << "\"prefix\": \"pipeline\",\n";
        o << indent(2) << "\"body\": [\n";
        o << indent(3) << "\"pipeline ${1:name}\",\n";
        o << indent(3) << "\"    $0\",\n";
        o << indent(3) << "\"end\"\n";
        o << indent(2) << "],\n";
        o << indent(2) << "\"description\": \"Create a new pipeline\"\n";
        o << indent(1) << "},\n";

        // pipeline with parameters
        o << indent(1) << "\"Pipeline with Parameters\": {\n";
        o << indent(2) << "\"prefix\": \"pipelinep\",\n";
        o << indent(2) << "\"body\": [\n";
        o << indent(3) << "\"pipeline ${1:name}(${2:params})\",\n";
        o << indent(3) << "\"    $0\",\n";
        o << indent(3) << "\"end\"\n";
        o << indent(2) << "],\n";
        o << indent(2) << "\"description\": \"Create a new pipeline with parameters\"\n";
        o << indent(1) << "},\n";

        // exec_seq
        o << indent(1) << "\"Exec Sequential\": {\n";
        o << indent(2) << "\"prefix\": \"execseq\",\n";
        o << indent(2) << "\"body\": [\"exec_seq ${1:pipeline_name}\"],\n";
        o << indent(2) << "\"description\": \"Execute a pipeline sequentially\"\n";
        o << indent(1) << "},\n";

        // exec_multi
        o << indent(1) << "\"Exec Multi\": {\n";
        o << indent(2) << "\"prefix\": \"execmulti\",\n";
        o << indent(2) << "\"body\": [\n";
        o << indent(3) << "\"exec_multi [\",\n";
        o << indent(3) << "\"    ${1:pipeline1},\",\n";
        o << indent(3) << "\"    ${2:pipeline2}\",\n";
        o << indent(3) << "\"]\"\n";
        o << indent(2) << "],\n";
        o << indent(2) << "\"description\": \"Execute multiple pipelines in parallel\"\n";
        o << indent(1) << "},\n";

        // exec_loop
        o << indent(1) << "\"Exec Loop\": {\n";
        o << indent(2) << "\"prefix\": \"execloop\",\n";
        o << indent(2) << "\"body\": [\"exec_loop ${1:main}\"],\n";
        o << indent(2) << "\"description\": \"Execute a pipeline in a loop\"\n";
        o << indent(1) << "},\n";

        // exec_interval
        o << indent(1) << "\"Exec Interval\": {\n";
        o << indent(2) << "\"prefix\": \"execinterval\",\n";
        o << indent(2) << "\"body\": [\"exec_interval ${1:pipeline_name} ${2:33}\"],\n";
        o << indent(2) << "\"description\": \"Execute pipeline at a fixed interval (ms)\"\n";
        o << indent(1) << "},\n";

        // exec_interval_multi
        o << indent(1) << "\"Exec Interval Multi\": {\n";
        o << indent(2) << "\"prefix\": \"execintervalmulti\",\n";
        o << indent(2) << "\"body\": [\n";
        o << indent(3) << "\"exec_interval_multi [\",\n";
        o << indent(3) << "\"    ${1:pipeline1},\",\n";
        o << indent(3) << "\"    ${2:pipeline2}\",\n";
        o << indent(3) << "\"] ${3:33}\"\n";
        o << indent(2) << "],\n";
        o << indent(2) << "\"description\": \"Execute multiple pipelines at a fixed interval (ms)\"\n";
        o << indent(1) << "},\n";

        // exec_rt_seq
        o << indent(1) << "\"Exec RT Sequential\": {\n";
        o << indent(2) << "\"prefix\": \"execrtseq\",\n";
        o << indent(2) << "\"body\": [\"exec_rt_seq ${1:pipeline_name}\"],\n";
        o << indent(2) << "\"description\": \"Execute pipeline in real-time sequential mode (dedicated thread)\"\n";
        o << indent(1) << "},\n";

        // exec_rt_multi
        o << indent(1) << "\"Exec RT Multi\": {\n";
        o << indent(2) << "\"prefix\": \"execrtmulti\",\n";
        o << indent(2) << "\"body\": [\n";
        o << indent(3) << "\"exec_rt_multi [\",\n";
        o << indent(3) << "\"    ${1:pipeline1},\",\n";
        o << indent(3) << "\"    ${2:pipeline2}\",\n";
        o << indent(3) << "\"]\"\n";
        o << indent(2) << "],\n";
        o << indent(2) << "\"description\": \"Execute multiple pipelines in real-time parallel mode\"\n";
        o << indent(1) << "},\n";

        // exec_nasync
        o << indent(1) << "\"Exec Nasync\": {\n";
        o << indent(2) << "\"prefix\": \"execnasync\",\n";
        o << indent(2) << "\"body\": [\"exec_nasync ${1:pipeline_name}\"],\n";
        o << indent(2) << "\"description\": \"Execute pipeline non-async (detached thread)\"\n";
        o << indent(1) << "},\n";

        // exec_fork
        o << indent(1) << "\"Exec Fork\": {\n";
        o << indent(2) << "\"prefix\": \"execfork\",\n";
        o << indent(2) << "\"body\": [\"exec_fork ${1:pipeline_name}\"],\n";
        o << indent(2) << "\"description\": \"Fork and run pipeline asynchronously without blocking\"\n";
        o << indent(1) << "},\n";

        // no_interval
        o << indent(1) << "\"No Interval\": {\n";
        o << indent(2) << "\"prefix\": \"nointerval\",\n";
        o << indent(2) << "\"body\": [\"no_interval\"],\n";
        o << indent(2) << "\"description\": \"Disable interval throttling for exec_interval\"\n";
        o << indent(1) << "},\n";

        // debug_start/end
        o << indent(1) << "\"Debug Block\": {\n";
        o << indent(2) << "\"prefix\": \"debugblock\",\n";
        o << indent(2) << "\"body\": [\n";
        o << indent(3) << "\"debug_start\",\n";
        o << indent(3) << "\"    $0\",\n";
        o << indent(3) << "\"debug_end\"\n";
        o << indent(2) << "],\n";
        o << indent(2) << "\"description\": \"Wrap code in a debug_start / debug_end block (only active in debug mode)\"\n";
        o << indent(1) << "},\n";

        // let
        o << indent(1) << "\"Let Variable\": {\n";
        o << indent(2) << "\"prefix\": \"let\",\n";
        o << indent(2) << "\"body\": [\"let ${1:name} = ${2:value}\"],\n";
        o << indent(2) << "\"description\": \"Declare a local variable with let\"\n";
        o << indent(1) << "},\n";

        // params block
        o << indent(1) << "\"Runtime Params Declaration\": {\n";
        o << indent(2) << "\"prefix\": \"params\",\n";
        o << indent(2) << "\"body\": [\n";
        o << indent(3) << "\"params [\",\n";
        o << indent(3) << "\"    ${1:param_name}:${2|int,float,bool,string|}=${3:0}$0\",\n";
        o << indent(3) << "\"]\"\n";
        o << indent(2) << "],\n";
        o << indent(2) << "\"description\": \"Declare runtime parameters that can be set externally while the pipeline runs\"\n";
        o << indent(1) << "},\n";

        // on_params
        o << indent(1) << "\"On Params Handler\": {\n";
        o << indent(2) << "\"prefix\": \"onparams\",\n";
        o << indent(2) << "\"body\": [\n";
        o << indent(3) << "\"on_params @${1:param_name}\",\n";
        o << indent(3) << "\"    $0\",\n";
        o << indent(3) << "\"end\"\n";
        o << indent(2) << "],\n";
        o << indent(2) << "\"description\": \"Event handler triggered when a runtime parameter changes\"\n";
        o << indent(1) << "},\n";

        // @param reference
        o << indent(1) << "\"Param Reference\": {\n";
        o << indent(2) << "\"prefix\": \"@\",\n";
        o << indent(2) << "\"body\": [\"@${1:param_name}\"],\n";
        o << indent(2) << "\"description\": \"Reference a runtime parameter value inline\"\n";
        o << indent(1) << "},\n";

        // Main pipeline template
        o << indent(1) << "\"Main Pipeline Template\": {\n";
        o << indent(2) << "\"prefix\": \"mainpipe\",\n";
        o << indent(2) << "\"body\": [\n";
        o << indent(3) << "\"# ${1:Description}\",\n";
        o << indent(3) << "\"\",\n";
        o << indent(3) << "\"pipeline main\",\n";
        o << indent(3) << "\"    video_cap(${2:0})\",\n";
        o << indent(3) << "\"    $0\",\n";
        o << indent(3) << "\"    display(\\\"${3:Output}\\\")\",\n";
        o << indent(3) << "\"    wait_key(1)\",\n";
        o << indent(3) << "\"end\",\n";
        o << indent(3) << "\"\",\n";
        o << indent(3) << "\"exec_loop main\"\n";
        o << indent(2) << "],\n";
        o << indent(2) << "\"description\": \"Create a main pipeline with video capture and display loop\"\n";
        o << indent(1) << "},\n";

        // Interval loop template
        o << indent(1) << "\"Interval Pipeline Template\": {\n";
        o << indent(2) << "\"prefix\": \"intervalpipe\",\n";
        o << indent(2) << "\"body\": [\n";
        o << indent(3) << "\"# Throttled pipeline running at ~${2:30} fps\",\n";
        o << indent(3) << "\"\",\n";
        o << indent(3) << "\"pipeline ${1:main}\",\n";
        o << indent(3) << "\"    video_cap(${3:0})\",\n";
        o << indent(3) << "\"    $0\",\n";
        o << indent(3) << "\"    display(\\\"${4:Output}\\\")\",\n";
        o << indent(3) << "\"    wait_key(1)\",\n";
        o << indent(3) << "\"end\",\n";
        o << indent(3) << "\"\",\n";
        o << indent(3) << "\"exec_interval ${1:main} ${2:33}\"\n";
        o << indent(2) << "],\n";
        o << indent(2) << "\"description\": \"Pipeline executed at a fixed interval (ms) for frame-rate control\"\n";
        o << indent(1) << "},\n";

        // RT pipeline template
        o << indent(1) << "\"Real-Time Pipeline Template\": {\n";
        o << indent(2) << "\"prefix\": \"rtpipe\",\n";
        o << indent(2) << "\"body\": [\n";
        o << indent(3) << "\"# Real-time low-latency pipeline\",\n";
        o << indent(3) << "\"\",\n";
        o << indent(3) << "\"pipeline ${1:main}\",\n";
        o << indent(3) << "\"    video_cap(${2:0})\",\n";
        o << indent(3) << "\"    $0\",\n";
        o << indent(3) << "\"    display(\\\"${3:Output}\\\")\",\n";
        o << indent(3) << "\"    wait_key(1)\",\n";
        o << indent(3) << "\"end\",\n";
        o << indent(3) << "\"\",\n";
        o << indent(3) << "\"exec_rt_seq ${1:main}\"\n";
        o << indent(2) << "],\n";
        o << indent(2) << "\"description\": \"Pipeline executed in real-time sequential mode (dedicated thread, low latency)\"\n";
        o << indent(1) << "},\n";

        // Stereo pipeline template
        o << indent(1) << "\"Stereo Pipeline Template\": {\n";
        o << indent(2) << "\"prefix\": \"stereopipe\",\n";
        o << indent(2) << "\"body\": [\n";
        o << indent(3) << "\"# Stereo vision pipeline\",\n";
        o << indent(3) << "\"\",\n";
        o << indent(3) << "\"pipeline capture_left\",\n";
        o << indent(3) << "\"    video_cap(${1:0})\",\n";
        o << indent(3) << "\"    undistort(\\\"camera_left\\\") -> \\\"rect_left\\\"\",\n";
        o << indent(3) << "\"end\",\n";
        o << indent(3) << "\"\",\n";
        o << indent(3) << "\"pipeline capture_right\",\n";
        o << indent(3) << "\"    video_cap(${2:2})\",\n";
        o << indent(3) << "\"    undistort(\\\"camera_right\\\") -> \\\"rect_right\\\"\",\n";
        o << indent(3) << "\"end\",\n";
        o << indent(3) << "\"\",\n";
        o << indent(3) << "\"pipeline stereo_process\",\n";
        o << indent(3) << "\"    exec_multi [\",\n";
        o << indent(3) << "\"        capture_left,\",\n";
        o << indent(3) << "\"        capture_right\",\n";
        o << indent(3) << "\"    ]\",\n";
        o << indent(3) << "\"    stereo_sgbm(\\\"rect_left\\\", \\\"rect_right\\\") -> \\\"disparity\\\"\",\n";
        o << indent(3) << "\"    use(\\\"disparity\\\")\",\n";
        o << indent(3) << "\"    apply_colormap(\\\"jet\\\")\",\n";
        o << indent(3) << "\"    display(\\\"Disparity\\\")\",\n";
        o << indent(3) << "\"    wait_key(1)\",\n";
        o << indent(3) << "\"end\",\n";
        o << indent(3) << "\"\",\n";
        o << indent(3) << "\"exec_loop stereo_process\"\n";
        o << indent(2) << "],\n";
        o << indent(2) << "\"description\": \"Create a stereo vision pipeline template\"\n";
        o << indent(1) << "},\n";

        // Chessboard calibration template
        o << indent(1) << "\"Chessboard Calibration Template\": {\n";
        o << indent(2) << "\"prefix\": \"chessboardcalib\",\n";
        o << indent(2) << "\"body\": [\n";
        o << indent(3) << "\"# Chessboard calibration data collection\",\n";
        o << indent(3) << "\"pipeline collect_frames\",\n";
        o << indent(3) << "\"    video_cap(${1:0})\",\n";
        o << indent(3) << "\"    find_chessboard_corners(${2:9}, ${3:6})\",\n";
        o << indent(3) << "\"    draw_chessboard_corners(${2:9}, ${3:6})\",\n";
        o << indent(3) << "\"    chessboard_detected_save(\\\"${4:/tmp/calib}\\\", \\\"corners\\\", ${2:9}, ${3:6})\",\n";
        o << indent(3) << "\"    display(\\\"Chessboard Calibration\\\")\",\n";
        o << indent(3) << "\"    wait_key(${5:30})\",\n";
        o << indent(3) << "\"end\",\n";
        o << indent(3) << "\"\",\n";
        o << indent(3) << "\"exec_loop collect_frames\"\n";
        o << indent(2) << "],\n";
        o << indent(2) << "\"description\": \"Collect chessboard calibration frames and save detected corners to JSON\"\n";
        o << indent(1) << "},\n";

        // Params pipe template
        o << indent(1) << "\"Pipeline with Params Template\": {\n";
        o << indent(2) << "\"prefix\": \"paramspipe\",\n";
        o << indent(2) << "\"body\": [\n";
        o << indent(3) << "\"# Runtime parameter declarations\",\n";
        o << indent(3) << "\"params [\",\n";
        o << indent(3) << "\"    ${1:brightness}:int=${2:50},\",\n";
        o << indent(3) << "\"    ${3:gain}:float=${4:1.0}\",\n";
        o << indent(3) << "\"]\",\n";
        o << indent(3) << "\"\",\n";
        o << indent(3) << "\"pipeline ${5:main}\",\n";
        o << indent(3) << "\"    video_cap(${6:0})\",\n";
        o << indent(3) << "\"    $0\",\n";
        o << indent(3) << "\"    display(\\\"${7:Output}\\\")\",\n";
        o << indent(3) << "\"    wait_key(1)\",\n";
        o << indent(3) << "\"end\",\n";
        o << indent(3) << "\"\",\n";
        o << indent(3) << "\"on_params @${1:brightness}\",\n";
        o << indent(3) << "\"    log \\\"${1:brightness} changed\\\"\",\n";
        o << indent(3) << "\"end\",\n";
        o << indent(3) << "\"\",\n";
        o << indent(3) << "\"exec_loop ${5:main}\"\n";
        o << indent(2) << "],\n";
        o << indent(2) << "\"description\": \"Pipeline template with runtime parameters and change handler\"\n";
        o << indent(1) << "}";  // No trailing comma — items from registry follow separated by commas.
    };

    staticSnippets();

    // ----------------------------------------------------------------
    // Per-item snippets generated from the registry
    // ----------------------------------------------------------------
    // Deduplicate display names (in case two items share the same title after
    // title-casing; append the function name as suffix if needed).
    std::set<std::string> usedKeys;

    for (const auto& name : names) {
        auto item = registry.getItem(name);
        if (!item) continue;

        std::string key = toTitleCase(name);
        if (usedKeys.count(key)) key += " (" + name + ")";
        usedKeys.insert(key);

        std::string body  = buildSnippetBody(*item);
        std::string desc  = buildParamDoc(*item);
        std::string prefix = name;  // exact function name is the prefix

        o << ",\n";
        o << indent(1) << "\"" << jsonEscape(key) << "\": {\n";
        o << indent(2) << "\"prefix\": \"" << jsonEscape(prefix) << "\",\n";
        o << indent(2) << "\"body\": [\"" << jsonEscape(body) << "\"],\n";
        o << indent(2) << "\"description\": \"" << jsonEscape(desc) << "\"\n";
        o << indent(1) << "}";
    }

    o << "\n}\n";
    return o.str();
}

// ---------------------------------------------------------------------------
// Write helper
// ---------------------------------------------------------------------------
bool writeFile(const std::string& path, const std::string& content, bool dryRun) {
    if (dryRun) {
        std::cout << "[dry-run] would write " << path
                  << " (" << content.size() << " bytes)\n";
        return true;
    }
    std::ofstream f(path);
    if (!f) {
        std::cerr << "ERROR: cannot open for writing: " << path << "\n";
        return false;
    }
    f << content;
    if (!f) {
        std::cerr << "ERROR: write failed: " << path << "\n";
        return false;
    }
    std::cout << "Wrote " << path << " (" << content.size() << " bytes)\n";
    return true;
}

void printHelp(const char* argv0) {
    std::cout <<
        "Usage:\n"
        "  " << argv0 << " [--out-dir <dir>] [--grammar-only] [--snippets-only] [--dry-run]\n"
        "  " << argv0 << " --help\n"
        "\n"
        "Options:\n"
        "  --out-dir <dir>   Path to the vscode-vsp extension directory\n"
        "                    (default: ./vscode-vsp relative to the binary)\n"
        "  --grammar-only    Only regenerate syntaxes/vsp.tmLanguage.json\n"
        "  --snippets-only   Only regenerate snippets/vsp.json\n"
        "  --dry-run         Print what would be written without touching the disk\n"
        "  --list            Print all registered function names, one per line, then exit\n"
        "  --help            Show this message\n"
        "\n"
        "The tool links against the live visionpipe interpreter and calls\n"
        "registerAllItems() so the output always matches the compiled binary.\n";
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char* argv[]) {
    std::string outDir = "./vscode-vsp";
    bool dryRun        = false;
    bool grammarOnly   = false;
    bool snippetsOnly  = false;
    bool listOnly      = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printHelp(argv[0]);
            return 0;
        } else if (arg == "--out-dir" && i + 1 < argc) {
            outDir = argv[++i];
        } else if (arg == "--dry-run") {
            dryRun = true;
        } else if (arg == "--grammar-only") {
            grammarOnly = true;
        } else if (arg == "--snippets-only") {
            snippetsOnly = true;
        } else if (arg == "--list") {
            listOnly = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printHelp(argv[0]);
            return 1;
        }
    }

    // Populate registry.
    visionpipe::ItemRegistry registry;
    visionpipe::registerAllItems(registry);

    if (listOnly) {
        auto names = registry.getItemNames();
        std::sort(names.begin(), names.end());
        for (const auto& n : names) std::cout << n << "\n";
        std::cout << "\nTotal: " << names.size() << " items\n";
        return 0;
    }

    // Trim trailing slash from outDir.
    while (outDir.size() > 1 && outDir.back() == '/') outDir.pop_back();

    bool ok = true;

    if (!snippetsOnly) {
        std::string grammar = generateGrammar(registry);
        ok &= writeFile(outDir + "/syntaxes/vsp.tmLanguage.json", grammar, dryRun);
    }

    if (!grammarOnly) {
        std::string snippets = generateSnippets(registry);
        ok &= writeFile(outDir + "/snippets/vsp.json", snippets, dryRun);
    }

    if (!ok) {
        std::cerr << "One or more files could not be written.\n";
        return 1;
    }

    if (!dryRun) {
        auto names = registry.getItemNames();
        std::cout << "\nDone. " << names.size()
                  << " items registered, extension files updated in: " << outDir << "\n";
    }
    return 0;
}
