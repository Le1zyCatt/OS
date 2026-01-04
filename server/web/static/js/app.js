/**
 * 论文审稿系统 Web 控制台
 * Academic Paper Review System - Web Console
 */

class APSConsole {
    constructor() {
        this.serverHost = 'localhost';
        this.serverPort = 8080;
        this.sessions = {}; // alias -> token
        this.activeUser = null;
        this.activeToken = null;
        this.currentPath = '/';
        this.commandHistory = [];
        this.historyIndex = -1;
        
        this.init();
    }
    
    init() {
        // DOM 元素
        this.terminalOutput = document.getElementById('terminalOutput');
        this.terminalInput = document.getElementById('terminalInput');
        this.inputPrompt = document.getElementById('inputPrompt');
        this.connectionDot = document.getElementById('connectionDot');
        this.serverInfo = document.getElementById('serverInfo');
        this.currentPathEl = document.getElementById('currentPath');
        
        // 绑定事件
        this.bindEvents();
        
        // 检查连接
        this.checkConnection();
        
        // 定时刷新
        setInterval(() => this.refreshIfNeeded(), 30000);
        
        // 待登录用户信息
        this.pendingLogin = null;
        // UI 立即反馈：登录中状态（不等网络返回）
        this.loginInProgressUser = null;
    }
    
    bindEvents() {
        // 终端输入
        this.terminalInput.addEventListener('keydown', (e) => this.handleInputKeydown(e));
        
        // 用户卡片点击 - 弹出密码输入模态框
        document.querySelectorAll('.user-card').forEach(card => {
            card.addEventListener('click', () => {
                const user = card.dataset.user;
                const pass = card.dataset.pass;
                const role = card.dataset.role;
                
                // 显示登录模态框
                this.showLoginModal(user, pass, role);
            });
        });
        
        // 登录模态框事件
        document.getElementById('confirmLoginBtn')?.addEventListener('click', () => this.confirmLogin());
        document.getElementById('cancelLoginBtn')?.addEventListener('click', () => this.hideLoginModal());
        document.getElementById('closeLoginModal')?.addEventListener('click', () => this.hideLoginModal());
        document.getElementById('loginPassword')?.addEventListener('keydown', (e) => {
            if (e.key === 'Enter') this.confirmLogin();
            if (e.key === 'Escape') this.hideLoginModal();
        });
        
        // 登出按钮
        document.getElementById('logoutBtn')?.addEventListener('click', () => this.logout());
        
        // 视图切换
        document.querySelectorAll('.view-tab').forEach(tab => {
            tab.addEventListener('click', () => {
                const view = tab.dataset.view;
                this.switchView(view);
            });
        });
        
        // 创建快照按钮
        document.getElementById('createSnapshotBtn')?.addEventListener('click', () => {
            this.createSnapshot();
        });
        
        // 一键测试按钮
        document.getElementById('runAllTestsBtn')?.addEventListener('click', () => {
            this.runAllTests();
        });
        
        // 命令面板切换
        document.getElementById('toggleCommandsBtn')?.addEventListener('click', () => {
            this.toggleCommandsPanel();
        });
        
        document.getElementById('closePanelBtn')?.addEventListener('click', () => {
            this.closeCommandsPanel();
        });
        
        // 命令项点击 - 复制到终端
        document.querySelectorAll('.command-item').forEach(item => {
            item.addEventListener('click', () => {
                const cmd = item.dataset.cmd;
                if (cmd) {
                    this.terminalInput.value = cmd;
                    this.terminalInput.focus();
                    
                    // 显示复制成功反馈
                    item.classList.add('copied');
                    setTimeout(() => item.classList.remove('copied'), 500);
                    
                    // 关闭面板
                    this.closeCommandsPanel();
                }
            });
        });
        
        // 聚焦输入框
        document.addEventListener('click', (e) => {
            if (e.target.closest('.terminal-view') && !e.target.closest('.commands-panel')) {
                this.terminalInput.focus();
            }
        });
    }
    
    toggleCommandsPanel() {
        const panel = document.getElementById('commandsPanel');
        if (panel) {
            panel.classList.toggle('open');
        }
    }
    
    closeCommandsPanel() {
        const panel = document.getElementById('commandsPanel');
        if (panel) {
            panel.classList.remove('open');
        }
    }

    // 复制终端内容到剪贴板
    copyTerminalContent() {
        const output = this.terminalOutput;
        if (!output) return;

        // 获取所有行的纯文本内容
        const lines = [];
        output.querySelectorAll('.terminal-line').forEach(line => {
            lines.push(line.textContent);
        });
        
        const text = lines.join('\n');
        
        navigator.clipboard.writeText(text).then(() => {
            // 显示复制成功提示
            this.showCopyToast('✓ 已复制终端内容');
        }).catch(err => {
            console.error('复制失败:', err);
            // 回退方案：选中内容
            this.selectTerminalContent();
        });
    }

    // 选中终端内容（用于手动复制）
    selectTerminalContent() {
        const output = this.terminalOutput;
        if (!output) return;

        const range = document.createRange();
        range.selectNodeContents(output);
        const selection = window.getSelection();
        selection.removeAllRanges();
        selection.addRange(range);
        
        this.showCopyToast('已选中内容，按 Ctrl+C 复制');
    }

    // 显示复制成功提示
    showCopyToast(message) {
        // 移除已有的 toast
        document.querySelectorAll('.copy-toast').forEach(t => t.remove());
        
        const toast = document.createElement('div');
        toast.className = 'copy-toast';
        toast.textContent = message;
        document.body.appendChild(toast);
        
        // 触发动画
        requestAnimationFrame(() => {
            toast.classList.add('show');
        });
        
        // 自动消失
        setTimeout(() => {
            toast.classList.remove('show');
            setTimeout(() => toast.remove(), 300);
        }, 2000);
    }

    // 清空终端
    clearTerminal() {
        this.terminalOutput.innerHTML = '';
        this.appendLine('终端已清空', 'info');
    }
    
    // 登录模态框相关方法
    showLoginModal(user, correctPass, role) {
        this.pendingLogin = { user, correctPass, role };
        
        const modal = document.getElementById('loginModal');
        const avatar = document.getElementById('loginAvatar');
        const username = document.getElementById('loginUsername');
        const roleEl = document.getElementById('loginRole');
        const hint = document.getElementById('passwordHint');
        const passInput = document.getElementById('loginPassword');
        
        // 如果模态框 DOM 不存在（例如浏览器缓存了旧模板），退化为 prompt 以保证“必须输入密码”
        if (!(modal && avatar && username && roleEl && hint && passInput)) {
            const entered = window.prompt(`请输入 ${user} 的密码：`, '');
            if (entered && entered.trim()) {
                // UI 先响应
                this.loginInProgressUser = user;
                this.updateUserCards();
                this.updateCurrentUserDisplay();
                this.login(user, entered.trim()).finally(() => {
                    this.loginInProgressUser = null;
                    this.updateUserCards();
                    this.updateCurrentUserDisplay();
                });
            }
            return;
        }

        if (modal && avatar && username && roleEl && hint && passInput) {
            // 更新模态框内容
            avatar.textContent = user.charAt(0).toUpperCase();
            avatar.className = `login-avatar ${user}`;
            username.textContent = user;
            roleEl.textContent = this.translateRole(role);
            hint.textContent = `提示: ${correctPass}`;
            passInput.value = '';
            
            // 显示模态框
            modal.style.display = 'flex';
            passInput.focus();
        }
    }
    
    hideLoginModal() {
        this.hideLoginModalInternal(true);
    }

    hideLoginModalInternal(clearPending) {
        const modal = document.getElementById('loginModal');
        if (modal) {
            modal.style.display = 'none';
        }
        if (clearPending) {
            this.pendingLogin = null;
        }
    }
    
    async confirmLogin() {
        if (!this.pendingLogin) return;

        const pending = this.pendingLogin;
        
        const passInput = document.getElementById('loginPassword');
        const enteredPass = passInput?.value || '';
        
        if (!enteredPass) {
            this.appendLine('请输入密码', 'error');
            return;
        }
        
        // UI 先响应：展示“登录中”状态
        this.loginInProgressUser = pending.user;
        this.updateUserCards();
        this.updateCurrentUserDisplay();

        // 隐藏模态框（不要清 pending，后面还要用）
        this.hideLoginModalInternal(false);
        
        // 如果已登录其他用户，先登出
        if (this.activeUser && this.activeUser !== pending.user) {
            await this.logout(true);
        }
        
        // 使用输入的密码登录
        await this.login(pending.user, enteredPass);

        // 登录完成/失败后，结束“登录中”状态
        this.loginInProgressUser = null;
        this.pendingLogin = null;
        this.updateUserCards();
        this.updateCurrentUserDisplay();
    }
    
    // 类似 test_client.py 的 _print_exchange 方法，显示输入命令和输出
    async printExchange(cmd, displayCmd = null) {
        const showCmd = displayCmd || cmd;
        // 显示输入命令（绿色）
        this.appendLine(`>>> ${showCmd}`, 'success');
        
        // 发送命令并获取响应
        const upper = cmd.split(' ')[0].toUpperCase();
        
        let fullCmd = cmd;
        if (this.activeToken && upper !== 'LOGIN' && upper !== 'LOGOUT') {
            const parts = cmd.split(/\s+/);
            const rest = cmd.substring(parts[0].length).trim();
            fullCmd = rest ? `${upper} ${this.activeToken} ${rest}` : `${upper} ${this.activeToken}`;
        }
        
        try {
            const response = await fetch('/api/command', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ command: fullCmd })
            });
            
            const data = await response.json();
            const resp = data.response || '(无响应)';
            
            // 显示响应（TREE 命令使用分层颜色）
            const isTree = upper === 'TREE';
            const lines = resp.split('\n');
            lines.forEach(line => {
                if (isTree) {
                    this.appendTreeLine(line);
                } else {
                    this.appendLine(line, 'response');
                }
            });
            this.appendLine('', 'info');
            
            return resp;
        } catch (err) {
            this.appendLine(`连接错误: ${err.message}`, 'error');
            return null;
        }
    }
    
    handleInputKeydown(e) {
        if (e.key === 'Enter') {
            const cmd = this.terminalInput.value.trim();
            if (cmd) {
                this.commandHistory.push(cmd);
                this.historyIndex = this.commandHistory.length;
                this.processCommand(cmd);
            }
            this.terminalInput.value = '';
        } else if (e.key === 'ArrowUp') {
            e.preventDefault();
            if (this.historyIndex > 0) {
                this.historyIndex--;
                this.terminalInput.value = this.commandHistory[this.historyIndex];
            }
        } else if (e.key === 'ArrowDown') {
            e.preventDefault();
            if (this.historyIndex < this.commandHistory.length - 1) {
                this.historyIndex++;
                this.terminalInput.value = this.commandHistory[this.historyIndex];
            } else {
                this.historyIndex = this.commandHistory.length;
                this.terminalInput.value = '';
            }
        }
    }
    
    async processCommand(cmd) {
        this.appendLine(`${this.getPromptText()} ${cmd}`, 'command');
        
        const upper = cmd.split(' ')[0].toUpperCase();
        
        // 本地命令处理
        if (upper === 'HELP' || cmd === ':help') {
            this.showHelp();
            return;
        }
        
        if (upper === 'CLEAR' || cmd === ':clear') {
            this.terminalOutput.innerHTML = '';
            return;
        }
        
        if (upper === 'LOGIN') {
            const parts = cmd.split(/\s+/);
            if (parts.length >= 3) {
                await this.login(parts[1], parts[2]);
            } else {
                this.appendLine('用法: LOGIN <username> <password>', 'error');
            }
            return;
        }
        
        if (upper === 'LOGOUT') {
            await this.logout();
            return;
        }
        
        // 需要 token 的命令
        await this.sendCommand(cmd);
    }

    async sendCommandRaw(cmd) {
        const upper = cmd.split(' ')[0].toUpperCase();

        // 自动注入 token
        let fullCmd = cmd;
        if (this.activeToken && upper !== 'LOGIN' && upper !== 'LOGOUT') {
            const parts = cmd.split(/\s+/);
            const rest = cmd.substring(parts[0].length).trim();
            fullCmd = rest ? `${upper} ${this.activeToken} ${rest}` : `${upper} ${this.activeToken}`;
        }

        const response = await fetch('/api/command_raw', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ command: fullCmd })
        });

        const buf = await response.arrayBuffer();
        return new Uint8Array(buf);
    }

    bytesStartsWith(bytes, asciiPrefix) {
        if (!bytes || bytes.length < asciiPrefix.length) return false;
        for (let i = 0; i < asciiPrefix.length; i++) {
            if (bytes[i] !== asciiPrefix.charCodeAt(i)) return false;
        }
        return true;
    }

    bytesToAsciiPreview(bytes, maxLen = 200) {
        try {
            const td = new TextDecoder('utf-8');
            return td.decode(bytes.slice(0, Math.min(maxLen, bytes.length)));
        } catch {
            return '';
        }
    }

    async sha256Hex(bytes) {
        const digest = await crypto.subtle.digest('SHA-256', bytes);
        return Array.from(new Uint8Array(digest)).map(b => b.toString(16).padStart(2, '0')).join('');
    }

    async printBinaryRead(path, expectedMagicAscii = null) {
        const cmd = `READ ${path}`;
        this.appendLine(`>>> ${cmd}`, 'success');

        const raw = await this.sendCommandRaw(cmd);
        if (!raw || raw.length === 0) {
            this.appendLine('(无响应)', 'warning');
            this.appendLine('', 'info');
            return;
        }

        if (!this.bytesStartsWith(raw, 'OK: ')) {
            const preview = this.bytesToAsciiPreview(raw, 200);
            this.appendLine(preview || '(二进制错误响应)', 'error');
            this.appendLine('', 'info');
            return;
        }

        const payload = raw.slice(4);
        const header = payload.slice(0, 16);
        const headerAscii = Array.from(header).map(b => (b >= 32 && b <= 126) ? String.fromCharCode(b) : '.').join('');
        const sha = await this.sha256Hex(payload);

        this.appendLine(`OK: 下载成功`, 'success');
        this.appendLine(`  - 大小: ${payload.length} bytes`, 'info');
        this.appendLine(`  - 头部(ASCII): ${headerAscii}`, 'info');
        this.appendLine(`  - SHA-256: ${sha}`, 'info');

        if (expectedMagicAscii) {
            const okMagic = this.bytesStartsWith(payload, expectedMagicAscii);
            this.appendLine(`  - Magic: ${okMagic ? '匹配' : '不匹配'} (${expectedMagicAscii})`, okMagic ? 'success' : 'warning');
        }

        this.appendLine('', 'info');
    }
    
    async sendCommand(cmd) {
        const upper = cmd.split(' ')[0].toUpperCase();

        // READ 二进制文件：改用 raw bytes 通道，避免 UTF-8 解码导致 PDF/docx 乱码
        if (upper === 'READ') {
            const parts = cmd.split(/\s+/);
            const path = parts[1] || '';
            const lower = path.toLowerCase();
            if (lower.endsWith('.pdf') || lower.endsWith('.docx') || lower.endsWith('.rtf')) {
                let magic = null;
                if (lower.endsWith('.pdf')) magic = '%PDF-';
                else if (lower.endsWith('.docx')) magic = 'PK\x03\x04';
                else if (lower.endsWith('.rtf')) magic = '{\\rtf1';

                // docx 的 magic 用 bytesStartsWith(ASCII) 不适用（含 0x03/0x04），这里降级为仅展示摘要
                if (lower.endsWith('.docx')) magic = null;

                await this.printBinaryRead(path, magic);
                return;
            }
        }
        
        // 自动注入 token
        let fullCmd = cmd;
        if (this.activeToken && upper !== 'LOGIN' && upper !== 'LOGOUT') {
            const parts = cmd.split(/\s+/);
            const rest = cmd.substring(parts[0].length).trim();
            fullCmd = rest ? `${upper} ${this.activeToken} ${rest}` : `${upper} ${this.activeToken}`;
        }
        
        try {
            const response = await fetch('/api/command', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ command: fullCmd })
            });
            
            const data = await response.json();
            this.handleResponse(data.response, upper);
        } catch (err) {
            this.appendLine(`连接错误: ${err.message}`, 'error');
            this.setConnectionStatus(false);
        }
    }
    
    handleResponse(response, cmdType) {
        if (!response) {
            this.appendLine('(无响应)', 'warning');
            return;
        }
        
        // 解析响应类型
        const isOk = response.startsWith('OK:') || response.startsWith('OK ');
        const isError = response.startsWith('ERROR:') || response.startsWith('ERROR ');
        
        // TREE 命令使用分层颜色
        if (cmdType === 'TREE') {
            const lines = response.split('\n');
            lines.forEach(line => {
                this.appendTreeLine(line);
            });
        } else {
            // 格式化输出
            const lines = response.split('\n');
            lines.forEach(line => {
                let type = 'response';
                if (isOk && lines.indexOf(line) === 0) type = 'success';
                else if (isError) type = 'error';
                else if (line.includes('│') || line.includes('├') || line.includes('└')) type = 'info';
                
                this.appendLine(line, type);
            });
        }
        
        // 特殊命令后处理
        if (cmdType === 'BACKUP_LIST' || cmdType === 'SYSTEM_STATUS') {
            this.refreshSnapshots();
        }
        
        if (cmdType === 'CD' && isOk) {
            // 更新当前路径
            this.sendCommand('PWD');
        }
        
        if (cmdType === 'PWD' && isOk) {
            const match = response.match(/OK:\s*(.+)/);
            if (match) {
                this.currentPath = match[1].trim();
                this.currentPathEl.textContent = this.currentPath;
            }
        }
    }
    
    // TREE 命令分层颜色显示
    appendTreeLine(line) {
        // 计算缩进层级（通过前导空格和树形符号）
        const indentMatch = line.match(/^([\s│├└─]*)/);
        let level = 0;
        if (indentMatch) {
            const indent = indentMatch[1];
            // 每4个字符（含树形符号）算一层
            level = Math.floor(indent.replace(/[^\s│├└─]/g, '').length / 4);
        }
        
        // 限制层级范围
        level = Math.min(level, 7);
        
        const lineEl = document.createElement('div');
        // 复用终端行样式，确保等宽与缩进显示一致
        lineEl.className = `terminal-line info tree-level-${level}`;
        lineEl.textContent = line;
        this.terminalOutput.appendChild(lineEl);
        this.terminalOutput.scrollTop = this.terminalOutput.scrollHeight;
    }
    
    async login(username, password) {
        this.appendLine(`正在登录 ${username}...`, 'info');
        
        try {
            const response = await fetch('/api/command', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ command: `LOGIN ${username} ${password}` })
            });
            
            const data = await response.json();
            const resp = data.response;
            
            if (resp.startsWith('OK:')) {
                // 解析 token
                const tokenMatch = resp.match(/OK:\s*(\S+)/);
                if (tokenMatch) {
                    const token = tokenMatch[1];
                    // 单用户模式：只保留当前用户会话
                    this.sessions = { [username]: token };
                    this.activeUser = username;
                    this.activeToken = token;
                    
                    // 更新 UI
                    this.updateUserCards();
                    this.updatePrompt();
                    this.updateCurrentUserDisplay();
                    this.setConnectionStatus(true);
                    
                    this.appendLine(`✓ 登录成功！`, 'success');
                    this.appendLine(`  用户名: ${username}`, 'info');
                    this.appendLine(`  Token: ${token}`, 'info');
                    
                    // 解析角色
                    const roleMatch = resp.match(/ROLE=(\w+)/);
                    if (roleMatch) {
                        this.appendLine(`  角色: ${this.translateRole(roleMatch[1])}`, 'info');
                    }
                    
                    // 刷新数据
                    // 不阻塞 UI：后台刷新
                    setTimeout(() => this.refreshAll(), 0);
                }
            } else {
                this.appendLine(resp, 'error');
            }
        } catch (err) {
            this.appendLine(`登录失败: ${err.message}`, 'error');
        }
    }
    
    async logout(silent = false) {
        if (!this.activeToken) {
            if (!silent) this.appendLine('当前未登录', 'warning');
            return;
        }
        
        try {
            await fetch('/api/command', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ command: `LOGOUT ${this.activeToken}` })
            });
            
            // 单用户模式：清空所有会话
            this.sessions = {};
            
            // 更新状态
            const prevUser = this.activeUser;
            this.activeUser = null;
            this.activeToken = null;
            
            this.updateUserCards();
            this.updatePrompt();
            this.updateCurrentUserDisplay();
            
            if (!silent) {
                this.appendLine(`✓ ${prevUser} 已登出`, 'success');
            }
        } catch (err) {
            if (!silent) {
                this.appendLine(`登出失败: ${err.message}`, 'error');
            }
        }
    }
    
    // 一键测试所有功能（模拟 test_client.py --smoke）
    async runAllTests() {
        const btn = document.getElementById('runAllTestsBtn');
        if (btn) {
            btn.disabled = true;
            btn.textContent = '⏳ 测试中...';
        }
        
        this.appendLine('', 'info');
        this.appendLine('═══════════════════════════════════════════════════════════════', 'info');
        this.appendLine('         🚀 一键冒烟测试 (Smoke Test) - Web Console             ', 'info');
        this.appendLine('═══════════════════════════════════════════════════════════════', 'info');
        this.appendLine('模拟 test_client.py --smoke 的完整命令流程', 'info');
        this.appendLine('', 'info');
        
        const delay = (ms) => new Promise(r => setTimeout(r, ms));
        const timestamp = Date.now();
        const testDir = `/demo_test_web_${timestamp}`;
        const paperId = `demo_paper_${timestamp}`;
        const filePaperPdf = `demo_file_paper_pdf_${timestamp}`;
        const filePaperDocx = `demo_file_paper_docx_${timestamp}`;
        const filePaperRtf = `demo_file_paper_rtf_${timestamp}`;
        const snapshotName = `snap_web_${timestamp}`;
        
        try {
            // ==================== 1. ADMIN 基础测试 ====================
            this.appendLine('═══════════════════════════════════════════════════════════════', 'warning');
            this.appendLine('【1/6】ADMIN - 登录与基础文件系统操作', 'warning');
            this.appendLine('═══════════════════════════════════════════════════════════════', 'warning');
            
            await this.login('admin', 'admin123');
            await delay(200);
            
            await this.printExchange('PWD');
            await delay(200);
            
            await this.printExchange(`MKDIR ${testDir}`);
            await delay(200);
            
            await this.printExchange(`WRITE ${testDir}/hello.txt Hello_World_from_Web_Console`);
            await delay(200);
            
            await this.printExchange(`READ ${testDir}/hello.txt`);
            await delay(200);
            
            // 多级目录测试
            await this.printExchange(`MKDIR ${testDir}/lvl1`);
            await delay(200);
            
            await this.printExchange(`MKDIR ${testDir}/lvl1/lvl2`);
            await delay(200);
            
            await this.printExchange(`MKDIR ${testDir}/lvl1/lvl2/lvl3`);
            await delay(200);
            
            await this.printExchange(`WRITE ${testDir}/lvl1/lvl2/deep_file.txt Content_in_deep_directory`);
            await delay(200);
            
            await this.printExchange(`CD ${testDir}`);
            await delay(200);
            
            await this.printExchange('LS .');
            await delay(200);
            
            await this.printExchange('CD /');
            await delay(200);
            
            await this.printExchange('LS /');
            await delay(200);
            
            await this.printExchange(`TREE ${testDir}`);
            await delay(300);
            
            // ==================== 2. ADMIN 管理命令 ====================
            this.appendLine('═══════════════════════════════════════════════════════════════', 'warning');
            this.appendLine('【2/6】ADMIN - 管理员专用命令', 'warning');
            this.appendLine('═══════════════════════════════════════════════════════════════', 'warning');
            
            await this.printExchange('USER_LIST');
            await delay(200);
            
            await this.printExchange('SYSTEM_STATUS');
            await delay(200);
            
            await this.printExchange('CACHE_STATS');
            await delay(300);
            
            // ==================== 3. 快照操作 ====================
            this.appendLine('═══════════════════════════════════════════════════════════════', 'warning');
            this.appendLine('【3/6】ADMIN - 快照操作 (Snapshot)', 'warning');
            this.appendLine('═══════════════════════════════════════════════════════════════', 'warning');
            
            await this.printExchange(`BACKUP_CREATE / ${snapshotName}`);
            await delay(200);
            
            await this.printExchange('BACKUP_LIST');
            await delay(200);
            
            // 修改文件后恢复快照
            await this.printExchange(`WRITE ${testDir}/hello.txt Modified_content_after_snapshot`);
            await delay(200);
            
            await this.printExchange(`READ ${testDir}/hello.txt`);
            await delay(200);
            
            await this.printExchange(`BACKUP_RESTORE ${snapshotName}`);
            await delay(200);
            
            await this.printExchange(`READ ${testDir}/hello.txt`);
            await delay(300);
            
            // ==================== 4. AUTHOR 论文上传 ====================
            this.appendLine('═══════════════════════════════════════════════════════════════', 'warning');
            this.appendLine('【4/6】AUTHOR - 论文上传', 'warning');
            this.appendLine('═══════════════════════════════════════════════════════════════', 'warning');
            
            await this.logout(true);
            await this.login('author', 'author123');
            await delay(200);
            
            await this.printExchange(`PAPER_UPLOAD ${paperId} This_is_a_demo_paper_content_for_academic_review_system_testing`);
            await delay(200);
            
            await this.printExchange(`STATUS ${paperId}`);
            await delay(300);

            // 4.1) AUTHOR：二进制文件上传（base64）
            const pdfBinary = "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n1 0 obj\n<<>>\nendobj\ntrailer\n<<>>\n%%EOF\n";
            const pdfB64 = btoa(pdfBinary);
            const docxB64 = btoa("PK\x03\x04" + "demo_docx_payload");
            const rtfB64 = btoa("{\\rtf1\\ansi\nHello}");

            await this.printExchange(
                `PAPER_UPLOAD_FILE_B64 ${filePaperPdf} pdf ${pdfB64}`,
                `PAPER_UPLOAD_FILE_B64 ${filePaperPdf} pdf <base64...>`
            );
            await delay(200);

            await this.printExchange(
                `PAPER_UPLOAD_FILE_B64 ${filePaperDocx} docx ${docxB64}`,
                `PAPER_UPLOAD_FILE_B64 ${filePaperDocx} docx <base64...>`
            );
            await delay(200);

            await this.printExchange(
                `PAPER_UPLOAD_FILE_B64 ${filePaperRtf} rtf ${rtfB64}`,
                `PAPER_UPLOAD_FILE_B64 ${filePaperRtf} rtf <base64...>`
            );
            await delay(300);

            // 用 admin 读取并验证（只输出摘要，不打印二进制）
            await this.logout(true);
            await this.login('admin', 'admin123');
            await delay(200);
            await this.printBinaryRead(`/papers/${filePaperPdf}/current.pdf`, '%PDF-');
            await delay(100);
            await this.printBinaryRead(`/papers/${filePaperDocx}/current.docx`);
            await delay(100);
            await this.printBinaryRead(`/papers/${filePaperRtf}/current.rtf`, '{\\rtf1');
            await delay(300);
            
            // ==================== 5. EDITOR 分配审稿人 ====================
            this.appendLine('═══════════════════════════════════════════════════════════════', 'warning');
            this.appendLine('【5/6】EDITOR - 分配审稿人与最终决定', 'warning');
            this.appendLine('═══════════════════════════════════════════════════════════════', 'warning');
            
            await this.logout(true);
            await this.login('editor', 'editor123');
            await delay(200);
            
            await this.printExchange(`ASSIGN_REVIEWER ${paperId} reviewer`);
            await delay(200);
            
            await this.printExchange(`STATUS ${paperId}`);
            await delay(300);
            
            // ==================== 6. REVIEWER 审稿流程 ====================
            this.appendLine('═══════════════════════════════════════════════════════════════', 'warning');
            this.appendLine('【6/6】REVIEWER - 下载论文并提交审稿意见', 'warning');
            this.appendLine('═══════════════════════════════════════════════════════════════', 'warning');
            
            await this.logout(true);
            await this.login('reviewer', 'reviewer123');
            await delay(200);
            
            await this.printExchange(`PAPER_DOWNLOAD ${paperId}`);
            await delay(200);
            
            await this.printExchange(`REVIEW_SUBMIT ${paperId} Excellent_paper_well_written_recommend_accept`);
            await delay(200);
            
            await this.printExchange(`STATUS ${paperId}`);
            await delay(300);
            
            // ==================== 7. EDITOR 最终决定 ====================
            this.appendLine('═══════════════════════════════════════════════════════════════', 'warning');
            this.appendLine('【7/7】EDITOR - 做出最终决定', 'warning');
            this.appendLine('═══════════════════════════════════════════════════════════════', 'warning');
            
            await this.logout(true);
            await this.login('editor', 'editor123');
            await delay(200);
            
            await this.printExchange(`DECIDE ${paperId} ACCEPT`);
            await delay(200);
            
            await this.printExchange(`STATUS ${paperId}`);
            await delay(300);
            
            // ==================== 8. AUTHOR 查看结果 ====================
            this.appendLine('═══════════════════════════════════════════════════════════════', 'warning');
            this.appendLine('【8/8】AUTHOR - 下载审稿意见', 'warning');
            this.appendLine('═══════════════════════════════════════════════════════════════', 'warning');
            
            await this.logout(true);
            await this.login('author', 'author123');
            await delay(200);
            
            await this.printExchange(`REVIEWS_DOWNLOAD ${paperId}`);
            await delay(300);
            
            // ==================== 完成 ====================
            this.appendLine('', 'info');
            this.appendLine('═══════════════════════════════════════════════════════════════', 'success');
            this.appendLine('                    ✅ 冒烟测试完成！                           ', 'success');
            this.appendLine('═══════════════════════════════════════════════════════════════', 'success');
            this.appendLine(`测试目录: ${testDir}`, 'info');
            this.appendLine(`测试论文: ${paperId}`, 'info');
            this.appendLine(`测试快照: ${snapshotName}`, 'info');
            this.appendLine('', 'info');
            this.appendLine('已展示命令: PWD, MKDIR, WRITE, READ, CD, LS, TREE,', 'info');
            this.appendLine('            USER_LIST, SYSTEM_STATUS, CACHE_STATS,', 'info');
            this.appendLine('            BACKUP_CREATE, BACKUP_LIST, BACKUP_RESTORE,', 'info');
            this.appendLine('            PAPER_UPLOAD, STATUS, ASSIGN_REVIEWER,', 'info');
            this.appendLine('            PAPER_DOWNLOAD, REVIEW_SUBMIT, DECIDE,', 'info');
            this.appendLine('            REVIEWS_DOWNLOAD', 'info');
            this.appendLine('            PAPER_UPLOAD_FILE_B64, READ /papers/<id>/current.<ext>', 'info');
            this.appendLine('', 'info');
            
            // 刷新快照视图
            this.refreshSnapshots();
            
        } catch (err) {
            this.appendLine(`测试出错: ${err.message}`, 'error');
        }
        
        if (btn) {
            btn.disabled = false;
            btn.textContent = '🚀 一键测试所有功能';
        }
    }
    
    async checkConnection() {
        try {
            const response = await fetch('/api/status');
            const data = await response.json();
            this.setConnectionStatus(data.connected);
            this.serverInfo.textContent = `${data.host}:${data.port}`;
        } catch (err) {
            this.setConnectionStatus(false);
        }
    }
    
    setConnectionStatus(connected) {
        if (connected) {
            this.connectionDot.classList.remove('disconnected');
        } else {
            this.connectionDot.classList.add('disconnected');
        }
    }
    
    updateUserCards() {
        document.querySelectorAll('.user-card').forEach(card => {
            const user = card.dataset.user;
            const statusEl = card.querySelector('.user-status');

            // 登录中状态
            if (this.loginInProgressUser && user === this.loginInProgressUser) {
                card.classList.add('pending');
            } else {
                card.classList.remove('pending');
            }
            
            if (user === this.activeUser) {
                card.classList.add('active');
                statusEl?.classList.add('online');
            } else {
                card.classList.remove('active');
                // 单用户模式：非当前用户一律不显示在线
                statusEl?.classList.remove('online');
            }
        });
    }
    
    updatePrompt() {
        const text = this.getPromptText();
        this.inputPrompt.textContent = text;
    }
    
    // 更新当前登录用户显示
    updateCurrentUserDisplay() {
        const section = document.getElementById('currentUserSection');
        const nameEl = document.getElementById('currentUserName');
        const tokenEl = document.getElementById('currentUserToken');
        
        if (section && nameEl && tokenEl) {
            if (this.activeUser && this.activeToken) {
                section.style.display = 'block';
                nameEl.textContent = `👤 ${this.activeUser}`;
                // 截断 token 显示
                const shortToken = this.activeToken.length > 20 
                    ? this.activeToken.substring(0, 20) + '...' 
                    : this.activeToken;
                tokenEl.textContent = `Token: ${shortToken}`;
            } else if (this.loginInProgressUser) {
                section.style.display = 'block';
                nameEl.textContent = `👤 ${this.loginInProgressUser}（登录中...）`;
                tokenEl.textContent = 'Token: -';
            } else {
                section.style.display = 'none';
                nameEl.textContent = '-';
                tokenEl.textContent = 'Token: -';
            }
        }
    }
    
    getPromptText() {
        return this.activeUser ? `[${this.activeUser}]$` : '[未登录]$';
    }
    
    translateRole(role) {
        const roles = {
            'ADMIN': '管理员',
            'EDITOR': '编辑',
            'REVIEWER': '审稿人',
            'AUTHOR': '作者',
            'GUEST': '访客'
        };
        return roles[role] || role;
    }
    
    appendLine(text, type = 'response') {
        const line = document.createElement('div');
        line.className = `terminal-line ${type}`;
        line.textContent = text;
        this.terminalOutput.appendChild(line);
        this.terminalOutput.scrollTop = this.terminalOutput.scrollHeight;
    }
    
    showHelp() {
        const helpText = `
╭────────────────────────────────────────────────────────────╮
│                      命令帮助                               │
├────────────────────────────────────────────────────────────┤
│ 通用命令:                                                  │
│   LOGIN <user> <pass>    登录系统                          │
│   LOGOUT                 登出当前用户                      │
│   HELP                   显示帮助                          │
│   CLEAR                  清空终端                          │
├────────────────────────────────────────────────────────────┤
│ 文件操作:                                                  │
│   LS [path]              列出目录                          │
│   PWD                    显示当前路径                      │
│   CD <path>              切换目录                          │
│   MKDIR <path>           创建目录                          │
│   READ <path>            读取文件                          │
│   WRITE <path> <content> 写入文件                          │
│   TREE [path]            显示目录树                        │
├────────────────────────────────────────────────────────────┤
│ 论文操作 (作者):                                           │
│   PAPER_UPLOAD <id> <content>   上传论文                   │
│   PAPER_UPLOAD_FILE_B64 <id> <ext> <b64> 上传论文文件       │
│   PAPER_REVISE <id> <content>   修改论文                   │
│   STATUS <id>                   查看状态                   │
│   REVIEWS_DOWNLOAD <id>         下载评审                   │
├────────────────────────────────────────────────────────────┤
│ 审稿操作 (审稿人):                                         │
│   PAPER_DOWNLOAD <id>           下载论文                   │
│   REVIEW_SUBMIT <id> <review>   提交评审                   │
├────────────────────────────────────────────────────────────┤
│ 编辑操作 (编辑):                                           │
│   ASSIGN_REVIEWER <id> <user>   分配审稿人                 │
│   DECIDE <id> <ACCEPT|REJECT>   做出决定                   │
├────────────────────────────────────────────────────────────┤
│ 管理操作 (管理员):                                         │
│   USER_LIST                     用户列表                   │
│   USER_ADD <u> <p> <role>       添加用户                   │
│   USER_DEL <username>           删除用户                   │
│   BACKUP_CREATE <path> [name]   创建快照                   │
│   BACKUP_LIST                   快照列表                   │
│   BACKUP_RESTORE <name>         恢复快照                   │
│   SYSTEM_STATUS                 系统状态                   │
│   CACHE_STATS                   缓存统计                   │
│   CACHE_CLEAR                   清空缓存                   │
╰────────────────────────────────────────────────────────────╯`;
        
        helpText.split('\n').forEach(line => {
            this.appendLine(line, 'info');
        });
    }
    
    switchView(viewName) {
        // 更新标签
        document.querySelectorAll('.view-tab').forEach(tab => {
            tab.classList.toggle('active', tab.dataset.view === viewName);
        });
        
        // 切换视图
        document.getElementById('terminalView').classList.toggle('active', viewName === 'terminal');
        document.getElementById('filesystemView').classList.toggle('active', viewName === 'filesystem');
        document.getElementById('snapshotView').classList.toggle('active', viewName === 'snapshot');
        
        // 刷新数据
        if (viewName === 'filesystem') {
            this.refreshFilesystemView();
        } else if (viewName === 'snapshot') {
            this.refreshSnapshots();
        } else if (viewName === 'terminal') {
            this.terminalInput.focus();
        }
    }
    
    async refreshAll() {
        await Promise.all([
            this.refreshFilesystemView(),
            this.refreshSnapshots()
        ]);
    }
    
    async refreshFilesystemView() {
        if (!this.activeToken) return;
        
        try {
            // 获取真实文件系统信息
            const fsInfoReq = fetch('/api/filesystem/info').then(r => r.json());

            const systemReq = fetch('/api/command', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ command: `SYSTEM_STATUS ${this.activeToken}` })
            }).then(r => r.json());

            const cacheReq = fetch('/api/command', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ command: `CACHE_STATS ${this.activeToken}` })
            }).then(r => r.json());

            // 注意：token 应该紧跟命令（TREE <token> <path>）
            const treeReq = fetch('/api/command', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ command: `TREE ${this.activeToken} /` })
            }).then(r => r.json());

            const [fsInfo, systemData, cacheData, treeData] = await Promise.all([fsInfoReq, systemReq, cacheReq, treeReq]);
            
            // 使用真实文件系统信息更新 UI
            this.updateFilesystemStats(fsInfo);
            this.parseSystemStatus(systemData.response);
            this.parseCacheStats(cacheData.response);
            this.renderDirectoryTree(treeData.response);
            
        } catch (err) {
            console.error('刷新文件系统视图失败:', err);
        }
    }

    updateFilesystemStats(fsInfo) {
        // 更新文件系统统计信息
        if (fsInfo.disk_size) {
            const sizeMatch = fsInfo.disk_size.match(/(\d+)\s*(\w+)/);
            if (sizeMatch) {
                document.getElementById('statDiskSize').innerHTML = 
                    `${sizeMatch[1]}<span class="stat-unit">${sizeMatch[2]}</span>`;
            }
        }
        
        if (fsInfo.block_size) {
            const blockMatch = fsInfo.block_size.match(/(\d+)\s*(\w+)/);
            if (blockMatch) {
                document.getElementById('statBlockSize').innerHTML = 
                    `${blockMatch[1]}<span class="stat-unit">${blockMatch[2]}</span>`;
            }
        }
        
        if (fsInfo.total_blocks !== undefined) {
            document.getElementById('statTotalBlocks').textContent = fsInfo.total_blocks;
        }
        
        if (fsInfo.free_blocks !== undefined) {
            document.getElementById('statFreeBlocks').textContent = fsInfo.free_blocks;
        }
        
        if (fsInfo.used_blocks !== undefined) {
            document.getElementById('statUsedBlocks').textContent = fsInfo.used_blocks;
        }
        
        if (fsInfo.free_inodes !== undefined) {
            document.getElementById('statFreeInodes').textContent = fsInfo.free_inodes;
        }
        
        // 更新磁盘使用条
        this.updateDiskUsageBar(fsInfo);
    }

    updateDiskUsageBar(fsInfo) {
        const totalBlocks = fsInfo.total_blocks || 8192;
        const systemBlocks = fsInfo.system_blocks || 123;
        const usedBlocks = fsInfo.used_blocks || 0;
        const freeBlocks = fsInfo.free_blocks || (totalBlocks - systemBlocks - usedBlocks);
        
        const systemPercent = (systemBlocks / totalBlocks * 100).toFixed(1);
        const usedPercent = (usedBlocks / totalBlocks * 100).toFixed(1);
        const freePercent = (100 - parseFloat(systemPercent) - parseFloat(usedPercent)).toFixed(1);
        
        const usageBar = document.getElementById('diskUsageBar');
        if (usageBar) {
            usageBar.innerHTML = `
                <div class="usage-segment system" style="width: ${systemPercent}%">系统</div>
                <div class="usage-segment used" style="width: ${usedPercent}%">${usedPercent > 5 ? '已用' : ''}</div>
                <div class="usage-segment free" style="width: ${freePercent}%">可用</div>
            `;
        }
        
        // 更新底部统计
        const fsTypeIndicator = fsInfo.is_real_fs ? '🟢 真实文件系统' : '🟡 模拟文件系统';
        const infoBar = usageBar?.nextElementSibling;
        if (infoBar) {
            infoBar.innerHTML = `
                <span>📦 系统保留: ${systemBlocks} 块</span>
                <span>${fsTypeIndicator}</span>
                <span>💾 数据块: ${fsInfo.data_blocks || (totalBlocks - systemBlocks)} 块</span>
            `;
        }
    }
    
    parseSystemStatus(response) {
        // 解析系统状态信息
        const match = {
            diskSize: response.match(/磁盘大小[:\s]*(\d+)\s*(MB|KB|GB)/i),
            blockSize: response.match(/块大小[:\s]*(\d+)\s*(KB|B)/i),
            totalBlocks: response.match(/总块数[:\s]*(\d+)/i),
            freeBlocks: response.match(/可用.*块[:\s]*(\d+)/i) || response.match(/空闲块[:\s]*(\d+)/i),
            usedBlocks: response.match(/已用块[:\s]*(\d+)/i),
            freeInodes: response.match(/空闲\s*inode[:\s]*(\d+)/i)
        };
        
        if (match.diskSize) {
            document.getElementById('statDiskSize').innerHTML = 
                `${match.diskSize[1]}<span class="stat-unit">${match.diskSize[2]}</span>`;
        }
        if (match.blockSize) {
            document.getElementById('statBlockSize').innerHTML = 
                `${match.blockSize[1]}<span class="stat-unit">${match.blockSize[2]}</span>`;
        }
        if (match.totalBlocks) {
            document.getElementById('statTotalBlocks').textContent = match.totalBlocks[1];
        }
        if (match.freeBlocks) {
            document.getElementById('statFreeBlocks').textContent = match.freeBlocks[1];
        }
        if (match.usedBlocks) {
            document.getElementById('statUsedBlocks').textContent = match.usedBlocks[1];
        }
        if (match.freeInodes) {
            document.getElementById('statFreeInodes').textContent = match.freeInodes[1];
        }
    }
    
    parseCacheStats(response) {
        // 支持多种格式：
        // 1. block_cache_hits=123 block_cache_misses=456 ...
        // 2. 命中: 123 未命中: 456 ...
        // 3. hits: 123 misses: 456 ...
        const match = {
            hits: response.match(/block_cache_hits[=:\s]*(\d+)/i) || 
                  response.match(/file_cache_hits[=:\s]*(\d+)/i) ||
                  response.match(/命中[:\s]*(\d+)/i) || 
                  response.match(/hits[=:\s]*(\d+)/i),
            misses: response.match(/block_cache_misses[=:\s]*(\d+)/i) || 
                    response.match(/file_cache_misses[=:\s]*(\d+)/i) ||
                    response.match(/未命中[:\s]*(\d+)/i) || 
                    response.match(/misses[=:\s]*(\d+)/i),
            size: response.match(/block_cache_size[=:\s]*(\d+)/i) || 
                  response.match(/file_cache_size[=:\s]*(\d+)/i) ||
                  response.match(/大小[:\s]*(\d+)/i) || 
                  response.match(/size[=:\s]*(\d+)/i),
            capacity: response.match(/block_cache_capacity[=:\s]*(\d+)/i) || 
                      response.match(/file_cache_capacity[=:\s]*(\d+)/i) ||
                      response.match(/容量[:\s]*(\d+)/i) || 
                      response.match(/capacity[=:\s]*(\d+)/i)
        };
        
        const hits = match.hits ? parseInt(match.hits[1]) : 0;
        const misses = match.misses ? parseInt(match.misses[1]) : 0;
        const total = hits + misses;
        const hitRate = total > 0 ? Math.round((hits / total) * 100) : 0;
        
        document.getElementById('cacheHits').textContent = hits;
        document.getElementById('cacheMisses').textContent = misses;
        document.getElementById('cacheHitRate').textContent = `${hitRate}%`;
        
        const size = match.size ? match.size[1] : '--';
        const capacity = match.capacity ? match.capacity[1] : '64';
        document.getElementById('cacheSize').textContent = `${size}/${capacity}`;
    }
    
    renderDirectoryTree(response) {
        const treeContainer = document.getElementById('directoryTree');
        
        if (!response || response.startsWith('ERROR')) {
            treeContainer.innerHTML = '<div style="color: var(--text-muted);">无法加载目录结构</div>';
            return;
        }
        
        // 解析树结构并渲染（保持 ASCII 树缩进，匹配截图）
        const lines = response.split('\n').filter(l => l.trim());
        const nodes = [];

        for (const line of lines) {
            if (line.startsWith('OK:')) continue;

            // 粗略计算层级：根据前缀树符号长度
            const indentMatch = line.match(/^([\s│├└─]*)/);
            let level = 0;
            if (indentMatch) {
                const indent = indentMatch[1];
                level = Math.floor(indent.length / 4);
            }
            level = Math.min(level, 7);

            // 不插入额外图标，完全按后端输出展示（空格/树形符号要保留）
            nodes.push(
                `<div class="tree-node tree-level-${level}"><span class="tree-text">${this.escapeHtml(line)}</span></div>`
            );
        }

        treeContainer.innerHTML = nodes.join('') || '<div style="color: var(--text-muted);">目录为空</div>';
    }
    
    async refreshSnapshots() {
        if (!this.activeToken) return;
        
        try {
            // 使用新的快照 API
            const response = await fetch(`/api/snapshots?token=${encodeURIComponent(this.activeToken)}`);
            const data = await response.json();
            
            if (data.success) {
                this.renderSnapshotsList(data.snapshots);
            } else {
                // 回退到旧方法
                const cmdResponse = await fetch('/api/command', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ command: `BACKUP_LIST ${this.activeToken}` })
                });
                const cmdData = await cmdResponse.json();
                this.renderSnapshots(cmdData.response);
            }
        } catch (err) {
            console.error('刷新快照列表失败:', err);
        }
    }

    renderSnapshotsList(snapshots) {
        const listContainer = document.getElementById('snapshotsList');
        const gridContainer = document.getElementById('snapshotGrid');
        
        if (!snapshots || snapshots.length === 0) {
            listContainer.innerHTML = '<div class="snapshot-item" style="color: var(--text-muted); justify-content: center;">暂无快照</div>';
            gridContainer.innerHTML = `
                <div class="snapshot-card-empty">
                    <div class="empty-icon">📷</div>
                    <div class="empty-text">暂无快照</div>
                    <div class="empty-hint">点击上方按钮创建第一个快照</div>
                </div>
            `;
            return;
        }
        
        // 渲染侧边栏快照列表
        listContainer.innerHTML = snapshots.map(name => `
            <div class="snapshot-item" data-name="${this.escapeHtml(name)}">
                <span class="snapshot-name">📸 ${this.escapeHtml(name)}</span>
                <button class="btn-restore" onclick="window.aps.restoreSnapshot('${this.escapeHtml(name)}')">恢复</button>
            </div>
        `).join('');
        
        // 渲染快照管理卡片网格
        gridContainer.innerHTML = snapshots.map(name => `
            <div class="snapshot-card">
                <div class="snapshot-card-header">
                    <div class="snapshot-card-icon">📸</div>
                    <div>
                        <div class="snapshot-card-title">${this.escapeHtml(name)}</div>
                        <div class="snapshot-card-time">文件系统快照</div>
                    </div>
                </div>
                <div class="snapshot-card-actions">
                    <button class="btn-restore" onclick="window.aps.restoreSnapshot('${this.escapeHtml(name)}')">🔄 恢复此快照</button>
                </div>
            </div>
        `).join('');
    }

    async createSnapshot() {
        if (!this.activeToken) {
            this.appendOutput('warning', '请先登录');
            return;
        }
        
        const name = prompt('请输入快照名称:', `snapshot_${Date.now()}`);
        if (!name) return;
        
        try {
            const response = await fetch('/api/snapshots/create', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ token: this.activeToken, name: name })
            });
            
            const data = await response.json();
            if (data.success) {
                this.appendOutput('success', `✅ 快照 "${name}" 创建成功`);
                this.refreshSnapshots();
            } else {
                this.appendOutput('error', `❌ 创建快照失败: ${data.error || data.response}`);
            }
        } catch (err) {
            this.appendOutput('error', `❌ 创建快照失败: ${err.message}`);
        }
    }

    async restoreSnapshot(name) {
        if (!this.activeToken) {
            this.appendOutput('warning', '请先登录');
            return;
        }
        
        if (!confirm(`确定要恢复到快照 "${name}" 吗？当前数据可能会丢失。`)) {
            return;
        }
        
        try {
            const response = await fetch('/api/snapshots/restore', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ token: this.activeToken, name: name })
            });
            
            const data = await response.json();
            if (data.success) {
                this.appendOutput('success', `✅ 已恢复到快照 "${name}"`);
                // 刷新文件系统视图
                this.refreshFilesystemView();
            } else {
                this.appendOutput('error', `❌ 恢复快照失败: ${data.error || data.response}`);
            }
        } catch (err) {
            this.appendOutput('error', `❌ 恢复快照失败: ${err.message}`);
        }
    }
    
    renderSnapshots(response) {
        const listContainer = document.getElementById('snapshotsList');
        const gridContainer = document.getElementById('snapshotGrid');
        
        if (!response || response.startsWith('ERROR') || response.includes('暂无快照') || response.includes('No snapshots')) {
            listContainer.innerHTML = '<div class="snapshot-item" style="color: var(--text-muted); justify-content: center;">暂无快照</div>';
            gridContainer.innerHTML = '<div style="color: var(--text-muted); grid-column: 1/-1; text-align: center; padding: 40px;">暂无快照，点击上方按钮创建第一个快照</div>';
            return;
        }
        
        // 解析快照列表
        const lines = response.split('\n').filter(l => l.trim() && !l.startsWith('OK'));
        const snapshots = [];
        
        lines.forEach(line => {
            // 尝试解析快照名称和时间
            const match = line.match(/(\S+)\s*(?:\((.+)\))?/) || line.match(/[-\s]*(\S+)/);
            if (match && match[1] && match[1] !== '-') {
                snapshots.push({
                    name: match[1],
                    time: match[2] || '未知时间'
                });
            }
        });
        
        if (snapshots.length === 0) {
            listContainer.innerHTML = '<div class="snapshot-item" style="color: var(--text-muted); justify-content: center;">暂无快照</div>';
            gridContainer.innerHTML = '<div style="color: var(--text-muted); grid-column: 1/-1; text-align: center; padding: 40px;">暂无快照</div>';
            return;
        }
        
        // 渲染侧边栏列表
        listContainer.innerHTML = snapshots.map(s => `
            <div class="snapshot-item" data-name="${this.escapeHtml(s.name)}">
                <span class="snapshot-icon">📸</span>
                <span class="snapshot-name">${this.escapeHtml(s.name)}</span>
            </div>
        `).join('');
        
        // 渲染主视图网格
        gridContainer.innerHTML = snapshots.map(s => `
            <div class="snapshot-card">
                <div class="snapshot-card-header">
                    <div class="snapshot-card-icon">📸</div>
                    <div>
                        <div class="snapshot-card-title">${this.escapeHtml(s.name)}</div>
                        <div class="snapshot-card-time">${this.escapeHtml(s.time)}</div>
                    </div>
                </div>
                <div class="snapshot-card-actions">
                    <button class="btn-restore" onclick="apsConsole.restoreSnapshot('${this.escapeHtml(s.name)}')">
                        🔄 恢复此快照
                    </button>
                </div>
            </div>
        `).join('');
        
        // 绑定侧边栏点击
        listContainer.querySelectorAll('.snapshot-item').forEach(item => {
            item.addEventListener('click', () => {
                const name = item.dataset.name;
                if (confirm(`确定要恢复快照 "${name}" 吗？`)) {
                    this.restoreSnapshot(name);
                }
            });
        });
    }
    
    async restoreSnapshot(name) {
        this.appendLine(`正在恢复快照: ${name}...`, 'info');
        await this.sendCommand(`BACKUP_RESTORE ${name}`);
        this.refreshAll();
    }
    
    refreshIfNeeded() {
        if (this.activeToken) {
            this.refreshSnapshots();
        }
    }
    
    escapeHtml(text) {
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    }
}

// 初始化
const apsConsole = new APSConsole();
