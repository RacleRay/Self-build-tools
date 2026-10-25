#!/usr/bin/env python3
"""微信公众号文章下载

支持两种链接：
  1. 单篇文章: https://mp.weixin.qq.com/s/xxxx
  2. 合集文章: https://mp.weixin.qq.com/mp/appmsgalbum?__biz=...&album_id=...

用法:
  python wechat_download.py <链接> [-o 输出目录]
  python wechat_download.py <文章链接> [更多链接...] -o downloads
"""

from __future__ import annotations

import argparse
import re
import time
from pathlib import Path
from urllib.parse import urlparse, parse_qs

import requests
from bs4 import BeautifulSoup

UA = ("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36 MicroMessenger")
HEADERS = {"User-Agent": UA}

session = requests.Session()
session.headers.update(HEADERS)


def sanitize(name: str, maxlen: int = 80) -> str:
    for ch in '\\/:*?"<>|\n\r\t':
        name = name.replace(ch, " ")
    return name.strip()[:maxlen] or "untitled"


def extract_article_urls(url: str) -> list[str]:
    parsed = urlparse(url)
    qs = parse_qs(parsed.query)
    if "appmsgalbum" in parsed.path:
        return fetch_album(url, qs)
    if "mp.weixin.qq.com" in parsed.netloc:
        return [url]
    raise ValueError(f"无法识别的链接: {url}")


def parse_album_page(html: str, seen: set[str], urls: list[str]):
    """解析合集页面中的 <li data-link=...> 条目，返回 (最后msgid, continue_flag)。"""
    soup = BeautifulSoup(html, "html.parser")
    last_msgid = ""
    for li in soup.select("li.js_album_item[data-link]"):
        link = li["data-link"].replace("&amp;", "&")
        msgid = li.get("data-msgid", "")
        last_msgid = msgid or last_msgid
        if link not in seen:
            seen.add(link)
            urls.append(link)
            title = li.get("data-title", "") or sanitize(Path(link).stem)
            print(f"  [{len(urls)}] {title}")
    m = re.search(r"continue_flag:\s*'(\d)'", html)
    return last_msgid, (m.group(1) == "1" if m else False)


def fetch_album(qs: dict) -> list[str]:
    """抓取合集全部文章链接"""
    urls: list[str] = []
    seen: set[str] = set()
    params = {k: v[0] for k, v in qs.items()}
    params.setdefault("action", "getalbum")
    params.setdefault("album_id", "")
    begin_msgid = params.pop("from_msgid", [""])[0]
    params["count"] = "10"
    for page in range(200):  # 防御性上限
        if page > 0:
            params["begin_msgid"] = begin_msgid
            params["begin_itemidx"] = "1"
            params["is_reverse"] = "0"
        before = len(urls)
        r = session.get("https://mp.weixin.qq.com/mp/appmsgalbum",
                        params=params, timeout=30)
        r.raise_for_status()
        begin_msgid, has_more = parse_album_page(r.text, seen, urls)
        new = len(urls) - before
        print(f"  -- 第{page + 1}页: 新增 {new} 篇")
        if new == 0 and page > 0 or not has_more:
            break
        time.sleep(0.5)
    print(f"合集共获取 {len(urls)} 篇文章")
    return urls


def parse_article(url: str) -> dict:
    r = session.get(url, timeout=30)
    r.raise_for_status()
    r.encoding = "utf-8"
    html = r.text
    soup = BeautifulSoup(html, "html.parser")

    def find(pattern: str) -> str:
        m = re.search(pattern, html)
        return m.group(1).strip() if m else ""

    title = find(r'var\s+msg_title\s*=\s*[\'"](.+?)[\'"]') or \
        (soup.title.string.strip() if soup.title else "untitled")
    publish_time = find(r'var\s+createTime\s*=\s*[\'"](.+?)[\'"]') or \
        find(r'"publish_time"\s*:\s*"([^"]+)"') or \
        find(r"var oriCreateTime = '(\d+)'")
    if publish_time.isdigit():
        publish_time = time.strftime("%Y-%m-%d", time.localtime(int(publish_time)))
    author = find(r'var\s+nickname\s*=\s*[\'"](.+?)[\'"]') or find(r'"nick_name"\s*:\s*"([^"]*)"')

    content = soup.find(id="js_content") or soup.find(id="page-content")
    return {"url": url, "title": title, "time": publish_time,
            "author": author, "content": content}


def save_article(article: dict, outdir: Path, save_md: bool) -> Path:
    name = sanitize(article["title"])
    folder = outdir / name
    folder.mkdir(parents=True, exist_ok=True)

    content = article["content"]
    if content is None:
        print(f"  !! 未找到正文（付费/被删文章？）: {article['title']}")
        return folder

    assets = folder / "assets"
    assets.mkdir(exist_ok=True)

    for i, img in enumerate(content.find_all("img"), 1):
        src = img.get("data-src") or img.get("src") or ""
        if not src.startswith(("http://", "https://")):
            img.decompose()
            continue
        src = src.replace("http://", "https://")
        ext = ".jpg"
        m = re.search(r"wx_fmt=(\w+)", src)
        if m:
            ext = "." + {"jpeg": "jpg"}.get(m.group(1), m.group(1))
        fname = f"{i:03d}{ext}"
        try:
            ir = session.get(src, timeout=30)
            ir.raise_for_status()
            (assets / fname).write_bytes(ir.content)
            img["src"] = f"assets/{fname}"
            img.attrs = {k: v for k, v in img.attrs.items() if k in ("src", "alt")}
        except requests.RequestException as e:
            print(f"  !! 图片下载失败: {e}")
        time.sleep(0.2)

    html = (f'<html><head><meta charset="utf-8"><title>{article["title"]}</title></head>'
            f'<body><h1>{article["title"]}</h1>'
            f'<p>{article["author"]} | {article["time"]} | '
            f'<a href="{article["url"]}">原文</a></p>{str(content)}</body></html>')
    html_path = folder / f"{name}.html"
    html_path.write_text(html, encoding="utf-8")

    if save_md:
        md = to_markdown(content)
        md = (f"# {article['title']}\n\n"
              f"> {article['author']} | {article['time']} | "
              f"[原文]({article['url']})\n\n{md}")
        (folder / f"{name}.md").write_text(md, encoding="utf-8")

    return folder


def to_markdown(content) -> str:
    """simple HTML -> Markdown。"""
    from bs4 import NavigableString, Tag

    def convert(node: Tag, list_depth: int = 0) -> str:
        out = []
        for child in node.children:
            if isinstance(child, NavigableString):
                out.append(str(child))
            elif isinstance(child, Tag):
                name = child.name
                if name in ("h1", "h2", "h3", "h4"):
                    out.append("\n" + "#" * int(name[1]) + " " +
                               child.get_text(strip=True) + "\n")
                elif name == "p" or name == "section":
                    out.append(convert(child, list_depth))
                elif name == "br":
                    out.append("\n")
                elif name in ("strong", "b"):
                    t = child.get_text(strip=True)
                    out.append(f"**{t}**" if t else "")
                elif name in ("em", "i"):
                    t = child.get_text(strip=True)
                    out.append(f"*{t}*" if t else "")
                elif name == "blockquote":
                    t = child.get_text("\n", strip=True)
                    out.append("\n" + "\n".join("> " + line for line in t.splitlines()) + "\n")
                elif name in ("ol", "ul"):
                    out.append(convert_list(child, name == "ol", list_depth))
                elif name == "img":
                    out.append(f"![image]({child.get('src', '')})")
                elif name == "a":
                    out.append(f"[{child.get_text(strip=True)}]({child.get('href', '')})")
                else:
                    out.append(convert(child, list_depth))
        return "".join(out)

    def convert_list(node: Tag, ordered: bool, depth: int) -> str:
        lines, idx = [], 0
        prefix = "  " * depth
        for li in node.find_all("li", recursive=False):
            idx += 1
            text = li.get_text("\n", strip=True)
            bullet = f"{idx}. " if ordered else "- "
            first, *rest = text.splitlines() or [""]
            lines.append(f"{prefix}{bullet}{first}")
            for r in rest:
                lines.append(f"{prefix}  {r}")
        return "\n" + "\n".join(lines) + "\n"

    md = convert(content)
    return re.sub(r"\n{3,}", "\n\n", md).strip() + "\n"


def main():
    ap = argparse.ArgumentParser(description="微信公众号文章/合集下载器")
    ap.add_argument("urls", nargs="+", help="文章链接或合集链接（可多个）")
    ap.add_argument("-o", "--output", default="downloads", help="输出目录（默认 downloads）")
    ap.add_argument("--no-md", action="store_true", help="不生成 Markdown，仅 HTML")
    args = ap.parse_args()

    outdir = Path(args.output)
    outdir.mkdir(parents=True, exist_ok=True)

    for link in args.urls:
        print(f"\n=== 处理: {link[:100]}{'...' if len(link) > 100 else ''}")
        try:
            article_urls = extract_article_urls(link)
        except Exception as e:
            print(f"  !! 失败: {e}")
            continue
        for i, u in enumerate(article_urls, 1):
            if len(article_urls) > 1:
                print(f"({i}/{len(article_urls)})")
            try:
                article = parse_article(u)
                path = save_article(article, outdir, not args.no_md)
                print(f"  已保存: {path}")
            except Exception as e:
                print(f"  !! 下载失败 {u[:80]}: {e}")
            time.sleep(0.5)


if __name__ == "__main__":
    main()

