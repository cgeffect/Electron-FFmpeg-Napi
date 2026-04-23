import { FFWasmPlayer } from './player'
import type { IVideoInfo } from './types'

function formatDuration(durationMs: number) {
  const totalSeconds = Math.max(0, Math.floor(durationMs / 1000))
  const minutes = Math.floor(totalSeconds / 60)
  const seconds = totalSeconds % 60
  return `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}`
}

function renderVideoMeta(fileName: string, info: IVideoInfo | null) {
  const metaEl = document.getElementById('video-meta')
  if (!metaEl)
    return
  if (!info) {
    metaEl.textContent = `当前文件: ${fileName} | 读取视频信息失败`
    return
  }
  metaEl.textContent = `当前文件: ${fileName} | ${info.width}x${info.height} | 时长 ${formatDuration(info.duration)} | ${info.fps.toFixed(2)} fps`
}

function renderStatus(text: string, type: 'normal' | 'loading' | 'error' = 'normal') {
  const statusEl = document.getElementById('video-status')
  if (!statusEl)
    return
  statusEl.textContent = text
  statusEl.style.color = type === 'error' ? '#ff6b6b' : type === 'loading' ? '#ffd166' : ''
}

;(async () => {
  try {
    const player = new FFWasmPlayer()
    const defaultSource = '/11.mp4'
    renderStatus('正在加载默认素材...', 'loading')
    await player.init(defaultSource)
    renderVideoMeta(defaultSource, player.getVideoInfo())
    renderStatus('加载完成')

    const fileInput = document.getElementById('video-file') as HTMLInputElement | null
    fileInput?.addEventListener('change', async (event) => {
      const target = event.target as HTMLInputElement
      const file = target.files?.[0]
      if (!file)
        return
      try {
        renderStatus(`正在加载本地文件: ${file.name}`, 'loading')
        await player.loadFromFile(file)
        renderVideoMeta(file.name, player.getVideoInfo())
        renderStatus('加载完成')
      }
      catch (error) {
        console.error('[ffwasm] load local file failed', error)
        renderStatus(`加载失败: ${file.name}`, 'error')
      }
    })
  }
  catch (error) {
    console.error('[ffwasm] init failed', error)
    renderStatus('初始化失败，请检查 wasm 和视频资源', 'error')
  }
})()
