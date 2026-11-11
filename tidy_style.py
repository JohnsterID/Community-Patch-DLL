#!/usr/bin/env python3
"""Rewrite the fix-its of two layout checks into the code base's brace style
(Allman braces, tab indentation), or drop the fix when that can't be done
safely. clang-tidy's own fix-its only insert or delete tokens:

readability-inconsistent-ifelse-braces
    inserts " {" after the condition / else, and "\\n}" after the statement or
    "} " before the next else. Rewritten: "{" and "}" on their own lines at the
    if/else line's indentation, after a trailing comment if there is one.

readability-redundant-nested-if
    replaces the outer condition with "(A) && (B)" and deletes the inner
    "if (B)" and the braces in between. Rewritten as one replacement of the
    whole statement: the emptied lines are removed and the body is moved one
    tab left per merged level.

Fixes are dropped (with a reason) when a comment or code shares a line that
the rewrite would move or delete, when a body starts on the if's own line, or
when the text around the fix isn't what the fix expects.
"""
import re

BRACES = "readability-inconsistent-ifelse-braces"
NESTED_IF = "readability-redundant-nested-if"
CHECKS = (BRACES, NESTED_IF)

_WS = b" \t"


class StyleError(Exception):
    pass


def _line_start(data, pos):
    return data.rfind(b"\n", 0, pos) + 1


def _line_end(data, pos):
    """Offset of the line break (\\r\\n or \\n) ending the line holding pos."""
    e = data.find(b"\n", pos)
    if e < 0:
        return len(data)
    return e - 1 if e > 0 and data[e - 1:e] == b"\r" else e


def _indent(data, pos):
    s = _line_start(data, pos)
    i = s
    while i < len(data) and data[i:i + 1] in (b" ", b"\t"):
        i += 1
    return data[s:i]


def _rest_is_blank_or_comment(rest):
    r = rest.strip(_WS)
    return r == b"" or r.startswith(b"//")


def _newline(data):
    return b"\r\n" if b"\r\n" in data else b"\n"


def _match_paren_forward(data, open_pos):
    """Index of the ")" matching the "(" at open_pos, skipping string and char
    literals and comments; -1 if not found."""
    depth = 0
    i = open_pos
    n = len(data)
    while i < n:
        c = data[i:i + 1]
        if c in (b'"', b"'"):
            q = c
            i += 1
            while i < n and data[i:i + 1] != q:
                i += 2 if data[i:i + 1] == b"\\" else 1
        elif data[i:i + 2] == b"//":
            e = data.find(b"\n", i)
            i = n if e < 0 else e
        elif data[i:i + 2] == b"/*":
            e = data.find(b"*/", i + 2)
            if e < 0:
                return -1
            i = e + 1
        elif c == b"(":
            depth += 1
        elif c == b")":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


_IF_OPEN = re.compile(rb"\bif\s*\(")


def _keyword_line_indent(data, offset):
    """Indentation for a brace opened at offset (right after an if condition or
    an else). The keyword's line may hold only whitespace, "}" or "else" before it."""
    j = offset
    while j > 0 and data[j - 1:j] in (b" ", b"\t"):
        j -= 1
    kw = -1
    if data[j - 1:j] == b")":
        # The last "if (" above whose condition closes right here (conditions can
        # span lines and hold strings with parentheses).
        window = max(0, j - 4000)
        for m in _IF_OPEN.finditer(data, window, j):
            if _match_paren_forward(data, m.end() - 1) == j - 1:
                kw = m.start()
        if kw < 0:
            raise StyleError("unbalanced condition")
    elif data[j - 4:j] == b"else":
        kw = j - 4
    else:
        raise StyleError("brace not after a condition or else")
    before = data[_line_start(data, kw):kw].strip(_WS)
    if before not in (b"", b"}", b"else", b"} else"):
        raise StyleError("code before the if/else on its line")
    return _indent(data, kw)


def restyle_braces(data, repls):
    """Return new [(offset, length, text)] for one diagnostic's replacements."""
    nl = _newline(data)
    out = []
    stack = []
    for off, length, text in sorted(repls):
        if length != 0:
            raise StyleError("unexpected non-insertion")
        if text == " {":
            ind = _keyword_line_indent(data, off)
            rest = data[off:_line_end(data, off)]
            if not _rest_is_blank_or_comment(rest):
                raise StyleError("body on the if/else line")
            eol = _line_end(data, off)
            out.append((eol, 0, nl + ind + b"{"))
            stack.append(ind)
        elif text == "\n}":
            if not stack:
                raise StyleError("closing brace without opening")
            ind = stack.pop()
            rest = data[off:_line_end(data, off)]
            if not _rest_is_blank_or_comment(rest):
                raise StyleError("code after the statement on its line")
            eol = _line_end(data, off)
            out.append((eol, 0, nl + ind + b"}"))
        elif text == "} ":
            if not stack:
                raise StyleError("closing brace without opening")
            ind = stack.pop()
            prefix = data[_line_start(data, off):off]
            if prefix.strip(_WS) != b"" or data[off:off + 4] != b"else":
                raise StyleError("else not on its own line")
            if prefix != ind:
                raise StyleError("else indented differently from its if")
            # Close the previous branch on the line before the else.
            ls = _line_start(data, off)
            prev_eol = ls - len(nl) if data[ls - len(nl):ls] == nl else ls - 1
            out.append((prev_eol, 0, nl + ind + b"}"))
        else:
            raise StyleError("unexpected fix text %r" % text)
    if stack:
        raise StyleError("opening brace without closing")
    return out


def _dedent(line, n):
    if not line.strip(_WS):
        return line
    if line[:n] != b"\t" * n:
        raise StyleError("body not indented with tabs")
    return line[n:]


def _iter_lines(data, start, end):
    """(line start, line bytes without the line break) for the lines from start to end."""
    pos = start
    while pos <= end:
        le = _line_end(data, pos)
        yield pos, data[pos:le]
        nxt = data.find(b"\n", pos)
        if nxt < 0 or nxt + 1 > end:
            break
        pos = nxt + 1


_IF_TOKEN = re.compile(rb"if\s*\(")


def restyle_nested_if(data, repls):
    """One replacement covering the whole merged statement.

    clang-tidy's deletions may include the line break and indentation around a
    token; each is trimmed to its token ("{", "}" or the inner "if (...)") and
    the statement is rebuilt line by line from the original text."""
    nl = _newline(data)
    repls = sorted(repls)
    cond = repls[0]
    dels = repls[1:]
    if cond[2] == "" or not dels or any(t != "" for _, _, t in dels):
        raise StyleError("unexpected fix shape")
    cond_end = cond[0] + cond[1]
    core = []
    for off, length, _ in dels:
        chunk = data[off:off + length]
        lead = len(chunk) - len(chunk.lstrip(b" \t\r\n"))
        keep = len(chunk.rstrip(b" \t\r\n"))
        if keep <= lead:
            raise StyleError("unexpected fix shape")
        core.append((off + lead, keep - lead))
    kinds = []
    n_open = n_if = 0
    for off, length in core:
        chunk = data[off:off + length]
        if chunk == b"{":
            kinds.append("open")
            n_open += 1
        elif chunk == b"}":
            kinds.append("close")
        elif _IF_TOKEN.match(chunk):
            close = _match_paren_forward(data, off + chunk.index(b"("))
            if close < 0 or close >= off + length:
                raise StyleError("unexpected fix shape")
            if b"\n" in data[off:close]:
                raise StyleError("multi-line inner condition")
            rest = data[close + 1:off + length].strip(b" \t\r\n")
            if rest == b"{":
                n_open += 1  # a middle level: its "if (...)" and "{" go in one deletion
            elif rest:
                raise StyleError("deleted text is not a brace or an if")
            kinds.append("if")
            n_if += 1
        else:
            raise StyleError("deleted text is not a brace or an if")
    heads = [c for c, k in zip(core, kinds) if k != "close"]
    closes = [c for c, k in zip(core, kinds) if k == "close"]
    levels = n_if
    if levels == 0 or n_open != len(closes):
        raise StyleError("unexpected fix shape")
    if closes and min(c[0] for c in closes) < max(h[0] for h in heads):
        raise StyleError("unexpected fix shape")
    # Only whitespace between the condition and the pieces that go away.
    prev = cond_end
    while data[prev:prev + 1] in (b" ", b"\t", b"\r", b"\n"):
        prev += 1
    if data[prev:prev + 1] != b")":
        raise StyleError("unexpected fix shape")
    prev += 1
    for off, length in heads:
        if data[prev:off].strip(b" \t\r\n"):
            raise StyleError("comment or code between the nested ifs")
        prev = off + length
    for (a_off, _), (b_off, _) in zip(closes, closes[1:]):
        if data[a_off + 1:b_off].strip(b" \t\r\n"):
            raise StyleError("comment or code between the closing braces")
    try:
        cond_text = cond[2].encode("utf-8")
        data[_line_start(data, cond[0]):_line_end(data, max(o + n for o, n in core))].decode("utf-8")
    except UnicodeError:
        raise StyleError("text is not UTF-8")
    region_start = _line_start(data, cond[0])
    head_line = _line_start(data, cond_end)
    region_end = _line_end(data, max(o + n for o, n in core))
    # Group the deleted bytes by line; a line that keeps only whitespace goes away.
    on_line = {}
    for off, length in core:
        for ls, line in _iter_lines(data, _line_start(data, off), _line_end(data, off + length)):
            a = max(off, ls) - ls
            b = min(off + length, ls + len(line)) - ls
            if b > a:
                on_line.setdefault(ls, []).append((ls + a, b - a))

    def strip_deletions(ls, line):
        out = line
        for off, length in sorted(on_line.get(ls, []), reverse=True):
            out = out[:off - ls] + out[off - ls + length:]
        return out
    body_from = _line_end(data, heads[-1][0] + heads[-1][1]) + len(nl)
    body_to = _line_start(data, closes[0][0]) if closes else region_end + len(nl)
    out = []
    for ls, line in _iter_lines(data, region_start, region_end):
        if ls < head_line:
            continue
        if ls == head_line:
            le = _line_end(data, cond_end)
            tail = strip_deletions(ls, data[ls:le])[cond_end - ls:] if ls in on_line else data[cond_end:le]
            out.append((data[region_start:cond[0]] + cond_text + tail).rstrip(_WS))
            continue
        if ls in on_line:
            rest = strip_deletions(ls, line)
            if rest.strip(_WS):
                raise StyleError("comment or code on a deleted line")
            continue
        if body_from <= ls < body_to:
            out.append(_dedent(line, levels))
        else:
            out.append(line)
    return [(region_start, region_end - region_start, nl.join(out))]


def restyle(diags, cache, replacement_cls):
    """Rewrite the fixes of CHECKS in place. Returns (kept, dropped[(diag, reason)])."""
    kept, dropped = [], []
    for d in diags:
        if d.check not in CHECKS or not d.replacements:
            kept.append(d)
            continue
        paths = {r.path for r in d.replacements}
        if len(paths) != 1:
            dropped.append((d, "style: fix spans files"))
            continue
        path = d.replacements[0].path
        data = cache.get(path)
        repls = [(r.offset, r.length, r.text) for r in d.replacements]
        try:
            new = restyle_braces(data, repls) if d.check == BRACES else restyle_nested_if(data, repls)
        except StyleError as e:
            dropped.append((d, "style: %s" % e))
            continue
        d.replacements = [replacement_cls(path, o, n, t.decode("utf-8") if isinstance(t, bytes) else t)
                          for o, n, t in new]
        kept.append(d)
    return kept, dropped
