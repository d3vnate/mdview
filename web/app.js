// SPDX-License-Identifier: MIT
(function () {
  'use strict';

  var contentEl = document.getElementById('content');
  var docEl = document.getElementById('doc');
  var tocEl = document.getElementById('toc');
  var sidebarEl = document.getElementById('sidebar');
  var titleEl = document.getElementById('doc-title');
  var tocObserver = null;

  // ------------------------------------------------------------ messaging

  function post(message) {
    if (window.chrome && window.chrome.webview && window.chrome.webview.postMessage) {
      window.chrome.webview.postMessage(message);
    }
  }

  // ------------------------------------------------------------ helpers

  function escapeHtml(text) {
    return text.replace(/[&<>"']/g, function (ch) {
      switch (ch) {
        case '&': return '&amp;';
        case '<': return '&lt;';
        case '>': return '&gt;';
        case '"': return '&quot;';
        default: return '&#39;';
      }
    });
  }

  // ------------------------------------------------------------ highlighter

  var KEYWORDS = {
    c: 'auto break case char const continue default do double else enum extern float for goto if inline int long register restrict return short signed sizeof static struct switch typedef union unsigned void volatile while',
    cpp: 'alignas alignof asm auto bool break case catch char char16_t char32_t class compl concept const consteval constexpr constinit const_cast continue co_await co_return co_yield decltype default delete do double dynamic_cast else enum explicit export extern false float for friend goto if inline int long mutable namespace new noexcept nullptr operator or private protected public register reinterpret_cast requires return short signed sizeof static static_assert static_cast struct switch template this thread_local throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while',
    csharp: 'abstract as async await base bool break byte case catch char checked class const continue decimal default delegate do double else enum event explicit extern false finally fixed float for foreach goto if implicit in int interface internal is lock long namespace new null object operator out override params private protected public readonly record ref return sbyte sealed short sizeof stackalloc static string struct switch this throw true try typeof uint ulong unchecked unsafe ushort using var virtual void volatile while',
    js: 'async await break case catch class const continue debugger default delete do else export extends false finally for function if import in instanceof let new null of return static super switch this throw true try typeof undefined var void while with yield',
    ts: 'abstract any as async await boolean break case catch class const constructor continue declare default delete do else enum export extends false finally for from function get if implements import in instanceof interface keyof let module namespace never new null number of private protected public readonly return set static string super switch symbol this throw true try type typeof undefined unique unknown var void while yield',
    python: 'False None True and as assert async await break class continue def del elif else except finally for from global if import in is lambda nonlocal not or pass raise return try while with yield match case',
    rust: 'as async await break const continue crate dyn else enum extern false fn for if impl in let loop match mod move mut pub ref return self Self static struct super trait true type unsafe use where while',
    go: 'break case chan const continue default defer else fallthrough for func go goto if import interface map package range return select struct switch type var true false nil',
    json: 'true false null',
    bash: 'if then else elif fi for while do done case esac function in return local export readonly declare unset shift echo cd exit set',
    sql: 'select from where insert into values update set delete create table drop alter add column primary key foreign references join inner left right outer on group by order having limit offset as and or not null is like in between distinct union all exists case when then else end'
  };

  var LANG_ALIASES = {
    'c++': 'cpp', 'c#': 'csharp', 'cs': 'csharp', 'javascript': 'js', 'jsx': 'js',
    'typescript': 'ts', 'tsx': 'ts', 'py': 'python', 'rs': 'rust', 'golang': 'go',
    'sh': 'bash', 'shell': 'bash', 'console': 'bash', 'xml': 'html', 'htm': 'html'
  };

  var KNOWN = {
    c: 1, cpp: 1, csharp: 1, js: 1, ts: 1, python: 1, rust: 1, go: 1,
    json: 1, bash: 1, sql: 1, html: 1, css: 1
  };

  function rulesFor(lang) {
    lang = LANG_ALIASES[lang] || lang;
    if (!KNOWN[lang]) return null;

    var kw = KEYWORDS[lang];
    var keywordRule = kw
      ? { src: '\\b(?:' + kw.split(' ').join('|') + ')\\b', cls: 'key' }
      : null;

    var fnRule = { src: '\\b[A-Za-z_]\\w*(?=\\s*\\()', cls: 'fn' };
    var numRule = { src: '\\b\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?\\b', cls: 'num' };
    var puncRule = { src: '[{}()\\[\\];,.\\-+*\\/%=<>!&|^~?:]+', cls: 'punc' };

    if (lang === 'html') {
      return [
        { src: '<!--[\\s\\S]*?-->', cls: 'com' },
        { src: '<\\/?[A-Za-z][\\w:-]*', cls: 'tag' },
        { src: '"[^"]*"|\'[^\']*\'', cls: 'str' },
        { src: '[\\w:-]+(?==)', cls: 'attr' },
        { src: '>', cls: 'punc' }
      ];
    }

    if (lang === 'css') {
      return [
        { src: '\\/\\*[\\s\\S]*?\\*\\/', cls: 'com' },
        { src: '"[^"]*"|\'[^\']*\'', cls: 'str' },
        { src: '@[\\w-]+', cls: 'key' },
        { src: '[-\\w]+(?=\\s*:)', cls: 'key' },
        { src: '-?\\d+(?:\\.\\d+)?(?:px|em|rem|%|vh|vw|s|ms|deg)?', cls: 'num' },
        { src: '[{}();:,]', cls: 'punc' }
      ];
    }

    var rules = [];

    if (lang === 'python') {
      rules.push({ src: '#[^\\n]*', cls: 'com' });
      rules.push({ src: '"""[\\s\\S]*?"""|\'\'\'[\\s\\S]*?\'\'\'', cls: 'str' });
    } else if (lang === 'bash') {
      rules.push({ src: '#[^\\n]*', cls: 'com' });
    } else if (lang === 'sql') {
      rules.push({ src: '--[^\\n]*', cls: 'com' });
      rules.push({ src: '\\/\\*[\\s\\S]*?\\*\\/', cls: 'com' });
    } else {
      rules.push({ src: '\\/\\/[^\\n]*', cls: 'com' });
      rules.push({ src: '\\/\\*[\\s\\S]*?\\*\\/', cls: 'com' });
    }

    if (lang === 'c' || lang === 'cpp') {
      rules.push({ src: '^[ \\t]*#[ \\t]*[A-Za-z_]\\w*', cls: 'pre' });
    }

    rules.push({ src: '"(?:[^"\\\\\\n]|\\\\.)*"', cls: 'str' });
    rules.push({ src: '\'(?:[^\'\\\\\\n]|\\\\.)*\'', cls: 'str' });
    if (lang === 'js' || lang === 'ts') {
      rules.push({ src: '`(?:[^`\\\\]|\\\\.)*`', cls: 'str' });
    }

    if (lang === 'bash') {
      rules.push({ src: '\\$\\{?[A-Za-z_][A-Za-z0-9_]*\\}?', cls: 'type' });
    }

    rules.push(numRule);
    if (keywordRule) rules.push(keywordRule);
    rules.push(fnRule);
    rules.push(puncRule);
    return rules;
  }

  function highlightCode(text, lang) {
    lang = LANG_ALIASES[lang] || lang;
    var rules = rulesFor(lang);
    if (!rules) return escapeHtml(text);

    var combined = rules.map(function (rule) { return '(' + rule.src + ')'; }).join('|');
    var flags = (lang === 'sql') ? 'gmi' : 'gm';
    var re;
    try {
      re = new RegExp(combined, flags);
    } catch (err) {
      return escapeHtml(text);
    }

    var out = '';
    var last = 0;
    var match;
    while ((match = re.exec(text)) !== null) {
      if (match.index > last) out += escapeHtml(text.slice(last, match.index));
      var cls = 'punc';
      for (var i = 1; i < match.length; i++) {
        if (match[i] !== undefined) {
          cls = rules[i - 1].cls;
          break;
        }
      }
      out += '<span class="tok-' + cls + '">' + escapeHtml(match[0]) + '</span>';
      last = match.index + match[0].length;
      if (match[0].length === 0) re.lastIndex++;
    }
    out += escapeHtml(text.slice(last));
    return out;
  }

  function wrapTables() {
    var tables = docEl.querySelectorAll('table');
    for (var i = 0; i < tables.length; i++) {
      var table = tables[i];
      if (table.parentNode && table.parentNode.classList.contains('table-wrap')) continue;
      var wrap = document.createElement('div');
      wrap.className = 'table-wrap';
      table.parentNode.insertBefore(wrap, table);
      wrap.appendChild(table);
    }
  }

  function highlightAll() {
    var blocks = docEl.querySelectorAll('pre > code');
    for (var i = 0; i < blocks.length; i++) {
      var code = blocks[i];
      if (code.dataset.highlighted) continue;
      var cls = code.className || '';
      var found = /language-([\w+#.-]+)/i.exec(cls);
      if (!found) continue;
      var lang = found[1].toLowerCase();
      if (!rulesFor(lang)) continue;
      code.innerHTML = highlightCode(code.textContent, lang);
      code.dataset.highlighted = '1';
    }
  }

  // ------------------------------------------------------------ outline

  function setActive(link) {
    var links = tocEl.querySelectorAll('a');
    for (var i = 0; i < links.length; i++) {
      links[i].classList.toggle('active', links[i] === link);
    }
  }

  function setupObserver(headings) {
    if (tocObserver) tocObserver.disconnect();
    if (!('IntersectionObserver' in window)) return;

    tocObserver = new IntersectionObserver(function (entries) {
      var best = null;
      entries.forEach(function (entry) {
        if (!entry.isIntersecting) return;
        if (!best || entry.boundingClientRect.top < best.boundingClientRect.top) {
          best = entry;
        }
      });
      if (!best) return;
      var id = best.target.id;
      var links = tocEl.querySelectorAll('a');
      for (var i = 0; i < links.length; i++) {
        links[i].classList.toggle('active', links[i].dataset.target === id);
      }
    }, { root: contentEl, rootMargin: '0px 0px -72% 0px', threshold: 0 });

    headings.forEach(function (heading) { tocObserver.observe(heading); });
  }

  function buildToc() {
    tocEl.innerHTML = '';
    if (tocObserver) {
      tocObserver.disconnect();
      tocObserver = null;
    }

    var headings = docEl.querySelectorAll('h1,h2,h3,h4,h5,h6');
    if (!headings.length) return;

    var fragment = document.createDocumentFragment();
    for (var i = 0; i < headings.length; i++) {
      var heading = headings[i];
      if (!heading.id) heading.id = 'h-' + (i + 1);

      var link = document.createElement('a');
      link.href = '#' + heading.id;
      link.textContent = heading.textContent;
      link.className = 'toc-l' + heading.tagName.charAt(1);
      link.dataset.target = heading.id;
      link.addEventListener('click', (function (target, anchor) {
        return function (event) {
          event.preventDefault();
          target.scrollIntoView({ behavior: 'smooth', block: 'start' });
          setActive(anchor);
        };
      })(heading, link));

      fragment.appendChild(link);
    }
    tocEl.appendChild(fragment);
    setupObserver(headings);
  }

  // ------------------------------------------------------------ sidebar

  function toggleSidebar(force) {
    var collapsed = sidebarEl.classList.contains('collapsed');
    var next = (typeof force === 'boolean') ? force : !collapsed;
    sidebarEl.classList.toggle('collapsed', next);
    try {
      localStorage.setItem('mdview.sidebarCollapsed', next ? '1' : '0');
    } catch (err) { /* ignore */ }
  }

  // ------------------------------------------------------------ states

  function emptyState() {
    return '<div class="empty">' +
      '<svg viewBox="0 0 24 24" aria-hidden="true">' +
      '<path d="M14 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V8z" fill="none" stroke="currentColor" stroke-width="1.5"/>' +
      '<path d="M14 2v6h6M8 13h8M8 17h5" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round"/>' +
      '</svg>' +
      '<h1>Markdown Viewer</h1>' +
      '<p>Open a <code>.md</code> file, or drag one onto this window.</p>' +
      '<button class="button primary" data-action="open">Open file</button>' +
      '</div>';
  }

  // ------------------------------------------------------------ loading

  function loadContent() {
    var scrollTop = contentEl.scrollTop;

    fetch('/content?ts=' + Date.now(), { cache: 'no-store' })
      .then(function (response) {
        if (!response.ok) throw new Error('HTTP ' + response.status);
        return response.text();
      })
      .then(function (html) {
        if (!html.trim()) {
          docEl.innerHTML = emptyState();
          tocEl.innerHTML = '';
          if (tocObserver) {
            tocObserver.disconnect();
            tocObserver = null;
          }
          titleEl.textContent = 'Markdown Viewer';
          contentEl.scrollTop = 0;
          return;
        }

        docEl.innerHTML = html;

        var title = 'Markdown Viewer';
        var template = document.getElementById('md-title');
        if (template) {
          var text = template.content ? template.content.textContent : template.textContent;
          if (text && text.trim()) title = text.trim();
          template.remove();
        }
        titleEl.textContent = title;

        wrapTables();
        highlightAll();
        buildToc();

        contentEl.scrollTop = scrollTop;
      })
      .catch(function (err) {
        docEl.innerHTML = '<div class="error"><h2>Failed to load document</h2><p>' +
          escapeHtml(String(err)) + '</p></div>';
        tocEl.innerHTML = '';
      });
  }

  // ------------------------------------------------------------ interactions

  document.addEventListener('click', function (event) {
    if (!event.target || !event.target.closest) return;
    var actionEl = event.target.closest('[data-action]');
    if (actionEl) {
      if (actionEl.getAttribute('data-action') === 'open') {
        event.preventDefault();
        post({ type: 'openDialog' });
        return;
      }
    }

    var anchor = event.target.closest('a');
    if (!anchor) return;
    var href = anchor.getAttribute('href');
    if (!href) return;

    if (href.charAt(0) === '#') {
      event.preventDefault();
      var id = href.slice(1);
      var target = docEl.querySelector('#' + (window.CSS && CSS.escape ? CSS.escape(id) : id));
      if (target) target.scrollIntoView({ behavior: 'smooth', block: 'start' });
      return;
    }

    if (/^(https?:|mailto:)/i.test(href)) {
      event.preventDefault();
      post({ type: 'external', url: href });
      return;
    }

    if (href.indexOf('https://doc.local/') === 0) {
      event.preventDefault();
      var clean = href.split('#')[0].split('?')[0];
      if (/\.(md|markdown|mdown|mkd|mdx)$/i.test(clean)) {
        post({ type: 'open', url: href });
      } else {
        post({ type: 'openLocal', url: href });
      }
      return;
    }

    if (/^file:/i.test(href)) {
      event.preventDefault();
      post({ type: 'openLocal', url: href });
    }
  });

  // ------------------------------------------------------------ drag & drop

  document.addEventListener('dragenter', function (event) {
    event.preventDefault();
    if (event.dataTransfer) event.dataTransfer.dropEffect = 'copy';
  });

  document.addEventListener('dragover', function (event) {
    event.preventDefault();
    if (event.dataTransfer) event.dataTransfer.dropEffect = 'copy';
  });

  document.addEventListener('drop', function (event) {
    event.preventDefault();
    if (!event.dataTransfer) return;

    var uriList = '';
    try {
      uriList = event.dataTransfer.getData('text/uri-list');
    } catch (err) {
      uriList = '';
    }

    var lines = uriList.split(/\r?\n/);
    for (var i = 0; i < lines.length; i++) {
      var line = lines[i].trim();
      if (line && line.charAt(0) !== '#') {
        post({ type: 'drop', url: line });
        return;
      }
    }

    // Some WebView2/OS combinations hide the URI list. Read the file content
    // instead so the drop still displays something (no path, so no live reload).
    var files = event.dataTransfer.files;
    if (files && files.length > 0 && typeof files[0].text === 'function') {
      var file = files[0];
      file.text().then(function (text) {
        post({ type: 'dropContent', name: file.name, content: text });
      }).catch(function () { /* ignore */ });
    }
  });

  document.getElementById('btn-open').addEventListener('click', function () {
    post({ type: 'openDialog' });
  });

  document.getElementById('btn-reload').addEventListener('click', function () {
    post({ type: 'reload' });
  });

  document.getElementById('btn-toc').addEventListener('click', function () {
    toggleSidebar();
  });

  // ------------------------------------------------------------ startup

  try {
    if (localStorage.getItem('mdview.sidebarCollapsed') === '1') {
      sidebarEl.classList.add('collapsed');
    }
  } catch (err) { /* ignore */ }

  window.__reload = loadContent;
  window.__toggleSidebar = function () { toggleSidebar(); };

  loadContent();
})();
