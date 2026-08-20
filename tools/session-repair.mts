#!/usr/bin/env node
// 会话日志修复工具（deepseek-harness-in-qt 壳内置）
// 诊断/修复 Zstandard JSONL 会话日志的 seq 重复/断裂（崩溃恢复 + 陈旧写入方并发所致）。
//
// 运行方式（由壳发起，cwd = dsh 仓库根，保证 tsx 与 workspace 模块可解析）：
//   node --import tsx/esm <本脚本> --repo <dsh仓库路径> --guid <会话GUID[短前缀]>
//        [--home <DSH_HOME>] [--dir <会话目录绝对路径>] --action diagnose|repair
//
// 复用 dsh 官方模块（语义单一来源，避免双份维护）：
//   - zstd.ts: scanZstdFrames / createZstdFrameDecoder / compressZstdFrame
//   - @deepseek-ai/dsh-session: decodeStorageRecord（与加载器完全一致的事件展开）
//   - format.ts: scanLog（与加载器一致的提交前缀校验，用于修复后验证）
//
// 修复形态（2026-08-20 两个真实案例验证）：
//   A 重复 seq（陈旧写入方游标落后）：重复点之后所有记录 seq/seq0 +1，事件零丢失
//   B 重叠（崩溃恢复方合成 closers 先落盘）：丢弃合成块（tool/result(interrupted)/
//     step/end/turn/end(interrupted)/session/end-seed，恰好覆盖重叠区间且>=2条），
//     保留真实延续，纯拼接无需重编号
//   缺口（seq > 期望）：按 committed 前缀截断（尾部事件引用缺失 seq，不可恢复）
// 安全：修复前备份 <文件>.bak-<时间戳>；前缀帧字节原样保留，只重编码受影响帧；
//       修复后严格走查 + scanLog 双验证，失败自动还原备份；活动会话拒绝修复。

import { createRequire } from 'node:module'
import { existsSync, mkdirSync, readdirSync, readFileSync, renameSync, copyFileSync, writeFileSync } from 'node:fs'
import { homedir } from 'node:os'
import { basename, dirname, join, resolve } from 'node:path'
import { pathToFileURL } from 'node:url'

// ---------- 参数 ----------
function parseArgs(argv) {
  const args = {}
  for (let i = 2; i < argv.length; i++) {
    const a = argv[i]
    if (a.startsWith('--')) args[a.slice(2)] = argv[++i]
  }
  return args
}
const args = parseArgs(process.argv)
const repo = args.repo
const guid = (args.guid ?? '').trim().toLowerCase()
const action = args.action ?? 'diagnose'
const home = args.home || process.env.DSH_HOME || join(homedir(), '.dsh')
const sessionsRoot = join(home, 'sessions')

// ---------- 动态导入 dsh 官方模块 ----------
const zstd = await import(pathToFileURL(join(repo, 'packages/session/session-persistence-jsonl/src/zstd.ts')))
const fmt = await import(pathToFileURL(join(repo, 'packages/session/session-persistence-jsonl/src/format.ts')))
const core = await import(pathToFileURL(join(repo, 'packages/core/session/src/index.ts')))
const { scanZstdFrames, createZstdFrameDecoder, compressZstdFrame } = zstd
const { decodeStorageRecord } = core

// ---------- 小工具 ----------
const out = (line) => console.log(line)
const stamp = () => new Date().toISOString().replace(/[:.]/g, '-').slice(0, 19)
const ERR_PREFIX = '[错误] '
function die(msg) {
  out(ERR_PREFIX + msg)
  process.exit(1)
}

/** 递归收集 session-<guid> 目录（精确匹配优先，其次短前缀匹配） */
function findSessionDirs() {
  if (args.dir) return [resolve(args.dir)]
  if (!existsSync(sessionsRoot)) return []
  const want = `session-${guid}`
  const exact = []
  const prefix = []
  const stack = [sessionsRoot]
  while (stack.length > 0) {
    const dir = stack.pop()
    let entries
    try {
      entries = readdirSync(dir, { withFileTypes: true })
    } catch {
      continue
    }
    for (const e of entries) {
      if (!e.isDirectory()) continue
      const full = join(dir, e.name)
      if (e.name === want) exact.push(full)
      else if (want.length >= 11 && e.name.startsWith(want)) prefix.push(full)
      else stack.push(full)
    }
  }
  return exact.length > 0 ? exact : prefix
}

/** 会话目录内候选日志文件：主文件 + 加载器隔离的 .corrupt-* */
function candidateFiles(dir) {
  const files = []
  for (const name of readdirSync(dir)) {
    if (name === 'session.jsonl.zstd' || name.startsWith('session.jsonl.zstd.corrupt-')) {
      files.push(join(dir, name))
    }
  }
  return files
}

/** 解码整个文件为逐帧明文（torn 尾帧丢弃其不完整行） */
function decodeFrames(file, buffer) {
  const { frames, tornStart } = scanZstdFrames(buffer)
  if (frames.length === 0) die(`${file}: 无完整帧（空或损坏的帧结构）`)
  const decoder = createZstdFrameDecoder()
  const framePlain = []
  for (const plaintext of decoder.decode(buffer, frames)) {
    framePlain.push(Buffer.from(plaintext)) // 解码器复用缓冲区，必须拷贝
  }
  if (tornStart !== undefined) {
    out(`[诊断] ${basename(file)}: 尾部存在不完整帧（tornStart=${tornStart}），其内容将被忽略`)
  }
  return framePlain
}

/**
 * 逐记录走查：返回 { ok, records, events, anomaly }
 * records[i] = { lineNo, frame, text, obj, seqs[], types[], synthetic }
 * anomaly = { kind: 'dup'|'gap'|'bad', recIdx, expected, got } | undefined
 */
function walkRecords(framePlain) {
  const records = []
  let events = 0
  let lineNo = 0
  let expected = 0
  let anomaly
  for (let f = 0; f < framePlain.length; f++) {
    const text = framePlain[f].toString('utf8')
    const lines = text.split('\n')
    // 帧可能以换行结尾：最后一段为空串时忽略
    for (let li = 0; li < lines.length; li++) {
      const line = lines[li]
      if (line === '') continue
      lineNo++
      if (lineNo === 1) continue // 首行 = 头部记录
      let obj
      let evs
      try {
        obj = JSON.parse(line)
        evs = decodeStorageRecord(obj)
      } catch (e) {
        if (anomaly === undefined)
          anomaly = { kind: 'bad', recIdx: records.length, expected, got: null }
        continue // 坏行跳过（与加载器一致：记录 issue 后继续）
      }
      const seqs = evs.map((e) => e.seq)
      const types = evs.map((e) => e.type)
      // 崩溃恢复方合成 closers 的标记（2026-08-20 案例实测）：
      //   tool/result: message.id 前缀 interrupted-tool-result-（合成结果消息）
      //   turn/end:    data.reason.kind === 'interrupted'
      //   session/end-seed: 类型本身
      const synthetic = evs.some(
        (e) =>
          e.type === 'session/end-seed' ||
          (e.type === 'tool/result' &&
            typeof e.data?.message?.id === 'string' &&
            e.data.message.id.startsWith('interrupted-tool-result-')) ||
          (e.type === 'turn/end' && e.data?.reason?.kind === 'interrupted'),
      )
      // step/end 单独算"类合成"（需配合块内其他 interrupted/end-seed 标记才触发形态 B）
      const closerish = synthetic || types.includes('step/end')
      records.push({ recIdx: records.length, lineNo, frame: f, text: line, obj, seqs, types, synthetic, closerish })
      if (anomaly !== undefined) continue
      for (const e of evs) {
        if (e.seq !== expected) {
          anomaly = {
            kind: e.seq < expected ? 'dup' : 'gap',
            recIdx: records.length - 1,
            expected,
            got: e.seq,
          }
          break
        }
        expected++
        events++
      }
    }
  }
  return { ok: anomaly === undefined, records, events, anomaly }
}

/** 定位第一个异常记录所在的帧索引（帧内行偏移忽略——重编码整帧） */
function frameIndexOf(records, recIdx) {
  return records[recIdx].frame
}

/**
 * 重写文件：受影响帧起点之前的字节原样保留，受影响帧起按需重编码。
 * keep(rec) 返回 false 丢弃整条记录；adjust(rec) 返回 {obj} 或 null（丢弃）。
 */
async function rewriteTail(buffer, framePlain, records, fromRecIdx, keep, adjust) {
  const fromFrame = frameIndexOf(records, fromRecIdx)
  const { frames } = scanZstdFrames(buffer)
  const prefixBytes = buffer.subarray(0, frames[fromFrame].start)
  const byFrame = new Map()
  for (let i = 0; i < records.length; i++) {
    const r = records[i]
    if (!byFrame.has(r.frame)) byFrame.set(r.frame, [])
    byFrame.get(r.frame).push(i)
  }
  let outBuf = Buffer.from(prefixBytes)
  for (let f = fromFrame; f < framePlain.length; f++) {
    const body = []
    for (const i of byFrame.get(f) ?? []) {
      if (f === fromFrame && i < fromRecIdx) {
        body.push(records[i].text) // 受影响首帧中异常点之前的行原样保留
        continue
      }
      if (!keep(records[i])) continue
      const adj = adjust(records[i])
      if (adj === null) continue
      body.push(JSON.stringify(adj.obj))
    }
    if (body.length === 0) continue // 空帧不写
    outBuf = Buffer.concat([outBuf, await compressZstdFrame(body.join('\n') + '\n')])
  }
  return outBuf
}

/** 严格走查 + 加载器语义验证 */
function verify(buffer) {
  const framePlain = decodeFrames('<验证>', buffer)
  const w = walkRecords(framePlain)
  if (!w.ok) return { ok: false, reason: `${w.anomaly.kind}@${w.anomaly.recIdx}`, records: w.records.length }
  // 与加载器一致的完整扫描（header + 全部明文）
  const concat = Buffer.concat(framePlain)
  try {
    fmt.scanLog(concat)
  } catch (e) {
    return { ok: false, reason: `scanLog: ${e.message}` }
  }
  return { ok: true, events: w.events }
}

/** 修复一个文件；返回修复后的 Buffer 与说明 */
async function repairFile(buffer) {
  let framePlain = decodeFrames('<修复>', buffer)
  let state = walkRecords(framePlain)
  if (state.ok) return { buffer, note: '无异常', changed: false }
  let iterations = 0
  while (!state.ok && iterations < 32) {
    iterations++
    const a = state.anomaly
    const rec = state.records[a.recIdx]
    let newBuffer
    let note
    if (a.kind === 'bad') {
      // 无法解析的行：丢弃该行（其后重编码）
      newBuffer = await rewriteTail(buffer, framePlain, state.records, a.recIdx, () => true, (r) =>
        r.recIdx === a.recIdx ? null : r,
      )
      note = `丢弃无法解析的记录（行 ${rec.lineNo}）`
    } else if (a.kind === 'gap') {
      // 缺口：保留 committed 前缀，截断尾部
      newBuffer = await rewriteTail(buffer, framePlain, state.records, a.recIdx, () => true, (r) =>
        r.recIdx >= a.recIdx ? null : r,
      )
      note = `seq 缺口（期望 ${a.expected}，实际 ${a.got}）：按 committed 前缀截断，丢弃其后 ${state.records.length - a.recIdx} 条记录`
    } else {
      // dup：候选修复 = 形态 B（丢弃合成 closers 块）+ 形态 A（尾部 seq/seq0 +1），
      // 逐个验证，取首个通过者（形态误判时自动切换）
      const i = a.recIdx
      // 向前收集紧邻的"类合成"块
      let b = i - 1
      while (b >= 0 && state.records[b].closerish) b--
      const blockStart = b + 1
      const block = state.records.slice(blockStart, i)
      const blockSeqStart = block.length > 0 ? block[0].seqs[0] : -1
      const blockSeqEnd = block.length > 0 ? block[block.length - 1].seqs.at(-1) : -1
      const hasMarker = block.some((r) => r.synthetic)
      const blockCovers = blockSeqStart === a.got && blockSeqEnd === a.expected - 1
      const prefixContiguous = blockStart === 0 || state.records[blockStart - 1].seqs.at(-1) === a.got - 1
      const candidates = []
      if (block.length >= 2 && hasMarker && blockCovers && prefixContiguous) {
        const drop = new Set(block.map((r) => r.recIdx))
        candidates.push({
          buf: await rewriteTail(buffer, framePlain, state.records, blockStart, () => true, (r) =>
            drop.has(r.recIdx) ? null : r,
          ),
          note: `形态B：丢弃崩溃恢复方合成 closers ${block.length} 条（行 ${block[0].lineNo}~${block.at(-1).lineNo}），保留真实延续`,
        })
      }
      candidates.push({
        buf: await rewriteTail(buffer, framePlain, state.records, i, () => true, (r) => {
          if (r.recIdx < i) return r
          const o = { ...r.obj }
          if (typeof o.seq === 'number') o.seq = o.seq + 1
          if (typeof o.seq0 === 'number') o.seq0 = o.seq0 + 1
          return { obj: o }
        }),
        note: `形态A：重复点之后 ${state.records.length - i} 条记录 seq/seq0 +1`,
      })
      for (const cand of candidates) {
        const v = verify(cand.buf)
        if (v.ok) {
          newBuffer = cand.buf
          note = `seq 重复（期望 ${a.expected}，实际 ${a.got}）：${cand.note}`
          break
        }
        out(`[修复] 候选被验证拒绝（${v.reason}）：${cand.note}`)
      }
      if (newBuffer === undefined) {
        return { buffer, note: '修复失败：seq 重复的两种候选均未通过验证', changed: false, failed: true }
      }
    }
    const v = verify(newBuffer)
    if (!v.ok) {
      out(`[修复] 第 ${iterations} 轮（${note}）后验证失败：${v.reason}，回退本次修改`)
      return { buffer, note: `修复失败：${note}（验证不通过）`, changed: false, failed: true }
    }
    buffer = newBuffer
    framePlain = decodeFrames('<修复中>', buffer)
    state = walkRecords(framePlain)
    out(`[修复] 第 ${iterations} 轮：${note}（当前事件数 ${state.events}）`)
  }
  return {
    buffer,
    changed: true,
    note: state.ok ? `修复完成：${state.events} 个事件全部连续` : `仍有异常：${state.anomaly.kind}（超过迭代上限）`,
    failed: !state.ok,
  }
}

// ---------- 主流程 ----------
out(`[会话修复] DSH_HOME=${home}  repo=${repo}  action=${action}  guid=${guid}`)
if (!repo || !existsSync(repo)) die('--repo 指向的 dsh 仓库不存在')
if (!args.dir && !guid) die('缺少 --guid（或 --dir）')

const dirs = findSessionDirs()
if (dirs.length === 0) die(`未找到会话目录：sessions/**/session-${guid}`)
if (dirs.length > 1) die(`GUID 匹配到多个会话目录：\n${dirs.join('\n')}\n请输入完整 GUID`)
const dir = dirs[0]
out(`[定位] 会话目录：${dir}`)

const activeId = process.env.DSH_SESSION_ID
const isActive = activeId !== undefined && basename(dir) === activeId

const files = candidateFiles(dir)
if (files.length === 0) die('会话目录内没有 session.jsonl.zstd 或 .corrupt-* 文件')

// ---------- diagnose ----------
for (const file of files) {
  const buffer = readFileSync(file)
  const framePlain = decodeFrames(file, buffer)
  const w = walkRecords(framePlain)
  const mb = (buffer.length / 1048576).toFixed(1)
  if (w.ok) {
    out(`[诊断] ${basename(file)}（${mb}MB，${w.events} 事件）：正常`)
  } else {
    const a = w.anomaly
    const rec = w.records[a.recIdx]
    out(
      `[诊断] ${basename(file)}（${mb}MB，${w.events} 事件）：异常 kind=${a.kind} 行=${rec.lineNo} ` +
        `期望seq=${a.expected} 实际seq=${a.got} 类型=${rec.types.join(',')}`,
    )
  }
}
if (action === 'diagnose') {
  if (isActive) out('[提示] 该会话是当前活动会话（DSH_SESSION_ID），拒绝修复。')
  process.exit(0)
}

// ---------- repair ----------
if (action !== 'repair') die(`未知 action: ${action}`)
if (isActive) die('拒绝修复当前活动会话（DSH_SESSION_ID 匹配），请先切换/结束该会话')

// 选定修复目标：主文件坏则修主文件；否则修最新的 .corrupt-*（隔离的旧日志）
let target = null
for (const file of files) {
  const buffer = readFileSync(file)
  const w = walkRecords(decodeFrames(file, buffer))
  if (!w.ok) {
    target = { file, buffer }
    break
  }
}
if (target === null) {
  out('[修复] 所有候选文件均正常，无需修复。')
  process.exit(0)
}

// 备份（修复前）
const backup = `${target.file}.bak-${stamp()}`
copyFileSync(target.file, backup)
out(`[修复] 已备份：${backup}`)

const result = await repairFile(target.buffer)
if (result.failed || !result.changed) {
  die(result.note)
}
out(`[修复] ${result.note}`)

// 落盘策略：
// - 目标即 session.jsonl.zstd → 原子替换
// - 目标是 .corrupt-* → 写 <目标>.repaired-<ts>；若 session.jsonl.zstd 缺失或空（隔离后新建）→ 同时恢复为主文件
const repairedPath = `${target.file}.repaired-${stamp()}`
writeFileSync(repairedPath, result.buffer)
out(`[修复] 修复结果已写入：${repairedPath}`)

const mainFile = join(dir, 'session.jsonl.zstd')
if (target.file === mainFile) {
  renameSync(repairedPath, mainFile)
  out('[修复] 已替换 session.jsonl.zstd（原文件已备份）')
} else if (!existsSync(mainFile)) {
  copyFileSync(repairedPath, mainFile)
  out('[修复] session.jsonl.zstd 缺失，已恢复为主文件')
} else {
  const mainOk = walkRecords(decodeFrames(mainFile, readFileSync(mainFile))).ok
  if (mainOk) {
    const w0 = walkRecords(decodeFrames(mainFile, readFileSync(mainFile)))
    if (w0.events === 0) {
      copyFileSync(repairedPath, mainFile)
      out('[修复] 现有 session.jsonl.zstd 为空（隔离后新建），已用修复数据恢复为主文件')
    } else {
      out('[修复] 现有 session.jsonl.zstd 含有效事件，未覆盖（避免丢失）；如需恢复旧会话请手动替换')
    }
  }
}
out('[修复] 完成。')
