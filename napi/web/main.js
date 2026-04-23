'use strict';

const fs = require('fs');
const path = require('path');
const { app, BrowserWindow, ipcMain, dialog } = require('electron');
const { NativePlayer } = require('../index');

let mainWindow = null;
const player = new NativePlayer();

function toSafeFrame(frame) {
  if (!frame) {
    return null;
  }
  return {
    ptsMs: frame.ptsMs,
    width: frame.width,
    height: frame.height,
    lineY: frame.lineY,
    lineU: frame.lineU,
    lineV: frame.lineV,
    y: frame.y,
    u: frame.u,
    v: frame.v,
  };
}

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1080,
    height: 760,
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });
  mainWindow.loadFile(path.join(__dirname, 'index.html'));
}

app.whenReady().then(() => {
  createWindow();
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
      createWindow();
    }
  });
});

app.on('window-all-closed', () => {
  player.close();
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

ipcMain.handle('video:pickFile', async () => {
  const result = await dialog.showOpenDialog({
    properties: ['openFile'],
    filters: [
      { name: 'Video', extensions: ['mp4', 'mov', 'mkv', 'webm', 'avi'] },
      { name: 'All Files', extensions: ['*'] },
    ],
  });
  if (result.canceled || result.filePaths.length === 0) {
    return null;
  }
  return result.filePaths[0];
});

ipcMain.handle('video:open', async (_, filePath) => {
  const fileBuffer = fs.readFileSync(filePath);
  const info = player.openFromBuffer(fileBuffer);
  return info;
});

ipcMain.handle('video:decode', async (_, ptsMs) => {
  return toSafeFrame(player.decodeAt(Number(ptsMs)));
});

ipcMain.handle('video:seek', async (_, ptsMs) => {
  return toSafeFrame(player.seekTo(Number(ptsMs)));
});

ipcMain.handle('video:holdSeek', async (_, seek) => {
  return player.setHoldSeek(Boolean(seek));
});

ipcMain.handle('video:close', async () => {
  player.close();
  return true;
});
