"use strict";
const vscode = require("vscode");
const cp     = require("child_process");
const path   = require("path");
const fs     = require("fs");
const os     = require("os");

/** @type {vscode.DiagnosticCollection} */
let diagnosticCollection;
/** @type {vscode.OutputChannel} */
let outputChannel;

/** @type {Map<string, NodeJS.Timeout>} */
const debounceTimers = new Map();

const DEBOUNCE_MS = 500;

function log(msg) {
    const line = `[${new Date().toISOString()}] ${msg}`;
    outputChannel.appendLine(line);
}

function activate(context) {
    outputChannel = vscode.window.createOutputChannel("CUI");
    context.subscriptions.push(outputChannel);
    outputChannel.show(true);  // show but don't steal focus

    log("Extension activating");
    log(`  __dirname : ${__dirname}`);
    log(`  workspaceFolders: ${JSON.stringify((vscode.workspace.workspaceFolders || []).map(f => f.uri.fsPath))}`);

    diagnosticCollection = vscode.languages.createDiagnosticCollection("cui");
    context.subscriptions.push(diagnosticCollection);

    vscode.workspace.textDocuments.forEach(doc => {
        if (doc.languageId === "cui") checkDocument(doc);
    });

    context.subscriptions.push(
        vscode.workspace.onDidOpenTextDocument(doc => {
            if (doc.languageId === "cui") checkDocument(doc);
        }),
        vscode.workspace.onDidChangeTextDocument(event => {
            if (event.document.languageId !== "cui") return;
            const key = event.document.uri.toString();
            clearTimeout(debounceTimers.get(key));
            debounceTimers.set(key, setTimeout(() => {
                debounceTimers.delete(key);
                checkDocument(event.document);
            }, DEBOUNCE_MS));
        }),
        vscode.workspace.onDidSaveTextDocument(doc => {
            if (doc.languageId === "cui") checkDocument(doc);
        }),
        vscode.workspace.onDidCloseTextDocument(doc => {
            diagnosticCollection.delete(doc.uri);
        }),
    );

    log("Extension activated");
}

function deactivate() {
    if (diagnosticCollection) diagnosticCollection.dispose();
}

function resolveToolPath() {
    const cfg      = vscode.workspace.getConfiguration("cui");
    const explicit = cfg.get("toolPath");
    if (explicit) {
        log(`  toolPath (explicit): ${explicit}`);
        return explicit;
    }
    for (const folder of (vscode.workspace.workspaceFolders || [])) {
        const candidate = path.join(folder.uri.fsPath, "tools", "cui", "main.py");
        log(`  toolPath candidate: ${candidate} → exists=${fs.existsSync(candidate)}`);
        if (fs.existsSync(candidate)) return candidate;
    }
    const fallback = path.join(__dirname, "..", "cui", "main.py");
    log(`  toolPath fallback: ${fallback} → exists=${fs.existsSync(fallback)}`);
    return fallback;
}

function resolveRegistryPath(raw) {
    if (!raw) return "";
    if (path.isAbsolute(raw)) return raw;
    const folders = vscode.workspace.workspaceFolders;
    if (folders && folders.length > 0) return path.join(folders[0].uri.fsPath, raw);
    return raw;
}

function checkDocument(doc) {
    const cfg        = vscode.workspace.getConfiguration("cui");
    const pythonPath = cfg.get("pythonPath") || "python";
    const toolPath   = resolveToolPath();
    const registry   = resolveRegistryPath(cfg.get("registryPath") || "");

    log(`checkDocument: ${doc.fileName}`);
    log(`  python   : ${pythonPath}`);
    log(`  toolPath : ${toolPath}`);
    log(`  registry : ${registry || "(none)"}`);

    if (!fs.existsSync(toolPath)) {
        log(`  ERROR: toolPath not found`);
        vscode.window.showErrorMessage(`CUI: checker not found at "${toolPath}". Set cui.toolPath in settings.`);
        return;
    }

    // Write source to a temp file to avoid stdin/encoding issues on Windows
    const tmpFile = path.join(os.tmpdir(), `cui_check_${Date.now()}.cui`);
    try {
        fs.writeFileSync(tmpFile, doc.getText(), "utf8");
    } catch (err) {
        log(`  ERROR writing temp file: ${err}`);
        return;
    }

    const args = [toolPath, "check"];
    if (registry && fs.existsSync(registry)) args.push("--registry", registry);
    args.push(tmpFile);

    log(`  spawn: ${pythonPath} ${args.join(" ")}`);

    let proc;
    try {
        proc = cp.spawn(pythonPath, args, { stdio: ["ignore", "pipe", "pipe"] });
    } catch (err) {
        log(`  ERROR spawn failed: ${err}`);
        fs.unlink(tmpFile, () => {});
        return;
    }

    let stdout = "";
    let stderr = "";
    proc.stdout.on("data", chunk => { stdout += chunk; });
    proc.stderr.on("data", chunk => { stderr += chunk; });

    proc.on("close", code => {
        fs.unlink(tmpFile, () => {});
        if (stderr) log(`  stderr: ${stderr.trim()}`);
        log(`  stdout (exit ${code}): ${stdout.trim()}`);

        let items;
        try {
            items = JSON.parse(stdout.trim() || "[]");
        } catch {
            log(`  ERROR: failed to parse JSON output`);
            return;
        }

        const vsDiags = items.map(d => {
            const startLine = Math.max(0, (d.line    || 1) - 1);
            const startCol  = Math.max(0, (d.col     || 1) - 1);
            const endLine   = Math.max(0, (d.endLine || d.line || 1) - 1);
            const endCol    = Math.max(0, (d.endCol  || (d.col || 1) + 1) - 1);
            const range = new vscode.Range(startLine, startCol, endLine, endCol);
            const sev   = d.severity === "warning"
                ? vscode.DiagnosticSeverity.Warning
                : vscode.DiagnosticSeverity.Error;
            return new vscode.Diagnostic(range, d.message, sev);
        });

        log(`  → ${vsDiags.length} diagnostic(s) pushed`);
        diagnosticCollection.set(doc.uri, vsDiags);
    });
}

module.exports = { activate, deactivate };
