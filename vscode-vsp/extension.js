const vscode = require('vscode');
const path = require('path');
const fs = require('fs');

function loadSnippetData(context) {
    const snippetsPath = path.join(context.extensionPath, 'snippets', 'vsp.json');
    try {
        const raw = fs.readFileSync(snippetsPath, 'utf8');
        return JSON.parse(raw);
    } catch {
        return {};
    }
}

function normalizeBody(body) {
    if (Array.isArray(body)) {
        return body.join('\n');
    }
    return typeof body === 'string' ? body : '';
}

function cleanSnippetText(text) {
    return text
        .replace(/\$\{\d+:([^}]+)\}/g, '$1')
        .replace(/\$\{\d+\|([^}]+)\|\}/g, (_, choices) => choices.split(',')[0])
        .replace(/\$\d+/g, '');
}

function parseParamsFromDescription(description) {
    if (!description || typeof description !== 'string') {
        return [];
    }
    const lines = description.split(/\r?\n/);
    const start = lines.findIndex((line) => line.trim() === 'Params:');
    if (start < 0) {
        return [];
    }
    const params = [];
    for (let i = start + 1; i < lines.length; i += 1) {
        const line = lines[i];
        if (!line.trim()) {
            continue;
        }
        if (!/^\s{2,}/.test(line)) {
            break;
        }
        const trimmed = line.trim();
        const match = trimmed.match(/^([A-Za-z_][\w]*)\s*\(([^)]+)\):\s*(.+)$/);
        if (match) {
            params.push({
                name: match[1],
                type: match[2],
                description: match[3]
            });
        }
    }
    return params;
}

function buildKnowledgeBase(snippets) {
    const keywords = [
        'pipeline', 'end', 'exec_seq', 'exec_multi', 'exec_loop', 'exec_interval',
        'exec_interval_multi', 'exec_rt_seq', 'exec_rt_multi', 'exec_nasync', 'exec_fork',
        'no_interval', 'use', 'cache', 'global', 'let', 'params', 'on_params',
        'debug_start', 'debug_end'
    ];

    const functionMap = new Map();
    const prefixMap = new Map();

    for (const [name, spec] of Object.entries(snippets)) {
        const prefix = spec.prefix;
        const bodyRaw = normalizeBody(spec.body);
        const body = cleanSnippetText(bodyRaw);
        const description = typeof spec.description === 'string' ? spec.description : '';

        if (typeof prefix === 'string' && prefix) {
            prefixMap.set(prefix, { name, prefix, body, description });
        }

        const fnMatch = body.match(/\b([A-Za-z_][\w]*)\s*\(/);
        if (!fnMatch) {
            continue;
        }

        const fnName = fnMatch[1];
        const paramListText = body.match(new RegExp(`${fnName}\\(([^)]*)\\)`));
        const inferredParams = paramListText && paramListText[1].trim()
            ? paramListText[1]
                .split(',')
                .map((part) => part.trim())
                .filter(Boolean)
                .map((part, idx) => ({
                    name: part.replace(/^"|"$/g, ''),
                    type: 'any',
                    description: `Argument ${idx + 1}`
                }))
            : [];

        const parsedParams = parseParamsFromDescription(description);
        const params = parsedParams.length > 0 ? parsedParams : inferredParams;

        functionMap.set(fnName, {
            name: fnName,
            detail: name,
            description,
            body,
            params
        });
    }

    return { keywords, functionMap, prefixMap };
}

function extractRuntimeParams(documentText) {
    const names = new Set();
    const blocks = [...documentText.matchAll(/\bparams\s*\[([\s\S]*?)\]/g)];
    for (const block of blocks) {
        const content = block[1] || '';
        for (const entry of content.split(',')) {
            const m = entry.match(/\b([A-Za-z_][\w]*)\s*:/);
            if (m) {
                names.add(m[1]);
            }
        }
    }
    return [...names];
}

function extractPipelines(documentText) {
    const names = new Set();
    const matches = documentText.matchAll(/^\s*pipeline\s+([A-Za-z_][\w]*)/gm);
    for (const m of matches) {
        names.add(m[1]);
    }
    return [...names];
}

function inFunctionCallContext(linePrefix) {
    return /\b([A-Za-z_][\w]*)\([^()]*$/.test(linePrefix);
}

function currentFunctionCall(linePrefix) {
    const match = linePrefix.match(/\b([A-Za-z_][\w]*)\s*\(([^()]*)$/);
    if (!match) {
        return null;
    }
    const argsText = match[2];
    const activeParameter = argsText.trim().length === 0 ? 0 : argsText.split(',').length - 1;
    return { functionName: match[1], activeParameter };
}

function createCompletionProvider(kb) {
    return {
        provideCompletionItems(document, position) {
            const linePrefix = document.lineAt(position).text.slice(0, position.character);
            const fullText = document.getText();
            const runtimeParams = extractRuntimeParams(fullText);
            const pipelines = extractPipelines(fullText);
            const items = [];

            if (/@[A-Za-z_\d]*$/.test(linePrefix)) {
                for (const param of runtimeParams) {
                    const item = new vscode.CompletionItem(param, vscode.CompletionItemKind.Variable);
                    item.insertText = param;
                    item.detail = 'runtime param';
                    items.push(item);
                }
                return items;
            }

            for (const kw of kb.keywords) {
                const item = new vscode.CompletionItem(kw, vscode.CompletionItemKind.Keyword);
                item.detail = 'VSP keyword';
                items.push(item);
            }

            for (const fn of kb.functionMap.values()) {
                const item = new vscode.CompletionItem(fn.name, vscode.CompletionItemKind.Function);
                item.detail = fn.detail;
                item.documentation = fn.description;
                item.insertText = new vscode.SnippetString(`${fn.name}($0)`);
                items.push(item);
            }

            for (const prefixInfo of kb.prefixMap.values()) {
                const item = new vscode.CompletionItem(prefixInfo.prefix, vscode.CompletionItemKind.Snippet);
                item.detail = prefixInfo.name;
                item.documentation = prefixInfo.description;
                items.push(item);
            }

            if (/\bexec_(seq|loop|multi|interval|interval_multi|rt_seq|rt_multi|nasync|fork)\s+[A-Za-z_\d]*$/.test(linePrefix)) {
                for (const pipeline of pipelines) {
                    const item = new vscode.CompletionItem(pipeline, vscode.CompletionItemKind.Reference);
                    item.detail = 'pipeline';
                    items.push(item);
                }
            }

            if (inFunctionCallContext(linePrefix)) {
                // suggest parameters of the function being called, if known
                const callInfo = currentFunctionCall(linePrefix);
                if (callInfo && kb.functionMap.has(callInfo.functionName)) {
                    const fn = kb.functionMap.get(callInfo.functionName);
                    for (const p of fn.params) {
                        const item = new vscode.CompletionItem(p.name, vscode.CompletionItemKind.Variable);
                        item.detail = `param of ${fn.name}`;
                        if (p.type || p.description) {
                            item.documentation = p.type
                                ? `**${p.type}**${p.description ? ` – ${p.description}` : ''}`
                                : p.description;
                        }
                        item.insertText = new vscode.SnippetString(p.name);
                        items.push(item);
                    }
                }

                // keep existing runtime param suggestions as well
                for (const param of runtimeParams) {
                    const item = new vscode.CompletionItem(`@${param}`, vscode.CompletionItemKind.Variable);
                    item.detail = 'runtime param reference';
                    item.insertText = `@${param}`;
                    items.push(item);
                }
            }

            return items;
        }
    };
}

function createHoverProvider(kb) {
    return {
        provideHover(document, position) {
            const range = document.getWordRangeAtPosition(position, /@?[A-Za-z_][\w]*/);
            if (!range) {
                return null;
            }
            const word = document.getText(range).replace(/^@/, '');

            if (kb.functionMap.has(word)) {
                const fn = kb.functionMap.get(word);
                const signature = `${fn.name}(${fn.params.map((p) => p.name).join(', ')})`;
                const md = new vscode.MarkdownString();
                md.appendCodeblock(signature, 'vsp');
                if (fn.description) {
                    md.appendMarkdown(`\n${fn.description}`);
                }
                if (fn.params && fn.params.length) {
                    md.appendMarkdown('\n\n**Parameters:**\n');
                    for (const p of fn.params) {
                        md.appendMarkdown(`- \`${p.name}\` (${p.type || 'any'})${p.description ? ` – ${p.description}` : ''}\n`);
                    }
                }
                return new vscode.Hover(md, range);
            }

            if (kb.prefixMap.has(word)) {
                const snippet = kb.prefixMap.get(word);
                const md = new vscode.MarkdownString();
                md.appendMarkdown(`**${snippet.name}**`);
                if (snippet.description) {
                    md.appendMarkdown(`\n\n${snippet.description}`);
                }
                if (snippet.body) {
                    md.appendCodeblock(snippet.body, 'vsp');
                }
                return new vscode.Hover(md, range);
            }

            return null;
        }
    };
}

function createSignatureHelpProvider(kb) {
    return {
        provideSignatureHelp(document, position) {
            const linePrefix = document.lineAt(position).text.slice(0, position.character);
            const call = currentFunctionCall(linePrefix);
            if (!call) {
                return null;
            }

            const fn = kb.functionMap.get(call.functionName);
            if (!fn) {
                return null;
            }

            const signatureLabel = `${fn.name}(${fn.params.map((p) => p.name).join(', ')})`;
            const sig = new vscode.SignatureInformation(signatureLabel, fn.description || undefined);
            sig.parameters = fn.params.map((p) => new vscode.ParameterInformation(
                `${p.name}: ${p.type}`,
                p.description || undefined
            ));

            const help = new vscode.SignatureHelp();
            help.signatures = [sig];
            help.activeSignature = 0;
            help.activeParameter = Math.min(call.activeParameter, Math.max(fn.params.length - 1, 0));
            return help;
        }
    };
}

function activate(context) {
    const snippets = loadSnippetData(context);
    const kb = buildKnowledgeBase(snippets);

    const selector = { language: 'vsp', scheme: 'file' };

    context.subscriptions.push(
        vscode.languages.registerCompletionItemProvider(
            selector,
            createCompletionProvider(kb),
            '.', '_', '@', '(', ',', ' '
        )
    );

    context.subscriptions.push(
        vscode.languages.registerHoverProvider(selector, createHoverProvider(kb))
    );

    context.subscriptions.push(
        vscode.languages.registerSignatureHelpProvider(
            selector,
            createSignatureHelpProvider(kb),
            '(', ','
        )
    );
}

function deactivate() {}

module.exports = {
    activate,
    deactivate
};
